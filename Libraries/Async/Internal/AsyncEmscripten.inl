// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"

struct SC::AsyncEventLoop::Internal
{
    FileDescriptor loopFd;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] Result close() { return loopFd.close(); }
    [[nodiscard]] Result createEventLoop() { return Result(true); }
    [[nodiscard]] Result createWakeup(AsyncEventLoop&) { return Result(true); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(const int&) const { return nullptr; }
};

struct SC::AsyncEventLoop::KernelQueue
{
    int newEvents = 0;
    int events[1] = {0};

    [[nodiscard]] Result pushNewSubmission(AsyncRequest&) { return Result(false); }
    [[nodiscard]] Result pollAsync(AsyncEventLoop&, PollMode) { return Result(false); }
    [[nodiscard]] Result validateEvent(int&, bool&) { return Result(true); }
    template <typename T>
    [[nodiscard]] Result setupAsync(T&)
    {
        return Result(false);
    }
    template <typename T>
    [[nodiscard]] Result stopAsync(T&)
    {
        return Result(false);
    }
    template <typename T>
    [[nodiscard]] Result activateAsync(T&)
    {
        return Result(false);
    }
    template <typename T>
    [[nodiscard]] Result completeAsync(T&)
    {
        return Result(false);
    }
};

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread() { return Result(true); }
SC::Result SC::AsyncEventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
SC::Result SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
