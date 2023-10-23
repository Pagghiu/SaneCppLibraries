// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Containers/IntrusiveDoubleLinkedList.h"
#include "../File/FileDescriptor.h"
#include "../Foundation/Function.h"
#include "../Strings/SmallString.h"
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
    [[nodiscard]] Result formatCommand(StringView&&... args)
    {
        return formatArguments({forward<StringView>(args)...});
    }

    [[nodiscard]] Result formatArguments(Span<const StringView> cmd);
    [[nodiscard]] Result waitForExitSync();
    template <typename... StringView>
    [[nodiscard]] Result launch(StringView&&... args)
    {
        SC_TRY(formatArguments({forward<StringView>(args)...}));
        return launch();
    }
    [[nodiscard]] Result launch(ProcessOptions options = {});
    [[nodiscard]] Result redirectStdOutTo(PipeDescriptor& pipe);
    [[nodiscard]] Result redirectStdErrTo(PipeDescriptor& pipe);
    [[nodiscard]] Result redirectStdInTo(PipeDescriptor& pipe);

  private:
    struct Internal;

    template <typename Lambda>
    [[nodiscard]] Result spawn(Lambda&& lambda);
    [[nodiscard]] Result fork();
    [[nodiscard]] bool   isChild() const;
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
        Result returnCode = Result(true);
    };

    ProcessChain(Delegate<const Error&> onError) : onError(onError) {}

    template <typename... StringView>
    [[nodiscard]] Result pipe(Process& p, StringView&&... args)
    {
        return pipe(p, {forward<const StringView>(args)...});
    }
    [[nodiscard]] Result pipe(Process& p, std::initializer_list<const StringView> cmd);
    [[nodiscard]] Result launch(ProcessChainOptions options = ProcessChainOptions());
    [[nodiscard]] Result waitForExitSync();
    [[nodiscard]] Result readStdOutUntilEOFSync(String& destination);
    [[nodiscard]] Result readStdErrUntilEOFSync(String& destination);
    [[nodiscard]] Result readStdOutUntilEOFSync(Vector<char>& destination);
    [[nodiscard]] Result readStdErrUntilEOFSync(Vector<char>& destination);

  private:
    Delegate<const Error&> onError;
    Error                  error;

    IntrusiveDoubleLinkedList<Process> processes;

    PipeDescriptor inputPipe;
    PipeDescriptor outputPipe;
    PipeDescriptor errorPipe;
};
