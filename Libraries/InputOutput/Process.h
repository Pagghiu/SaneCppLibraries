// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Function.h"
#include "../Foundation/Language.h"
#include "../Foundation/MovableHandle.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Result.h"
#include "../Foundation/StringNative.h"
#include "../Foundation/Types.h"
#include "../Foundation/Vector.h"
#include "FileDescriptor.h"

namespace SC
{
// TODO: We should really type erase the process handle and do this under th hood
#if SC_PLATFORM_WINDOWS
using ProcessNativeID     = unsigned long;
using ProcessNativeHandle = void*;
ReturnCode ProcessNativeHandleCloseWindows(const ProcessNativeHandle& handle);
using ProcessHandle = MovableHandle<ProcessNativeHandle, nullptr, ReturnCode, &ProcessNativeHandleCloseWindows>;
#else
using ProcessNativeID     = int32_t;
using ProcessNativeHandle = int32_t;
ReturnCode ProcessNativeHandleClosePosix(const ProcessNativeHandle& handle);
using ProcessHandle = MovableHandle<ProcessNativeHandle, 0, ReturnCode, &ProcessNativeHandleClosePosix>;
#endif
struct ProcessID
{
    ProcessNativeID pid = 0;
};

struct ProcessEntry;
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
} // namespace SC

struct SC::ProcessEntry
{
    ProcessID          processID;
    ProcessHandle      processHandle;
    ProcessExitStatus  exitStatus;
    FileDescriptor     standardInput;
    FileDescriptor     standardOutput;
    FileDescriptor     standardError;
    StringNative<255>  command;
    StringNative<255>  currentDirectory;
    StringNative<1024> environment;

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
    Vector<ProcessEntry>   processes;

    FileDescriptorPipe inputPipe;
    FileDescriptorPipe outputPipe;
    FileDescriptorPipe errorPipe;
    ReturnCode         queueProcess(Span<StringView*> spanArguments);
};
