// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../FileSystem/FileDescriptor.h"
#include "../Foundation/Function.h"
#include "../Foundation/Opaque.h"
#include "../Foundation/Optional.h"
#include "../Foundation/String.h"

namespace SC
{
struct ProcessID
{
    int32_t pid = 0;
};

struct Process;
struct ProcessOptions
{
    bool useShell               = true;
    bool pipeSTDIN              = false;
    bool pipeSTDOUT             = false;
    bool pipeSTDERR             = false;
    bool inheritFileDescriptors = false;
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

    [[nodiscard]] ReturnCode waitProcessExit();
    [[nodiscard]] ReturnCode run(const ProcessOptions& options);

  private:
    struct Internal;
    template <typename Lambda>
    [[nodiscard]] ReturnCode spawn(Lambda&& lambda);
    [[nodiscard]] ReturnCode fork();
    [[nodiscard]] bool       isChild() const;
};

namespace SC
{
struct ProcessShell;
} // namespace SC

struct SC::ProcessShell
{
    struct Error
    {
        ReturnCode returnCode = true;
    };
    ProcessOptions options;

    ProcessShell(Delegate<const Error&> onError) : onError(onError) {}

    [[nodiscard]] ProcessShell& pipe(StringView s1, StringView s2 = StringView(), StringView s3 = StringView(),
                                     StringView s4 = StringView());

    ReturnCode launch();
    ReturnCode readOutputSync(String* outputString = nullptr, String* errorString = nullptr);
    ReturnCode waitSync();

  private:
    Delegate<const Error&> onError;
    Error                  error;
    Vector<Process>        processes;

    FileDescriptorPipe inputPipe;
    FileDescriptorPipe outputPipe;
    FileDescriptorPipe errorPipe;

    ReturnCode queueProcess(Span<StringView*> spanArguments);
};
