// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Process.h"
#include "../Foundation/StringConverter.h"

#if SC_PLATFORM_WINDOWS
#include "ProcessInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "ProcessInternalEmscripten.inl"
#else
#include "ProcessInternalPosix.inl"
#endif

SC::ReturnCode SC::ProcessChain::launch(ProcessChainOptions options)
{
    if (error.returnCode.isError())
        return error.returnCode;

    if (processes.isEmpty())
        return false;

    if (options.pipeSTDIN)
    {
        SC_TRY_IF(processes.front->redirectStdInTo(inputPipe));
    }

    if (options.pipeSTDOUT)
    {
        SC_TRY_IF(processes.back->redirectStdOutTo(outputPipe));
    }

    if (options.pipeSTDERR)
    {
        SC_TRY_IF(processes.back->redirectStdErrTo(errorPipe));
    }

    for (Process* process = processes.front; process != nullptr; process = process->next)
    {
        error.returnCode = process->launch();
        if (error.returnCode.isError())
        {
            // TODO: Decide what to do with the queue
            onError(error);
            return error.returnCode;
        }
    }
    SC_TRY_IF(inputPipe.readPipe.handle.close());
    SC_TRY_IF(outputPipe.writePipe.handle.close());
    SC_TRY_IF(errorPipe.writePipe.handle.close());
    return true;
}

SC::ReturnCode SC::ProcessChain::pipe(Process& process, std::initializer_list<StringView> cmd)
{
    SC_TRY_MSG(process.parent == nullptr, "Process::pipe - already in use"_a8);

    if (not processes.isEmpty())
    {
        FileDescriptorPipe chainPipe;
        SC_TRY_IF(chainPipe.createPipe(FileDescriptorPipe::ReadInheritable, FileDescriptorPipe::WriteInheritable));
        SC_TRY_IF(processes.back->standardOutput.handle.assign(move(chainPipe.writePipe.handle)));
        SC_TRY_IF(process.standardInput.handle.assign(move(chainPipe.readPipe.handle)));
    }
    SC_TRY_IF(process.formatCommand(cmd));
    process.parent = this;
    processes.queueBack(process);
    return true;
}

SC::ReturnCode SC::ProcessChain::readStdOutUntilEOFSync(String& destination)
{
    return outputPipe.readPipe.readUntilEOF(destination);
}

SC::ReturnCode SC::ProcessChain::readStdErrUntilEOFSync(String& destination)
{
    return errorPipe.readPipe.readUntilEOF(destination);
}

SC::ReturnCode SC::ProcessChain::readStdOutUntilEOFSync(Vector<char_t>& destination)
{
    return outputPipe.readPipe.readUntilEOF(destination);
}

SC::ReturnCode SC::ProcessChain::readStdErrUntilEOFSync(Vector<char_t>& destination)
{
    return errorPipe.readPipe.readUntilEOF(destination);
}

SC::ReturnCode SC::ProcessChain::waitForExitSync()
{
    for (Process* process = processes.front; process != nullptr; process = process->next)
    {
        SC_TRY_IF(process->waitForExitSync());
        process->parent = nullptr;
    }
    processes.clear();
    SC_TRY_IF(inputPipe.writePipe.handle.close());
    SC_TRY_IF(outputPipe.readPipe.handle.close());
    SC_TRY_IF(errorPipe.readPipe.handle.close());
    return error.returnCode;
}

SC::ReturnCode SC::Process::formatCommand(std::initializer_list<StringView> params)
{
    command.data.clear();
    bool            first = true;
    StringConverter formattedCmd(command);
    for (const StringView& svp : params)
    {
        if (not first)
        {
            SC_TRY_IF(formattedCmd.appendNullTerminated(" "));
        }
        first = false;
        if (svp.containsASCIICharacter(' '))
        {
            // has space, must escape it
            SC_TRY_IF(formattedCmd.appendNullTerminated("\""));
            SC_TRY_IF(formattedCmd.appendNullTerminated(svp));
            SC_TRY_IF(formattedCmd.appendNullTerminated("\""));
        }
        else
        {
            SC_TRY_IF(formattedCmd.appendNullTerminated(svp));
        }
    }
    return true;
}

SC::ReturnCode SC::Process::redirectStdOutTo(FileDescriptorPipe& pipe)
{
    SC_TRY_IF(pipe.createPipe(FileDescriptorPipe::ReadNonInheritable, FileDescriptorPipe::WriteInheritable));
    return standardOutput.handle.assign(move(pipe.writePipe.handle));
}

SC::ReturnCode SC::Process::redirectStdErrTo(FileDescriptorPipe& pipe)
{
    SC_TRY_IF(pipe.createPipe(FileDescriptorPipe::ReadNonInheritable, FileDescriptorPipe::WriteInheritable));
    return standardError.handle.assign(move(pipe.writePipe.handle));
}

SC::ReturnCode SC::Process::redirectStdInTo(FileDescriptorPipe& pipe)
{
    SC_TRY_IF(pipe.createPipe(FileDescriptorPipe::ReadInheritable, FileDescriptorPipe::WriteNonInheritable));
    return standardInput.handle.assign(move(pipe.readPipe.handle));
}
