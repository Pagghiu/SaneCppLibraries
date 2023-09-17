// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Process.h"
#include "../Foundation/Strings/StringConverter.h"

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
    SC_TRY_IF(inputPipe.readPipe.close());
    SC_TRY_IF(outputPipe.writePipe.close());
    SC_TRY_IF(errorPipe.writePipe.close());
    return true;
}

SC::ReturnCode SC::ProcessChain::pipe(Process& process, std::initializer_list<const StringView> cmd)
{
    SC_TRY_MSG(process.parent == nullptr, "Process::pipe - already in use"_a8);

    if (not processes.isEmpty())
    {
        PipeDescriptor chainPipe;
        SC_TRY_IF(chainPipe.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteInheritable));
        SC_TRY_IF(processes.back->standardOutput.assign(move(chainPipe.writePipe)));
        SC_TRY_IF(process.standardInput.assign(move(chainPipe.readPipe)));
    }
    SC_TRY_IF(process.formatArguments(Span<const StringView>(cmd)));
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
    SC_TRY_IF(inputPipe.writePipe.close());
    SC_TRY_IF(outputPipe.readPipe.close());
    SC_TRY_IF(errorPipe.readPipe.close());
    return error.returnCode;
}

SC::ReturnCode SC::Process::formatArguments(Span<const StringView> params)
{
    bool            first = true;
    StringConverter formattedCmd(command);
    formattedCmd.clear();
    for (const StringView& svp : params)
    {
        if (not first)
        {
            SC_TRY_IF(formattedCmd.appendNullTerminated(" "));
        }
        first = false;
        if (svp.containsChar(' '))
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

SC::ReturnCode SC::Process::redirectStdOutTo(PipeDescriptor& pipe)
{
    SC_TRY_IF(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteInheritable));
    return standardOutput.assign(move(pipe.writePipe));
}

SC::ReturnCode SC::Process::redirectStdErrTo(PipeDescriptor& pipe)
{
    SC_TRY_IF(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteInheritable));
    return standardError.assign(move(pipe.writePipe));
}

SC::ReturnCode SC::Process::redirectStdInTo(PipeDescriptor& pipe)
{
    SC_TRY_IF(pipe.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteNonInheritable));
    return standardInput.assign(move(pipe.readPipe));
}
