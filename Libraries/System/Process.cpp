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

SC::ReturnCode SC::ProcessShell::launch()
{
    if (error.returnCode.isError())
        return error.returnCode;

    if (processes.isEmpty())
        return false;

    if (options.pipeSTDIN)
    {
        SC_TRY_IF(inputPipe.createPipe(FileDescriptorPipe::ReadInheritable, FileDescriptorPipe::WriteNonInheritable));
        SC_TRY_IF(processes.front().standardInput.handle.assign(move(inputPipe.readPipe.handle)));
    }

    if (options.pipeSTDOUT)
    {
        SC_TRY_IF(outputPipe.createPipe(FileDescriptorPipe::ReadNonInheritable, FileDescriptorPipe::WriteInheritable));
        SC_TRY_IF(processes.back().standardOutput.handle.assign(move(outputPipe.writePipe.handle)));
    }

    if (options.pipeSTDERR)
    {
        SC_TRY_IF(errorPipe.createPipe(FileDescriptorPipe::ReadNonInheritable, FileDescriptorPipe::WriteInheritable));
        SC_TRY_IF(processes.back().standardError.handle.assign(move(errorPipe.writePipe.handle)));
    }

    for (Process& process : processes)
    {
        error.returnCode = process.run(options);
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
    if (outputPipe.readPipe.handle.isValid() && outputString)
    {
        while (not readResult.isEOF)
        {
            SC_TRY(readResult, outputPipe.readPipe.readAppend(outputString->data, {buffer.data(), buffer.size()}));
        }
        SC_TRY_IF(outputString->pushNullTerm());
    }
    if (errorPipe.readPipe.handle.isValid() && errorString)
    {
        while (not readResult.isEOF)
        {
            SC_TRY(readResult, errorPipe.readPipe.readAppend(errorString->data, {buffer.data(), buffer.size()}));
        }
        SC_TRY_IF(errorString->pushNullTerm());
    }
    return true;
}

SC::ReturnCode SC::ProcessShell::waitSync()
{
    for (Process& p : processes)
    {
        SC_TRY_IF(p.waitProcessExit());
    }
    processes.clear();
    SC_TRY_IF(inputPipe.writePipe.handle.close());
    SC_TRY_IF(outputPipe.readPipe.handle.close());
    SC_TRY_IF(errorPipe.readPipe.handle.close());
    return error.returnCode;
}

SC::ReturnCode SC::ProcessShell::queueProcess(Span<StringView*> spanArguments)
{
    Process         process;
    StringConverter command(process.command);

    if (options.useShell)
    {
        bool first = true;
        for (StringView* svp : spanArguments)
        {
            if (not first)
            {
                SC_TRY_IF(command.appendNullTerminated(" "));
            }
            first = false;
            if (svp->containsASCIICharacter(' '))
            {
                // has space, must escape it
                SC_TRY_IF(command.appendNullTerminated("\""));
                SC_TRY_IF(command.appendNullTerminated(*svp));
                SC_TRY_IF(command.appendNullTerminated("\""));
            }
            else
            {
                SC_TRY_IF(command.appendNullTerminated(*svp));
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
        SC_TRY_IF(chainPipe.createPipe(FileDescriptorPipe::ReadInheritable, FileDescriptorPipe::WriteInheritable));
        SC_TRY_IF(processes.back().standardOutput.handle.assign(move(chainPipe.writePipe.handle)));
        SC_TRY_IF(process.standardInput.handle.assign(move(chainPipe.readPipe.handle)));
    }
    SC_TRY_IF(processes.push_back(move(process)));
    return true;
}
