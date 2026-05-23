// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "HttpClient.h"

namespace SC
{
//! @addtogroup group_http_client
//! @{

/// @brief Caller-owned memory for HttpClientOperationScheduler.
struct SC_HTTP_CLIENT_EXPORT HttpClientOperationSchedulerMemory
{
    Span<HttpClientOperation*> operations;
    Span<uint8_t>              readyOperations; ///< One byte per operation
};

/// @brief Optional coordinator for polling many HttpClientOperation instances.
///
/// The scheduler installs itself as the operation notifier. It does not own operations, operation
/// memory, or request state, and it does not start requests. It only tracks which operations queued
/// events so callers can avoid blindly polling every operation on each loop iteration.
struct SC_HTTP_CLIENT_EXPORT HttpClientOperationScheduler final : private HttpClientOperationNotifier
{
    [[nodiscard]] Result init(const HttpClientOperationSchedulerMemory& memory);
    [[nodiscard]] Result close();

    [[nodiscard]] Result markReady(HttpClientOperation& operation);
    [[nodiscard]] Result pollReady(size_t& numPolled, uint32_t waitMilliseconds = 0);
    [[nodiscard]] Result pollAll(size_t& numPolled);

    [[nodiscard]] size_t getNumReadyOperations() const;
    [[nodiscard]] size_t getNumOperations() const;
    [[nodiscard]] size_t getNumRequestsInFlight() const;
    [[nodiscard]] bool   isOperationRegistered(HttpClientOperation& operation) const;
    [[nodiscard]] bool   hasRequestsInFlight() const;
    [[nodiscard]] bool   isInitialized() const { return initialized; }

  private:
    virtual void notifyHttpClientOperation(HttpClientOperation& operation) override;

    [[nodiscard]] size_t findOperationIndex(HttpClientOperation& operation) const;
    [[nodiscard]] bool   hasReadyOperationLocked() const;

    HttpClientOperationSchedulerMemory schedulerMemory;
    mutable HttpClientLocalMutex       readyMutex;
    HttpClientLocalConditionVariable   readyCV;
    bool                               initialized = false;
};

//! @}
} // namespace SC
