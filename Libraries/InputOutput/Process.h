// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Function.h"
#include "../Foundation/Language.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Result.h"
#include "../Foundation/StringNative.h"
#include "../Foundation/Types.h"
#include "../Foundation/Vector.h"
#include "FileDescriptor.h"

namespace SC
{

#if SC_PLATFORM_WINDOWS
using ProcessNativeID     = unsigned long;
using ProcessNativeHandle = void*;
#else
using ProcessNativeID     = int32_t;
using ProcessNativeHandle = int32_t;
#endif
struct ProcessID
{
    ProcessNativeID pid = 0;
};

struct ProcessHandle
{
    static constexpr ProcessNativeHandle InvalidValue() { return 0; }

    ProcessNativeHandle handle = InvalidValue();
    ProcessHandle()            = default;
    ProcessHandle(ProcessHandle&& other)
    {
        handle = other.handle;
        other.makeInvalid();
    }
    ProcessHandle& operator=(ProcessHandle&& other)
    {
        SC_TRUST_RESULT(close());
        handle = other.handle;
        other.makeInvalid();
        return *this;
    }
    ProcessHandle(const ProcessHandle&)            = delete;
    ProcessHandle& operator=(const ProcessHandle&) = delete;
    ~ProcessHandle() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] bool       isValid() { return handle != InvalidValue(); }
    void                     makeInvalid() { handle = InvalidValue(); }
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

    [[nodiscard]] ReturnCode fork();
    [[nodiscard]] bool       isChild() const;
    [[nodiscard]] ReturnCode waitProcessExit();

    [[nodiscard]] ReturnCode run(const ProcessOptions& options);

  private:
    struct Internal;
    template <typename Lambda>
    [[nodiscard]] ReturnCode spawn(Lambda&& lambda);
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
    Delegate<const Error&> onError;
    Error                  error;
    ProcessShell(const ProcessShell& copy)            = delete;
    ProcessShell& operator=(const ProcessShell& copy) = delete;
    ProcessShell(Delegate<const Error&> onError) : onError(onError) {}

    ProcessOptions options;

    [[nodiscard]] ProcessShell& pipe(StringView s1, StringView s2 = StringView(), StringView s3 = StringView(),
                                     StringView s4 = StringView())
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

    Vector<ProcessEntry> processes;

    FileDescriptorPipe inputPipe;
    FileDescriptorPipe outputPipe;
    FileDescriptorPipe errorPipe;

    ReturnCode launch();
    ReturnCode readOutputSync(String* outputString = nullptr, String* errorString = nullptr);
    ReturnCode waitSync();
    ReturnCode queueProcess(Span<StringView*> spanArguments);
};
