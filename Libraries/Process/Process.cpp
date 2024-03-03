// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Process.h"
#include "../Strings/StringConverter.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/ProcessWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "Internal/ProcessEmscripten.inl"
#else
#include "Internal/ProcessPosix.inl"
#endif

//-------------------------------------------------------------------------------------------------------
// ProcessChain
//-------------------------------------------------------------------------------------------------------

SC::Result SC::ProcessChain::launch(const Process::StdOut& stdOut, const Process::StdIn& stdIn,
                                    const Process::StdErr& stdErr)
{
    if (processes.isEmpty())
    {
        return Result(false);
    }
    for (Process* process = processes.front; process != nullptr; process = process->next)
    {
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

SC::Result SC::ProcessChain::pipe(Process& process, const Span<const StringView> cmd)
{
    SC_TRY_MSG(process.parent == nullptr, "Process::pipe - already in use");

    if (not processes.isEmpty())
    {
        PipeDescriptor chainPipe;
        SC_TRY(chainPipe.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteInheritable));
        SC_TRY(processes.back->stdOutFd.assign(move(chainPipe.writePipe)));
        SC_TRY(process.stdInFd.assign(move(chainPipe.readPipe)));
    }
    SC_TRY(process.formatArguments(Span<const StringView>(cmd)));
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

//-------------------------------------------------------------------------------------------------------
// Process
//-------------------------------------------------------------------------------------------------------

SC::Result SC::Process::formatArguments(Span<const StringView> params)
{
#if SC_PLATFORM_WINDOWS
    bool first = true;

    StringConverter formattedCmd(command, StringConverter::Clear);
    for (const StringView& param : params)
    {
        if (not first)
        {
            SC_TRY(formattedCmd.appendNullTerminated(" "));
        }
        first = false;
        if (param.containsCodePoint(' ')) // TODO: Must escape also quotes
        {
            SC_TRY(formattedCmd.appendNullTerminated("\""));
            SC_TRY(formattedCmd.appendNullTerminated(param));
            SC_TRY(formattedCmd.appendNullTerminated("\""));
        }
        else
        {
            SC_TRY(formattedCmd.appendNullTerminated(param));
        }
    }
#else

    SC_TRY_MSG(params.sizeInElements() <= MAX_NUM_ARGUMENTS, "Exceeding limit of 256 arguments");
    StringConverter formattedCmd(command, StringConverter::Clear);
    for (size_t idx = 0; idx < params.sizeInElements(); ++idx)
    {
        commandArgumentsByteOffset[idx] = command.sizeInBytesIncludingTerminator();
        SC_TRY(formattedCmd.appendNullTerminated(params.data()[idx], false)); // false == keep previous null terminator
    }
    commandArgumentsNumber = params.sizeInElements();
#endif
    return Result(true);
}

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
            SC_TRY(fileDescriptor.assign(inputObject.fileDescriptor));
        }
        break;
        case StdStream::Operation::ExternalPipe:
        case StdStream::Operation::Vector:
        case StdStream::Operation::String:
        case StdStream::Operation::ReadableSpan: {
            SC_TRY(pipe.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteNonInheritable));
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
        case StdStream::Operation::Ignore: break;
        case StdStream::Operation::FileDescriptor: {
            SC_TRY(fileDescriptor.assign(outputObject.fileDescriptor));
        }
        break;
        case StdStream::Operation::ExternalPipe:
        case StdStream::Operation::Vector:
        case StdStream::Operation::String:
        case StdStream::Operation::WritableSpan: {
            SC_TRY(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteInheritable));
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
    PipeDescriptor& stdoutPipe =
        stdOutput.operation == StdOut::Operation::ExternalPipe ? *stdOutput.pipeDescriptor : pipes[0];
    PipeDescriptor& stderrPipe =
        stdError.operation == StdOut::Operation::ExternalPipe ? *stdError.pipeDescriptor : pipes[1];
    PipeDescriptor& stdinPipe =
        stdInput.operation == StdOut::Operation::ExternalPipe ? *stdInput.pipeDescriptor : pipes[2];

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
    case StdStream::Operation::Vector: {
        SC_TRY(stdinPipe.writePipe.write(stdInput.vector->toSpan()));
        SC_TRY(stdinPipe.writePipe.close());
    }
    break;
    case StdStream::Operation::String: {
        SC_TRY(stdinPipe.writePipe.write(stdInput.string->view().toCharSpan()));
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
        case StdStream::Operation::Vector: {
            SC_TRY(pipe.readPipe.readUntilEOF(*outputObject.vector));
            return pipe.close();
        }
        break;
        case StdStream::Operation::String: {
            SC_TRY(pipe.readPipe.readUntilEOF(*outputObject.string));
            return pipe.close();
        }
        case StdStream::Operation::WritableSpan: {
            Span<char> actuallyRead;
            SC_TRY(pipe.readPipe.read(outputObject.writableSpan, actuallyRead));
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
