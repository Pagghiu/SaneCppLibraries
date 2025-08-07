// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Async/Internal/AsyncInternal.h"
struct SC::AsyncEventLoop::Internal::KernelEvents
{
    bool                  isEpoll = true;
    AlignedStorage<16400> storage;

    KernelEvents(KernelQueue& kernelQueue, AsyncKernelEvents& asyncKernelEvents);
    ~KernelEvents();

    KernelEventsIoURing&       getUring();
    KernelEventsPosix&         getPosix();
    const KernelEventsIoURing& getUring() const;
    const KernelEventsPosix&   getPosix() const;

    [[nodiscard]] uint32_t getNumEvents() const;

    Result syncWithKernel(AsyncEventLoop&, Internal::SyncMode);
    Result validateEvent(uint32_t&, bool&);

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t);

    // clang-format off
    template <typename T> Result setupAsync(AsyncEventLoop&, T&);
    template <typename T> Result activateAsync(AsyncEventLoop&, T&);
    template <typename T> Result cancelAsync(AsyncEventLoop&, T&);
    template <typename T> Result completeAsync(T&);

    template <typename T> static Result teardownAsync(T*, AsyncTeardown&);

    // If False, makes re-activation a no-op, that is a lightweight optimization.
    // More importantly it prevents an assert about being Submitting state when async completes during re-activation run cycle.
    template<typename T> static bool needsSubmissionWhenReactivating(T&)
    {
        return true;
    }

    template <typename T, typename P> static Result executeOperation(T&, P& p);
    // clang-format on
};
