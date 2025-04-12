// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Threading.h"

namespace SC
{
struct ThreadPool;
struct ThreadPoolTask;
} // namespace SC

//! @addtogroup group_threading
//! @{

/// @brief A small task containing a function to execute that can be queued in the thread pool.
///
/// Fill Task::function with any function to execute in the thread pool.
struct SC::ThreadPoolTask
{

    Function<void()> function; ///< Function that will be executed during the task
  private:
    friend struct ThreadPool;
    ThreadPool*     threadPool = nullptr;
    ThreadPoolTask* next       = nullptr;
};

/// @brief Simple thread pool that executes tasks in a fixed number of worker threads.
///
/// This class is not copyable / moveable due to it containing Mutex and Condition variable.
/// Additionally, this class does not allocate any memory by itself, and expects the caller to supply
/// SC::ThreadPool::Task objects.
///
/// @warning The caller is responsible of keeping Task address stable until the it will be completed.
/// If it's not already completed the task must still be valid during ThreadPool::destroy or ThreadPool destructor.
///
/// Example:
/// @snippet Tests/Libraries/Threading/ThreadPoolTest.cpp threadPoolSnippet
///
struct SC::ThreadPool
{
    using Task = ThreadPoolTask;

    ThreadPool() = default;
    ~ThreadPool() { (void)destroy(); }

    /// @brief Create a thread pool with the requested number of worker threads
    [[nodiscard]] Result create(size_t workerThreads);

    /// @brief Destroy the thread pool created previously with ThreadPool::create
    /// @warning Tasks that are queued will NOT be executed (but you can use ThreadPool::waitForAllTasks for that)
    [[nodiscard]] Result destroy();

    /// @brief Queue a task (that should not be already in use)
    [[nodiscard]] Result queueTask(Task& task);

    /// @brief Blocks execution until all queued and pending tasks will be fully completed
    [[nodiscard]] Result waitForAllTasks();

    /// @brief Blocks execution until all queued and pending tasks will be fully completed
    [[nodiscard]] Result waitForTask(Task& task);

  private:
    Task* taskHead = nullptr; // Head of the FIFO linked list containing all threads
    Task* taskTail = nullptr; // Tail of the FIFO linked list containing all threads

    size_t numRunningTasks  = 0; // How many tasks are running in this moment
    size_t numWorkerThreads = 0; // How many worker threads exist in this pool (== 0 means uninitialized threadpool)

    Mutex             poolMutex;     // Protects all ThreadPool members access from worker threads
    ConditionVariable taskAvailable; // Signals to worker threads that there is a new queued task available
    ConditionVariable taskCompleted; // Signals to threadpool that there is a new task that was completed

    bool stopRequested = false; // Signals background threads to end their infinite task processing loop

    struct WorkerThread;
};

//! @}
