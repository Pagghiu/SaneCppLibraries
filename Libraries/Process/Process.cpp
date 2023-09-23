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
        SC_TRY(processes.front->redirectStdInTo(inputPipe));
    }

    if (options.pipeSTDOUT)
    {
        SC_TRY(processes.back->redirectStdOutTo(outputPipe));
    }

    if (options.pipeSTDERR)
    {
        SC_TRY(processes.back->redirectStdErrTo(errorPipe));
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
    SC_TRY(inputPipe.readPipe.close());
    SC_TRY(outputPipe.writePipe.close());
    SC_TRY(errorPipe.writePipe.close());
    return true;
}

SC::ReturnCode SC::ProcessChain::pipe(Process& process, std::initializer_list<const StringView> cmd)
{
    SC_TRY_MSG(process.parent == nullptr, "Process::pipe - already in use"_a8);

    if (not processes.isEmpty())
    {
        PipeDescriptor chainPipe;
        SC_TRY(chainPipe.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteInheritable));
        SC_TRY(processes.back->standardOutput.assign(move(chainPipe.writePipe)));
        SC_TRY(process.standardInput.assign(move(chainPipe.readPipe)));
    }
    SC_TRY(process.formatArguments(Span<const StringView>(cmd)));
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
        SC_TRY(process->waitForExitSync());
        process->parent = nullptr;
    }
    processes.clear();
    SC_TRY(inputPipe.writePipe.close());
    SC_TRY(outputPipe.readPipe.close());
    SC_TRY(errorPipe.readPipe.close());
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
            SC_TRY(formattedCmd.appendNullTerminated(" "));
        }
        first = false;
        if (svp.containsChar(' '))
        {
            // has space, must escape it
            SC_TRY(formattedCmd.appendNullTerminated("\""));
            SC_TRY(formattedCmd.appendNullTerminated(svp));
            SC_TRY(formattedCmd.appendNullTerminated("\""));
        }
        else
        {
            SC_TRY(formattedCmd.appendNullTerminated(svp));
        }
    }
    return true;
}

SC::ReturnCode SC::Process::redirectStdOutTo(PipeDescriptor& pipe)
{
    SC_TRY(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteInheritable));
    return standardOutput.assign(move(pipe.writePipe));
}

SC::ReturnCode SC::Process::redirectStdErrTo(PipeDescriptor& pipe)
{
    SC_TRY(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteInheritable));
    return standardError.assign(move(pipe.writePipe));
}

SC::ReturnCode SC::Process::redirectStdInTo(PipeDescriptor& pipe)
{
    SC_TRY(pipe.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteNonInheritable));
    return standardInput.assign(move(pipe.readPipe));
}
