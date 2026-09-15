// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/Function.h"
#include "../FileSystemWatcherError.h"

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

    ResultFileSystemWatcher start(LPTHREAD_START_ROUTINE func, LPVOID param)
    {
        thread = ::CreateThread(nullptr, 0, func, param, 0, nullptr);
        if (thread)
            return ResultFileSystemWatcher(true);
        const uint32_t nativeError = static_cast<uint32_t>(::GetLastError());
        return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                       FileSystemWatcherErrorDetail::WindowsCreateThread, nativeError);
    }

    ResultFileSystemWatcher join()
    {
        if (!thread)
            return ResultFileSystemWatcher(true);
        const DWORD    res         = ::WaitForSingleObject(thread, INFINITE);
        const uint32_t nativeError = res == WAIT_FAILED ? static_cast<uint32_t>(::GetLastError()) : 0;
        ::CloseHandle(thread);
        thread = nullptr;
        return res == WAIT_OBJECT_0
                   ? ResultFileSystemWatcher(true)
                   : ResultFileSystemWatcher(FileSystemWatcherError::StopWatchingFailed,
                                             FileSystemWatcherErrorDetail::WindowsWaitForSingleObject, nativeError);
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

    ResultFileSystemWatcher start(Function<void(FSWThread&)> func)
    {
        if (thread != 0)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::WorkerAlreadyStarted);
        userFunction  = move(func);
        const int res = pthread_create(&thread, nullptr, &FSWThread::threadFunc, this);
        if (res != 0)
            return ResultFileSystemWatcher(FileSystemWatcherError::WatchSetupFailed,
                                           FileSystemWatcherErrorDetail::PosixPthreadCreate,
                                           static_cast<uint32_t>(res));
        return ResultFileSystemWatcher(true);
    }

    ResultFileSystemWatcher join()
    {
        if (thread != 0)
        {
            const int res = pthread_join(thread, nullptr);
            thread        = 0;
            if (res != 0)
                return ResultFileSystemWatcher(FileSystemWatcherError::StopWatchingFailed,
                                               FileSystemWatcherErrorDetail::PosixPthreadJoin,
                                               static_cast<uint32_t>(res));
        }
        return ResultFileSystemWatcher(true);
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
