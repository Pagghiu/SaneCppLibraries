// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../FileSystem/FileDescriptor.h"
#include "../Foundation/Function.h"
#include "../Foundation/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Opaque.h"
#include "../Foundation/Optional.h"
#include "../Foundation/String.h"

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

struct ProcessExitStatus
{
    Optional<int32_t> value = 0;
};
#if SC_PLATFORM_WINDOWS
using ProcessNative                                 = void*;   // HANDLE
static constexpr ProcessNative ProcessNativeInvalid = nullptr; // INVALID_HANDLE_VALUE
#else
using ProcessNative                                 = int; // pid_t
static constexpr ProcessNative ProcessNativeInvalid = 0;
#endif
ReturnCode ProcessNativeHandleClose(ProcessNative& handle);
struct ProcessNativeHandle
    : public UniqueTaggedHandle<ProcessNative, ProcessNativeInvalid, ReturnCode, &ProcessNativeHandleClose>
{
};
} // namespace SC

struct SC::Process
{
    ProcessID           processID;
    ProcessExitStatus   exitStatus;
    FileDescriptor      standardInput;
    FileDescriptor      standardOutput;
    FileDescriptor      standardError;
    StringNative<255>   command          = StringEncoding::Native;
    StringNative<255>   currentDirectory = StringEncoding::Native;
    StringNative<1024>  environment      = StringEncoding::Native;
    ProcessNativeHandle handle;

    ProcessChain* parent = nullptr;

    Process* next = nullptr;
    Process* prev = nullptr;

    template <typename... StringView>
    [[nodiscard]] ReturnCode formatCommand(StringView&&... args)
    {
        return formatCommand({forward<StringView>(args)...});
    }
    [[nodiscard]] ReturnCode formatCommand(std::initializer_list<StringView> cmd);
    [[nodiscard]] ReturnCode waitForExitSync();
    template <typename... StringView>
    [[nodiscard]] ReturnCode launch(StringView&&... args)
    {
        SC_TRY_IF(formatCommand({forward<StringView>(args)...}));
        return launch();
    }
    [[nodiscard]] ReturnCode launch(ProcessOptions options = {});
    [[nodiscard]] ReturnCode redirectStdOutTo(FileDescriptorPipe& pipe);
    [[nodiscard]] ReturnCode redirectStdErrTo(FileDescriptorPipe& pipe);
    [[nodiscard]] ReturnCode redirectStdInTo(FileDescriptorPipe& pipe);

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
        return pipe(p, {forward<StringView>(args)...});
    }
    [[nodiscard]] ReturnCode pipe(Process& p, std::initializer_list<StringView> cmd);
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

    FileDescriptorPipe inputPipe;
    FileDescriptorPipe outputPipe;
    FileDescriptorPipe errorPipe;
};
