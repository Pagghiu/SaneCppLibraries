// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"

struct SC::AsyncEventLoop::Internal
{
    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] Result close() { return Result(true); }
    [[nodiscard]] Result createEventLoop(AsyncEventLoop::Options) { return Result(true); }
    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop&) { return Result(true); }
    [[nodiscard]] Result wakeUpFromExternalThread() { return Result(true); }
    [[nodiscard]] Result associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
    [[nodiscard]] Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
};

struct SC::AsyncEventLoop::KernelQueue
{
    KernelQueue(Internal&) {}
    uint32_t getNumEvents() const { return 0; }

    [[nodiscard]] Result syncWithKernel(AsyncEventLoop&, SyncMode) { return Result(true); }
    [[nodiscard]] Result validateEvent(uint32_t, bool&) { return Result(true); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t) const { return nullptr; }

    // clang-format off
    template <typename T> [[nodiscard]] bool setupAsync(T&)     { return true; }
    template <typename T> [[nodiscard]] bool teardownAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool activateAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool completeAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool cancelAsync(T&)    { return true; }
    // clang-format on
};
