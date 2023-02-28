// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Process.h"

#if SC_PLATFORM_WINDOWS
#include "ProcessInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "ProcessInternalEmscripten.inl"
#else
#include "ProcessInternalPosix.inl"
#endif

template <>
template <int BufferSizeInBytes>
void SC::CompilerFirewallFuncs<SC::ProcessEntry::ProcessHandle>::construct(uint8_t* buffer)
{
    static_assert(BufferSizeInBytes >= sizeof(ProcessEntry::ProcessHandle), "Increase size of unique static pimpl");
    new (buffer, PlacementNew()) ProcessEntry::ProcessHandle();
}
template <>
void SC::CompilerFirewallFuncs<SC::ProcessEntry::ProcessHandle>::destruct(ProcessEntry::ProcessHandle& obj)
{
    obj.~ProcessHandle();
}
template <>
void SC::CompilerFirewallFuncs<SC::ProcessEntry::ProcessHandle>::moveConstruct(uint8_t*                      buffer,
                                                                               ProcessEntry::ProcessHandle&& obj)
{
    new (buffer, PlacementNew()) ProcessEntry::ProcessHandle(forward<ProcessEntry::ProcessHandle>(obj));
}
template <>
void SC::CompilerFirewallFuncs<SC::ProcessEntry::ProcessHandle>::moveAssign(ProcessEntry::ProcessHandle&  pthis,
                                                                            ProcessEntry::ProcessHandle&& obj)
{
    pthis = forward<ProcessEntry::ProcessHandle>(obj);
}

SC::ReturnCode SC::ProcessShell::launch()
{
    if (error.returnCode.isError())
        return error.returnCode;
    if (processes.isEmpty())
        return false;

    if (options.pipeSTDIN)
    {
        SC_TRY_IF(inputPipe.createPipe());
        SC_TRY_IF(inputPipe.readPipe.posix().setCloseOnExec());
        SC_TRY_IF(inputPipe.writePipe.posix().setCloseOnExec());
        SC_TRY_IF(inputPipe.writePipe.windows().disableInherit());
        SC_TRY_IF(processes.front().standardInput.assignMovingFrom(inputPipe.readPipe));
    }
    if (options.pipeSTDOUT)
    {
        SC_TRY_IF(outputPipe.createPipe());
        SC_TRY_IF(outputPipe.readPipe.posix().setCloseOnExec());
        SC_TRY_IF(outputPipe.writePipe.posix().setCloseOnExec());
        SC_TRY_IF(outputPipe.readPipe.windows().disableInherit());
        SC_TRY_IF(processes.back().standardOutput.assignMovingFrom(outputPipe.writePipe));
    }
    if (options.pipeSTDERR)
    {
        SC_TRY_IF(errorPipe.createPipe());
        SC_TRY_IF(errorPipe.readPipe.posix().setCloseOnExec());
        SC_TRY_IF(errorPipe.writePipe.posix().setCloseOnExec());
        SC_TRY_IF(errorPipe.readPipe.windows().disableInherit());
        SC_TRY_IF(processes.back().standardError.assignMovingFrom(errorPipe.writePipe));
    }

    for (ProcessEntry& process : processes)
    {
        error.returnCode = process.run(options);
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

SC::ProcessShell& SC::ProcessShell::pipe(StringView s1, StringView s2, StringView s3, StringView s4)
{
    if (not error.returnCode)
    {
        return *this;
    }
    StringView* arguments[]  = {&s1, &s2, &s3, &s4};
    size_t      numArguments = ConstantArraySize(arguments);
    for (; numArguments > 0; --numArguments)
    {
        if (!arguments[numArguments - 1]->isEmpty())
        {
            break;
        }
    }
    Span<StringView*> spanArguments = Span<StringView*>(&arguments[0], numArguments * sizeof(StringView*));
    error.returnCode                = queueProcess(spanArguments);
    return *this;
}

SC::ReturnCode SC::ProcessShell::readOutputSync(String* outputString, String* errorString)
{
    Array<char, 1024> buffer;
    SC_TRUST_RESULT(buffer.resizeWithoutInitializing(buffer.capacity()));
    FileDescriptor::ReadResult readResult;
    if (outputPipe.readPipe.isValid() && outputString)
    {
        while (not readResult.isEOF)
        {
            SC_TRY(readResult, outputPipe.readPipe.readAppend(outputString->data, {buffer.data(), buffer.size()}));
        }
        outputString->pushNullTerm();
    }
    if (errorPipe.readPipe.isValid() && errorString)
    {
        while (not readResult.isEOF)
        {
            SC_TRY(readResult, errorPipe.readPipe.readAppend(errorString->data, {buffer.data(), buffer.size()}));
        }
        errorString->pushNullTerm();
    }
    return true;
}

SC::ReturnCode SC::ProcessShell::waitSync()
{
    for (ProcessEntry& p : processes)
    {
        SC_TRY_IF(p.waitProcessExit());
    }
    processes.clear();
    SC_TRY_IF(inputPipe.writePipe.close());
    SC_TRY_IF(outputPipe.readPipe.close());
    SC_TRY_IF(errorPipe.readPipe.close());
    return error.returnCode;
}

SC::ReturnCode SC::ProcessShell::queueProcess(Span<StringView*> spanArguments)
{
    ProcessEntry process;
    if (options.useShell)
    {
        bool first = true;
        for (StringView* svp : spanArguments)
        {
            if (not first)
            {
                SC_TRY_IF(process.command.appendNullTerminated(" "));
            }
            first = false;
            if (svp->containsASCIICharacter(' '))
            {
                // has space, must escape it
                SC_TRY_IF(process.command.appendNullTerminated("\""));
                SC_TRY_IF(process.command.appendNullTerminated(*svp));
                SC_TRY_IF(process.command.appendNullTerminated("\""));
            }
            else
            {
                SC_TRY_IF(process.command.appendNullTerminated(*svp));
            }
        }
    }
    else
    {
        return "UseShell==false Not Implemented yet"_a8;
    }
    if (not processes.isEmpty())
    {
        FileDescriptorPipe chainPipe;
        SC_TRY_IF(chainPipe.createPipe());
        SC_TRY_IF(chainPipe.readPipe.posix().setCloseOnExec());
        SC_TRY_IF(chainPipe.writePipe.posix().setCloseOnExec());
        SC_TRY_IF(processes.back().standardOutput.assignMovingFrom(chainPipe.writePipe));
        SC_TRY_IF(process.standardInput.assignMovingFrom(chainPipe.readPipe));
    }
    SC_TRY_IF(processes.push_back(move(process)));
    return true;
}
