// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Function.h"
#include "../Foundation/Opaque.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Result.h"
#include "../Foundation/StringView.h"

namespace SC
{
struct Thread;
struct ConditionVariable;
struct Mutex;
struct EventObject;
} // namespace SC

struct SC::Mutex
{
    Mutex();
    ~Mutex();

    // Underlying OS primitives can't be copied or moved. If needed use new / smartptr
    Mutex(const Mutex&)            = delete;
    Mutex(Mutex&&)                 = delete;
    Mutex& operator=(const Mutex&) = delete;
    Mutex& operator=(Mutex&&)      = delete;

    void lock();
    void unlock();

#if SC_PLATFORM_APPLE
    static constexpr int OpaqueMutexSize      = 56 + sizeof(long);
    static constexpr int OpaqueMutexAlignment = alignof(long);
#elif SC_PLATFORM_WINDOWS
    static constexpr int OpaqueMutexSize      = 4 * sizeof(void*) + 2 * sizeof(long);
    static constexpr int OpaqueMutexAlignment = alignof(void*);
#elif SC_PLATFORM_EMSCRIPTEN
    static constexpr int OpaqueMutexSize      = sizeof(void*) * 6 + sizeof(long);
    static constexpr int OpaqueMutexAlignment = alignof(long);
#else
    static constexpr int OpaqueMutexSize      = sizeof(void*;
    static constexpr int OpaqueMutexAlignment = alignof(long);
#endif
    // Wrap native mutex as opaque array of bytes
    using OpaqueMutex = OpaqueHandle<OpaqueMutexSize, OpaqueMutexAlignment>;
    OpaqueMutex mutex;
};

struct SC::ConditionVariable
{
    ConditionVariable();
    ~ConditionVariable();

    // Underlying OS primitives can't be copied or moved. If needed use new / smartptr
    ConditionVariable(const ConditionVariable&)            = delete;
    ConditionVariable(ConditionVariable&&)                 = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;
    ConditionVariable& operator=(ConditionVariable&&)      = delete;

    void wait(Mutex& mutex);
    void signal();

#if SC_PLATFORM_APPLE
    static constexpr int OpaqueCVSize      = 40 + sizeof(long);
    static constexpr int OpaqueCVAlignment = alignof(long);
#elif SC_PLATFORM_WINDOWS
    static constexpr int OpaqueCVSize         = sizeof(void*);
    static constexpr int OpaqueCVAlignment    = alignof(void*);
#elif SC_PLATFORM_EMSCRIPTEN
    static constexpr int OpaqueCVSize         = sizeof(void*) * 12;
    static constexpr int OpaqueCVAlignment    = alignof(long);
#else
    static constexpr int OpaqueCVSize         = sizeof(void*);
    static constexpr int OpaqueCVAlignment    = alignof(long);
#endif

    //  Wrap native condition variable as opaque array of bytes
    using OpaqueConditionVariable = OpaqueHandle<OpaqueCVSize, OpaqueCVAlignment>;
    OpaqueConditionVariable condition;
};

struct SC::Thread
{
    Thread() = default;
    ~Thread();

    // Can me moved
    Thread(Thread&&)            = default;
    Thread& operator=(Thread&&) = default;

    // Cannot be copied
    Thread(const Thread&)            = delete;
    Thread& operator=(const Thread&) = delete;

    // Starts the new thread with given name and func
    // The passed in func will get copied on thread stack and destructed there
    [[nodiscard]] ReturnCode start(StringView threadName, Action&& func);
    [[nodiscard]] ReturnCode join();
    [[nodiscard]] ReturnCode detach();

    static void Sleep(uint32_t milliseconds);

  private:
    struct CreateParams;
    struct Internal;
    using OpaqueThread = OpaqueHandle<sizeof(void*), alignof(void*)>;
    UniqueOptional<OpaqueThread> thread;
};

struct SC::EventObject
{
    void wait();
    void signal();

  private:
    bool              isSignaled = false;
    Mutex             mutex;
    ConditionVariable cond;
};
