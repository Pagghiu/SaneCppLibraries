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

SC::Result SC::ProcessChain::launch(ProcessChainOptions options)
{
    if (not error.returnCode)
        return error.returnCode;

    if (processes.isEmpty())
        return Result(false);

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
        if (not error.returnCode)
        {
            onError(error);
        }
    }
    SC_TRY(inputPipe.readPipe.close());
    SC_TRY(outputPipe.writePipe.close());
    SC_TRY(errorPipe.writePipe.close());
    return Result(true);
}

SC::Result SC::ProcessChain::pipe(Process& process, std::initializer_list<const StringView> cmd)
{
    SC_TRY_MSG(process.parent == nullptr, "Process::pipe - already in use");

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
    return Result(true);
}

SC::Result SC::ProcessChain::readStdOutUntilEOFSync(String& destination)
{
    return outputPipe.readPipe.readUntilEOF(destination);
}

SC::Result SC::ProcessChain::readStdErrUntilEOFSync(String& destination)
{
    return errorPipe.readPipe.readUntilEOF(destination);
}

SC::Result SC::ProcessChain::readStdOutUntilEOFSync(Vector<char>& destination)
{
    return outputPipe.readPipe.readUntilEOF(destination);
}

SC::Result SC::ProcessChain::readStdErrUntilEOFSync(Vector<char>& destination)
{
    return errorPipe.readPipe.readUntilEOF(destination);
}

SC::Result SC::ProcessChain::waitForExitSync()
{
    for (Process* process = processes.front; process != nullptr; process = process->next)
    {
        error.returnCode = process->waitForExitSync();
        if (not error.returnCode)
        {
            onError(error);
        }
        process->parent = nullptr;
    }
    processes.clear();
    SC_TRY(inputPipe.writePipe.close());
    SC_TRY(outputPipe.readPipe.close());
    SC_TRY(errorPipe.readPipe.close());
    return error.returnCode;
}

SC::Result SC::Process::formatArguments(Span<const StringView> params)
{
    bool            first = true;
    StringConverter formattedCmd(command, StringConverter::Clear);
    for (const StringView& svp : params)
    {
        if (not first)
        {
            SC_TRY(formattedCmd.appendNullTerminated(" "));
        }
        first = false;
        if (svp.containsChar(' ')) // TODO: Must escape also quotes
        {
            SC_TRY(formattedCmd.appendNullTerminated("\""));
            SC_TRY(formattedCmd.appendNullTerminated(svp));
            SC_TRY(formattedCmd.appendNullTerminated("\""));
        }
        else
        {
            SC_TRY(formattedCmd.appendNullTerminated(svp));
        }
    }
    return Result(true);
}

SC::Result SC::Process::redirectStdOutTo(PipeDescriptor& pipe)
{
    SC_TRY(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteInheritable));
    return standardOutput.assign(move(pipe.writePipe));
}

SC::Result SC::Process::redirectStdErrTo(PipeDescriptor& pipe)
{
    SC_TRY(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteInheritable));
    return standardError.assign(move(pipe.writePipe));
}

SC::Result SC::Process::redirectStdInTo(PipeDescriptor& pipe)
{
    SC_TRY(pipe.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteNonInheritable));
    return standardInput.assign(move(pipe.readPipe));
}
