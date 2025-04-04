// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"
#include "AsyncInternal.h"

struct SC::AsyncEventLoop::KernelQueue
{
    ~KernelQueue() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] Result close() { return Result(true); }
    [[nodiscard]] Result createEventLoop(AsyncEventLoop::Options) { return Result(true); }
    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop&) { return Result(true); }
    [[nodiscard]] Result wakeUpFromExternalThread() { return Result(true); }
    [[nodiscard]] Result associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
    [[nodiscard]] Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
    [[nodiscard]] Result makesSenseToRunInThreadPool(AsyncRequest&) { return Result(true); }
};

struct SC::AsyncEventLoop::KernelEvents
{
    KernelEvents(KernelQueue&) {}
    uint32_t getNumEvents() const { return 0; }

    [[nodiscard]] Result syncWithKernel(AsyncEventLoop&, Internal::SyncMode) { return Result(true); }
    [[nodiscard]] Result validateEvent(uint32_t, bool&) { return Result(true); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t) const { return nullptr; }

    // clang-format off
    template <typename T> [[nodiscard]] bool setupAsync(T&)     { return true; }
    template <typename T> [[nodiscard]] bool activateAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool completeAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool cancelAsync(T&)    { return true; }


    template <typename T> [[nodiscard]] static bool teardownAsync(T&)  { return true; }
    template <typename T, typename P>
    [[nodiscard]] static Result executeOperation(T&, P&)        { return Result(true); }
    // clang-format on
};
