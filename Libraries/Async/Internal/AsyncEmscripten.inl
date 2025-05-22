// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"
#include "AsyncInternal.h"

struct SC::AsyncEventLoop::KernelQueue
{
    ~KernelQueue() { SC_TRUST_RESULT(close()); }
    Result close() { return Result(true); }
    Result createEventLoop(AsyncEventLoop::Options) { return Result(true); }
    Result createSharedWatchers(AsyncEventLoop&) { return Result(true); }
    Result wakeUpFromExternalThread() { return Result(true); }
    Result associateExternallyCreatedSocket(SocketDescriptor&) { return Result(true); }
    Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
    Result makesSenseToRunInThreadPool(AsyncRequest&) { return Result(true); }
};

struct SC::AsyncEventLoop::KernelEvents
{
    KernelEvents(KernelQueue&) {}
    uint32_t getNumEvents() const { return 0; }

    Result syncWithKernel(AsyncEventLoop&, Internal::SyncMode) { return Result(true); }
    Result validateEvent(uint32_t, bool&) { return Result(true); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t) const { return nullptr; }

    // clang-format off
    template <typename T> [[nodiscard]] static bool setupAsync(T&)     { return true; }
    template <typename T> [[nodiscard]] static bool activateAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] static bool completeAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] static bool cancelAsync(T&)    { return true; }

    template <typename T> [[nodiscard]] static bool teardownAsync(T&)          { return true; }
    template <typename T, typename P>   static Result executeOperation(T&, P&) { return Result(true); }
    // clang-format on
};
