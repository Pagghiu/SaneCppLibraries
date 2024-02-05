// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"

struct SC::AsyncEventLoop::Internal
{
    FileDescriptor loopFd;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] Result close() { return loopFd.close(); }
    [[nodiscard]] Result createEventLoop() { return Result(true); }
    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop&) { return Result(true); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(const int&) const { return nullptr; }
};

struct SC::AsyncEventLoop::KernelQueue
{
    int newEvents = 0;
    int events[1] = {0};

    [[nodiscard]] Result syncWithKernel(AsyncEventLoop&, SyncMode) { return Result(false); }
    [[nodiscard]] Result validateEvent(int&, bool&) { return Result(true); }
    // clang-format off
    template <typename T> [[nodiscard]] bool setupAsync(T&)     { return true; }
    template <typename T> [[nodiscard]] bool teardownAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool activateAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool completeAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool cancelAsync(T&)    { return true; }
    // clang-format on
};

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread() { return Result(true); }
SC::Result SC::AsyncEventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
SC::Result SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
