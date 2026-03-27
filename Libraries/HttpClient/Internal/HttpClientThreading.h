// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Foundation/Compiler.h"
#include "../../Foundation/PrimitiveTypes.h"

namespace SC
{
/// Internal synchronization helpers duplicated in HttpClient to avoid a dependency on Threading.
struct SC_COMPILER_EXPORT HttpClientLocalMutex final
{
    HttpClientLocalMutex();
    ~HttpClientLocalMutex();

    HttpClientLocalMutex(const HttpClientLocalMutex&)            = delete;
    HttpClientLocalMutex(HttpClientLocalMutex&&)                 = delete;
    HttpClientLocalMutex& operator=(const HttpClientLocalMutex&) = delete;
    HttpClientLocalMutex& operator=(HttpClientLocalMutex&&)      = delete;

    void lock();
    void unlock();

  private:
    friend struct HttpClientLocalConditionVariable;
    alignas(uint64_t) char storage[128];
};

/// Internal synchronization helpers duplicated in HttpClient to avoid a dependency on Threading.
struct SC_COMPILER_EXPORT HttpClientLocalConditionVariable final
{
    HttpClientLocalConditionVariable();
    ~HttpClientLocalConditionVariable();

    HttpClientLocalConditionVariable(const HttpClientLocalConditionVariable&)            = delete;
    HttpClientLocalConditionVariable(HttpClientLocalConditionVariable&&)                 = delete;
    HttpClientLocalConditionVariable& operator=(const HttpClientLocalConditionVariable&) = delete;
    HttpClientLocalConditionVariable& operator=(HttpClientLocalConditionVariable&&)      = delete;

    void wait(HttpClientLocalMutex& mutex);
    bool wait(HttpClientLocalMutex& mutex, uint32_t timeoutMilliseconds);

    void signal();
    void broadcast();

  private:
    alignas(uint64_t) char storage[128];
};
} // namespace SC
