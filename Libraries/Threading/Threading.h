// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/AlignedStorage.h"
#include "../Foundation/Function.h"
#include "../Foundation/Result.h"
#include "Internal/Optional.h" // UniqueOptional

namespace SC
{
struct Thread;
struct ConditionVariable;
struct Mutex;
struct RWLock;
struct EventObject;
} // namespace SC

//! @defgroup group_threading Threading
//! @copybrief library_threading (see @ref library_threading library page for more details)

//! @addtogroup group_threading
//! @{

/// @brief A native OS mutex to synchronize access to shared resources.
///
/// Example:
/// @snippet Tests/Libraries/Threading/ThreadingTest.cpp mutexSnippet
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
    static constexpr int OpaqueMutexSize      = sizeof(void*) * 6;
    static constexpr int OpaqueMutexAlignment = alignof(long);
#endif
    // Wrap native mutex as opaque array of bytes
    using OpaqueMutex = AlignedStorage<OpaqueMutexSize, OpaqueMutexAlignment>;
    OpaqueMutex mutex;
};

/// @brief A native OS condition variable.
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
    void broadcast();

  private:
#if SC_PLATFORM_APPLE
    static constexpr int OpaqueCVSize      = 40 + sizeof(long);
    static constexpr int OpaqueCVAlignment = alignof(long);
#elif SC_PLATFORM_WINDOWS
    static constexpr int OpaqueCVSize      = sizeof(void*);
    static constexpr int OpaqueCVAlignment = alignof(void*);
#elif SC_PLATFORM_EMSCRIPTEN
    static constexpr int OpaqueCVSize      = sizeof(void*) * 12;
    static constexpr int OpaqueCVAlignment = alignof(long);
#else
    static constexpr int OpaqueCVSize      = sizeof(void*) * 6;
    static constexpr int OpaqueCVAlignment = alignof(long);
#endif

    //  Wrap native condition variable as opaque array of bytes
    using OpaqueConditionVariable = AlignedStorage<OpaqueCVSize, OpaqueCVAlignment>;
    OpaqueConditionVariable condition;
};

/// @brief A native OS thread.
///
/// Example:
/// @code{.cpp}
/// Thread thread;
/// thread.start([](Thread& thread)
/// {
///     // It's highly recommended setting a name for the thread
///     thread.setThreadName(SC_NATIVE_STR("My Thread"));
///     // Do something on the thread
///     Thread::Sleep(1000); // Sleep for 1 second
/// });
/// thread.join(); // wait until thread has finished executing
///
/// // ...or
///
/// thread.detach(); // To keep thread running after Thread destructor
/// @endcode
///
/// @warning Thread destructor will assert if SC::Thread::detach() or SC::Thread::join() has not been called.
struct SC::Thread
{
    Thread() = default;
    ~Thread();

    // Cannot be copied or moved (as it would require a dynamic allocation for the type erased Function)
    Thread(Thread&&)                 = delete;
    Thread& operator=(Thread&&)      = delete;
    Thread(const Thread&)            = delete;
    Thread& operator=(const Thread&) = delete;

    /// @brief Returns thread id of the thread calling the function
    /// @return thread id
    static uint64_t CurrentThreadID();

    /// @brief Returns thread id of this thread object (not current thread)
    uint64_t threadID();

    /// @brief Starts the new thread with given name and func
    /// @param func     Function running on thread. Must be a valid pointer to action for the entire duration of thread.
    [[nodiscard]] Result start(Function<void(Thread&)>&& func);

    /// @brief Sets current thread name ONLY if called from inside the thread.
    /// @param name The name of the thread
    /// @warning This function will ASSERT if it's not called from the thread itself.
    void setThreadName(const native_char_t* name);

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
    void setThreadNameInternal(const native_char_t* name);
    struct Internal;
    using OpaqueThread = AlignedStorage<sizeof(void*), alignof(void*)>;
    UniqueOptional<OpaqueThread> thread;
    Function<void(Thread&)>      userFunction;
};

/// @brief A Read-Write lock that allows multiple concurrent readers but only one writer
///
/// Example:
/// @snippet Tests/Libraries/Threading/ThreadingTest.cpp rwlockSnippet
struct SC::RWLock
{
    RWLock()  = default;
    ~RWLock() = default;

    RWLock(const RWLock&)            = delete;
    RWLock(RWLock&&)                 = delete;
    RWLock& operator=(const RWLock&) = delete;
    RWLock& operator=(RWLock&&)      = delete;

    /// @brief Acquire a read lock. Multiple readers can hold the lock concurrently.
    void lockRead();

    /// @brief Release a previously acquired read lock
    void unlockRead();

    /// @brief Acquire a write lock. Only one writer can hold the lock, and no readers can hold it simultaneously.
    void lockWrite();

    /// @brief Release a previously acquired write lock
    void unlockWrite();

  private:
    Mutex mutex;

    ConditionVariable readersQ;
    ConditionVariable writersQ;

    int  activeReaders  = 0;
    int  waitingWriters = 0;
    bool writerActive   = false;
};

/// @brief An automatically reset event object to synchronize two threads.
/// @n
/// Example:
/// @snippet Tests/Libraries/Threading/ThreadingTest.cpp eventObjectSnippet
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
