// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/AlignedStorage.h"
#include "../Foundation/Function.h"
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"
#include "Internal/Optional.h" // UniqueOptional

namespace SC
{
struct Thread;
struct ConditionVariable;
struct Mutex;
struct EventObject;
} // namespace SC

//! @defgroup group_threading Threading
//! @copybrief library_threading (see @ref library_threading library page for more details)

//! @addtogroup group_threading
//! @{

/// @brief A native OS Mutex
struct SC::Mutex
{
    Mutex();
    ~Mutex();

    // Underlying OS primitives can't be copied or moved
    Mutex(const Mutex&)            = delete;
    Mutex(Mutex&&)                 = delete;
    Mutex& operator=(const Mutex&) = delete;
    Mutex& operator=(Mutex&&)      = delete;

    void lock();
    void unlock();

  private:
    friend struct ConditionVariable;
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
    using OpaqueMutex = AlignedStorage<OpaqueMutexSize, OpaqueMutexAlignment>;
    OpaqueMutex mutex;
};

/// @brief A native OS condition variable
struct SC::ConditionVariable
{
    ConditionVariable();
    ~ConditionVariable();

    // Underlying OS primitives can't be copied or moved
    ConditionVariable(const ConditionVariable&)            = delete;
    ConditionVariable(ConditionVariable&&)                 = delete;
    ConditionVariable& operator=(const ConditionVariable&) = delete;
    ConditionVariable& operator=(ConditionVariable&&)      = delete;

    void wait(Mutex& mutex);
    void signal();

  private:
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
    using OpaqueConditionVariable = AlignedStorage<OpaqueCVSize, OpaqueCVAlignment>;
    OpaqueConditionVariable condition;
};

/// @brief A native OS thread
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

    /// @brief Returns current thread identifier
    /// @return thread id
    static uint64_t CurrentThreadID();

    /// Starts the new thread with given name and func
    /// @param threadName Name given to the thread being started
    /// @param func     Function running on thread. Must be a valid pointer to action for the entire duration of thread.
    /// @param syncFunc Function garanteed to be run before  start returns
    [[nodiscard]] Result start(StringView threadName, Action* func, Action* syncFunc = nullptr);

    /// @brief Waits for thread to finish and releases its resources
    /// @return Valid Result if thread has finished
    [[nodiscard]] Result join();

    /// @brief Detaches the thread so that its resources are automatically released back to the system without
    /// Thread::join
    /// @return Valid Result if thread has been detached
    [[nodiscard]] Result detach();

    /// @brief Check if thread has been started
    /// @return `true` if thread has been started
    [[nodiscard]] bool wasStarted() const;

    /// @brief Puts current thread to sleep
    /// @param milliseconds Sleep for given number of milliseconds
    static void Sleep(uint32_t milliseconds);

  private:
    struct CreateParams;
    struct Internal;
    using OpaqueThread = AlignedStorage<sizeof(void*), alignof(void*)>;
    UniqueOptional<OpaqueThread> thread;
};

/// @brief An automatically reset event object to synchonize two threads
struct SC::EventObject
{
    bool autoReset = true;

    /// @brief Waits on a thread for EventObject::signal to be called from another thread
    void wait();

    /// @brief Unblocks another thread, waiting on EventObject::wait
    void signal();

  private:
    bool  isSignaled = false;
    Mutex mutex;

    ConditionVariable cond;
};

//! @}
