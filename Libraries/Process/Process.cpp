// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Process.h"
#include "../File/File.h"

#include "Internal/EnvironmentTable.h"
#include "Internal/StringsArena.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/ProcessWindows.inl"
#else
#include "Internal/ProcessPosix.inl" // IWYU pragma: keep
#endif

//-------------------------------------------------------------------------------------------------------
// ProcessChain
//-------------------------------------------------------------------------------------------------------

SC::Result SC::ProcessChain::launch(const Process::StdOut& stdOut, const Process::StdIn& stdIn,
                                    const Process::StdErr& stdErr)
{
    if (processes.isEmpty())
    {
        return Result::Error("ProcessChain::launch - No Processes");
    }
    for (Process* process = processes.front; process != nullptr; process = process->next)
    {
        process->options = options;
        if (process == processes.front)
        {
            if (process == processes.back)
            {
                // single item in the list
                SC_TRY(process->launch(stdOut, stdIn, stdErr));
            }
            else
            {
                // stdout and stderr are chained from previous process
                SC_TRY(process->launch(Process::StdOut::AlreadySetup(), stdIn, Process::StdErr::AlreadySetup()));
            }
        }
        else if (process == processes.back)
        {
            // Gets stdin from previous process in the chain
            SC_TRY(process->launch(stdOut, Process::StdIn::AlreadySetup(), stdErr));
        }
        else
        {
            // Use directly launchImplementation as we've already setup redirection manually
            SC_TRY(process->launchImplementation());
        }
    }
    return Result(true);
}

SC::Result SC::ProcessChain::pipe(Process& process, const Span<const StringSpan> cmd)
{
    // TODO: Expose options to decide if to pipe also stderr
    SC_TRY_MSG(process.parent == nullptr, "Process::pipe - already in use");

    if (not processes.isEmpty())
    {
        PipeDescriptor chainPipe;
        PipeOptions    chainOptions;
        chainOptions.writeInheritable = true;
        chainOptions.readInheritable  = true;
        SC_TRY(chainPipe.createPipe(chainOptions));
        SC_TRY(processes.back->stdOutFd.assign(move(chainPipe.writePipe)));
        SC_TRY(process.stdInFd.assign(move(chainPipe.readPipe)));
    }
    SC_TRY(process.formatArguments(Span<const StringSpan>(cmd)));
    process.parent = this;
    processes.queueBack(process);
    return Result(true);
}

SC::Result SC::ProcessChain::waitForExitSync()
{
    for (Process* process = processes.front; process != nullptr; process = process->next)
    {
        SC_TRY(process->waitForExitSync());
        process->parent = nullptr;
    }
    processes.clear();
    return Result(true);
}

void SC::ProcessChain::ProcessLinkedList::clear()
{
    Process* current = front;
    while (current != nullptr)
    {
        Process* next = current->next;
        current->next = nullptr;
        current->prev = nullptr;
        current       = next;
    }
    back  = nullptr;
    front = nullptr;
}

void SC::ProcessChain::ProcessLinkedList::queueBack(Process& process)
{
    SC_ASSERT_DEBUG(process.next == nullptr and process.prev == nullptr);
    if (back)
    {
        back->next   = &process;
        process.prev = back;
    }
    else
    {
        SC_ASSERT_DEBUG(front == nullptr);
        front = &process;
    }
    back = &process;
    SC_ASSERT_DEBUG(back->next == nullptr);
    SC_ASSERT_DEBUG(front->prev == nullptr);
}

//-------------------------------------------------------------------------------------------------------
// Process
//-------------------------------------------------------------------------------------------------------
SC::Process::Options::Options() { windowsHide = Process::isWindowsConsoleSubsystem(); }

