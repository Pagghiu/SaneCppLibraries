// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../File/FileDescriptor.h"
#include "../Foundation/Containers/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Objects/Function.h"
#include "../Foundation/Objects/Optional.h"
#include "../Foundation/Strings/String.h"
#include "ProcessDescriptor.h"

namespace SC
{
struct Process;
struct ProcessChain;
struct ProcessChainOptions;
struct ProcessOptions
{
    bool inheritFileDescriptors = false;
};
struct ProcessID
{
    int32_t pid = 0;
};
} // namespace SC

struct SC::Process
{
    ProcessDescriptor  handle;
    ProcessID          processID;
    FileDescriptor     standardInput;
    FileDescriptor     standardOutput;
    FileDescriptor     standardError;
    StringNative<255>  command          = StringEncoding::Native;
    StringNative<255>  currentDirectory = StringEncoding::Native;
    StringNative<1024> environment      = StringEncoding::Native;

    ProcessDescriptor::ExitStatus exitStatus;

    ProcessChain* parent = nullptr;

    Process* next = nullptr;
    Process* prev = nullptr;

    template <typename... StringView>
    [[nodiscard]] ReturnCode formatCommand(StringView&&... args)
    {
        return formatArguments({forward<StringView>(args)...});
    }

    [[nodiscard]] ReturnCode formatArguments(Span<const StringView> cmd);
    [[nodiscard]] ReturnCode waitForExitSync();
    template <typename... StringView>
    [[nodiscard]] ReturnCode launch(StringView&&... args)
    {
        SC_TRY(formatArguments({forward<StringView>(args)...}));
        return launch();
    }
    [[nodiscard]] ReturnCode launch(ProcessOptions options = {});
    [[nodiscard]] ReturnCode redirectStdOutTo(PipeDescriptor& pipe);
    [[nodiscard]] ReturnCode redirectStdErrTo(PipeDescriptor& pipe);
    [[nodiscard]] ReturnCode redirectStdInTo(PipeDescriptor& pipe);

  private:
    struct Internal;

    template <typename Lambda>
    [[nodiscard]] ReturnCode spawn(Lambda&& lambda);
    [[nodiscard]] ReturnCode fork();
    [[nodiscard]] bool       isChild() const;
};

struct SC::ProcessChainOptions
{
    bool pipeSTDIN  = false;
    bool pipeSTDOUT = false;
    bool pipeSTDERR = false;
};
struct SC::ProcessChain
{

    struct Error
    {
        ReturnCode returnCode = true;
    };

    ProcessChain(Delegate<const Error&> onError) : onError(onError) {}

    template <typename... StringView>
    [[nodiscard]] ReturnCode pipe(Process& p, StringView&&... args)
    {
        return pipe(p, {forward<const StringView>(args)...});
    }
    [[nodiscard]] ReturnCode pipe(Process& p, std::initializer_list<const StringView> cmd);
    [[nodiscard]] ReturnCode launch(ProcessChainOptions options = ProcessChainOptions());
    [[nodiscard]] ReturnCode waitForExitSync();
    [[nodiscard]] ReturnCode readStdOutUntilEOFSync(String& destination);
    [[nodiscard]] ReturnCode readStdErrUntilEOFSync(String& destination);
    [[nodiscard]] ReturnCode readStdOutUntilEOFSync(Vector<char_t>& destination);
    [[nodiscard]] ReturnCode readStdErrUntilEOFSync(Vector<char_t>& destination);

  private:
    Delegate<const Error&> onError;
    Error                  error;

    IntrusiveDoubleLinkedList<Process> processes;

    PipeDescriptor inputPipe;
    PipeDescriptor outputPipe;
    PipeDescriptor errorPipe;
};
