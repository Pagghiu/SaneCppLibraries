// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Function.h"
#include "../../Foundation/Result.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
namespace SC
{
struct FSWAtomicBool
{
    volatile bool value = false;

    bool load() { return InterlockedOr(reinterpret_cast<LONG volatile*>(&value), 0) != 0; }
    bool exchange(bool desired) { return InterlockedExchange(reinterpret_cast<LONG volatile*>(&value), desired) != 0; }
};

struct FSWThread
{
    HANDLE thread = nullptr;

    Result start(LPTHREAD_START_ROUTINE func, LPVOID param)
    {
        thread = ::CreateThread(nullptr, 0, func, param, 0, nullptr);
        return thread ? Result(true) : Result::Error("CreateThread failed");
    }

    Result join()
    {
        if (!thread)
            return Result(true);
        DWORD res = ::WaitForSingleObject(thread, INFINITE);
        ::CloseHandle(thread);
        thread = nullptr;
        return res == WAIT_OBJECT_0 ? Result(true) : Result::Error("WaitForSingleObject failed");
    }

    bool wasStarted() const { return thread != nullptr; }

    void setThreadName(const wchar_t* name) { ::SetThreadDescription(::GetCurrentThread(), name); }
};
} // namespace SC
#else
#include <errno.h>   // errno
#include <pthread.h> // pthread_
#include <unistd.h>  // usleep
namespace SC
{
// Atomic bool for thread safety
struct FSWAtomicBool
{
    volatile bool value = false;

    bool load() const { return __atomic_load_n(&value, __ATOMIC_SEQ_CST); }
    void store(bool desired) { __atomic_store_n(&value, desired, __ATOMIC_SEQ_CST); }
    bool exchange(bool desired) { return __atomic_exchange_n(&value, desired, __ATOMIC_SEQ_CST); }
};

// Minimal Mutex wrapper
struct FSWMutex
{
    pthread_mutex_t mutex;

    FSWMutex() { ::pthread_mutex_init(&mutex, nullptr); }
    ~FSWMutex() { ::pthread_mutex_destroy(&mutex); }

    void lock() { ::pthread_mutex_lock(&mutex); }
    void unlock() { ::pthread_mutex_unlock(&mutex); }
};

// Minimal Condition Variable wrapper
struct FSWCondition
{
    pthread_cond_t cond;

    FSWCondition() { ::pthread_cond_init(&cond, nullptr); }
    ~FSWCondition() { ::pthread_cond_destroy(&cond); }

    void wait(FSWMutex& mutex) { ::pthread_cond_wait(&cond, &mutex.mutex); }
    void signal() { ::pthread_cond_signal(&cond); }
    void broadcast() { ::pthread_cond_broadcast(&cond); }
};

// Minimal Event Object
struct FSWEventObject
{
    FSWMutex     mutex;
    FSWCondition cond;

    bool signaled  = false;
    bool autoReset = true;

    void wait()
    {
        mutex.lock();
        while (not signaled)
        {
            cond.wait(mutex);
        }
        if (autoReset)
        {
            signaled = false;
        }
        mutex.unlock();
    }

    void signal()
    {
        mutex.lock();
        signaled = true;
        cond.signal();
        mutex.unlock();
    }
};

// Minimal Thread wrapper
struct FSWThread
{
    pthread_t                  thread = 0;
    Function<void(FSWThread&)> userFunction;

    static void* threadFunc(void* arg)
    {
        FSWThread& self = *static_cast<FSWThread*>(arg);
        self.userFunction(self);
        return 0;
    }

    Result start(Function<void(FSWThread&)> func)
    {
        SC_TRY_MSG(thread == 0, "Thread already started");
        userFunction  = move(func);
        const int res = pthread_create(&thread, nullptr, &FSWThread::threadFunc, this);
        SC_TRY_MSG(res == 0, "pthread_create error");
        return Result(true);
    }

    Result join()
    {
        if (thread != 0)
        {
            const int res = pthread_join(thread, nullptr);
            thread        = 0;
            SC_TRY_MSG(res == 0, "pthread_join error");
        }
        return Result(true);
    }

    bool wasStarted() const { return thread != 0; }

    void setThreadName(const char* name)
    {
#if SC_PLATFORM_APPLE
        pthread_setname_np(name);
#else
        pthread_setname_np(pthread_self(), name);
#endif
    }
};

} // namespace SC
#endif