SC::Result SC::Process::launch(const StdOut& stdOutput, const StdIn& stdInput, const StdErr& stdError)
{
    auto setupInput = [](const StdIn& inputObject, PipeDescriptor& pipe, FileDescriptor& fileDescriptor)
    {
        switch (inputObject.operation)
        {
        case StdStream::Operation::AlreadySetup: break;
        case StdStream::Operation::Inherit: break;
        case StdStream::Operation::Ignore: break;
        case StdStream::Operation::FileDescriptor: {
            SC_TRY_MSG(fileDescriptor.assign(inputObject.fileDescriptor), "Input file is not valid");
        }
        break;
        case StdStream::Operation::ExternalPipe: {
            SC_TRY_MSG(fileDescriptor.assign(move(inputObject.pipeDescriptor->readPipe)),
                       "Input pipe is not valid (forgot createPipe?)");
        }
        break;
        case StdStream::Operation::GrowableBuffer:
        case StdStream::Operation::ReadableSpan: {
            PipeOptions pipeReadOptions;
            pipeReadOptions.readInheritable  = true;
            pipeReadOptions.writeInheritable = false;
            SC_TRY(pipe.createPipe(pipeReadOptions));
            SC_TRY(fileDescriptor.assign(move(pipe.readPipe)));
        }
        break;
        case StdStream::Operation::WritableSpan: {
            return Result(false);
        }
        }
        return Result(true);
    };

    auto setupOutput = [](const StdOut& outputObject, PipeDescriptor& pipe, FileDescriptor& fileDescriptor)
    {
        switch (outputObject.operation)
        {
        case StdStream::Operation::AlreadySetup: break;
        case StdStream::Operation::Inherit: break;
        case StdStream::Operation::Ignore: {
            SC_TRY(fileDescriptor.openForWriteToDevNull());
            break;
        }
        case StdStream::Operation::FileDescriptor: {
            SC_TRY_MSG(fileDescriptor.assign(outputObject.fileDescriptor), "Output file is not valid");
        }
        break;
        case StdStream::Operation::ExternalPipe: {
            SC_TRY_MSG(fileDescriptor.assign(move(outputObject.pipeDescriptor->writePipe)),
                       "Output pipe is not valid (forgot createPipe?)");
        }
        break;
        case StdStream::Operation::GrowableBuffer:
        case StdStream::Operation::WritableSpan: {
            PipeOptions pipeWriteOptions;
            pipeWriteOptions.readInheritable  = false;
            pipeWriteOptions.writeInheritable = true;
            SC_TRY(pipe.createPipe(pipeWriteOptions));
            SC_TRY(fileDescriptor.assign(move(pipe.writePipe)));
        }
        break;
        case StdStream::Operation::ReadableSpan: {
            return Result(false);
        }
        }
        return Result(true);
    };
    PipeDescriptor  pipes[3];
    PipeDescriptor& stdoutPipe = pipes[0];
    PipeDescriptor& stderrPipe = pipes[1];
    PipeDescriptor& stdinPipe  = pipes[2];

    // Setup requested input / output / error redirection
    SC_TRY(setupInput(stdInput, stdinPipe, stdInFd));
    SC_TRY(setupOutput(stdOutput, stdoutPipe, stdOutFd));
    SC_TRY(setupOutput(stdError, stderrPipe, stdErrFd));

    SC_TRY(launchImplementation());

    switch (stdInput.operation)
    {
    case StdStream::Operation::AlreadySetup: break;
    case StdStream::Operation::Inherit: break;
    case StdStream::Operation::Ignore: break;
    case StdStream::Operation::ExternalPipe: break;
    case StdStream::Operation::FileDescriptor: break;
    case StdStream::Operation::GrowableBuffer: {
        IGrowableBuffer::DirectAccess da = stdInput.growableBuffer->getDirectAccess();
        SC_TRY(stdinPipe.writePipe.write({static_cast<const char*>(da.data), da.sizeInBytes}));
        SC_TRY(stdinPipe.writePipe.close());
    }
    break;
    case StdStream::Operation::ReadableSpan: {
        SC_TRY(stdinPipe.writePipe.write(stdInput.readableSpan));
        SC_TRY(stdinPipe.writePipe.close());
    }
    break;
    case StdStream::Operation::WritableSpan: {
        return Result(false);
    }
    }

    auto finalizeOutput = [](const StdOut& outputObject, PipeDescriptor& pipe)
    {
        switch (outputObject.operation)
        {
        case StdStream::Operation::AlreadySetup: break;
        case StdStream::Operation::Inherit: break;
        case StdStream::Operation::Ignore: break;
        case StdStream::Operation::ExternalPipe: break;
        case StdStream::Operation::FileDescriptor: break;
        case StdStream::Operation::GrowableBuffer: {
            SC_TRY(pipe.readPipe.readUntilEOF(move(*outputObject.growableBuffer)));
            return pipe.close();
        }
        case StdStream::Operation::WritableSpan: {
            Span<char> actuallyRead;
            SC_TRY(pipe.readPipe.read(*outputObject.writableSpan, actuallyRead));
            *outputObject.writableSpan = actuallyRead;
            return pipe.close();
        }
        case StdStream::Operation::ReadableSpan: {
            return Result(false);
        }
        }
        return Result(true);
    };

    // Read output if requested
    SC_TRY(finalizeOutput(stdOutput, stdoutPipe));
    SC_TRY(finalizeOutput(stdError, stderrPipe));

    return Result(true);
}

SC::Result SC::Process::setWorkingDirectory(StringSpan processWorkingDirectory)
{
    return Result(currentDirectory.assign(processWorkingDirectory));
}

SC::Result SC::Process::setEnvironment(StringSpan name, StringSpan value)
{
    StringsArena table = {environment, environmentNumber, environmentByteOffset};
    return table.appendAsSingleString({name, SC_NATIVE_STR("="), value});
}

//-------------------------------------------------------------------------------------------------------
// ProcessEnvironment
//-------------------------------------------------------------------------------------------------------

bool SC::ProcessEnvironment::contains(StringSpan variableName, size_t* index)
{
    for (size_t idx = 0; idx < numberOfEnvironment; ++idx)
    {
        StringSpan name, value;
        SC_TRY(get(idx, name, value));
        if (name == variableName)
        {
            if (index != nullptr)
            {
                *index = idx;
            }
            return true;
        }
    }
    return false;
}
