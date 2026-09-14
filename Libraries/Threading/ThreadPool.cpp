// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "ThreadPool.h"
#include "../Common/Deferred.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <pthread.h>
#endif

struct SC::ThreadPool::WorkerThread
{
#if SC_PLATFORM_WINDOWS
    static DWORD WINAPI execute(void* arg)
#else
    static void* execute(void* arg)
#endif
    {
        ThreadPool& threadPool = *reinterpret_cast<ThreadPool*>(arg);
        for (;;)
        {
            // 1. Loop forever, trying to grab new tasks
            Task* task;
            if (not waitForAvailableTask(threadPool, task))
            {
                return 0; // Stop was requested, terminate the infinite loop (and so the thread)
            }

            // 2. Execute the task
            if (task != nullptr)
            {
                task->function();
            }

            // 3. Signal completion of the task
            signalCompletedTask(threadPool, task);
        }
    }

    /// @return true if the a new task is available, false if the thread was requested to stop
    [[nodiscard]] static bool waitForAvailableTask(ThreadPool& threadPool, Task*& task)
    {
        // Function is entirely protected by the mutex
        threadPool.poolMutex.lock();
        auto deferUnlock = MakeDeferred([&threadPool] { threadPool.poolMutex.unlock(); });

        // 1. Check for new task (or for the stop message)
        while (not threadPool.stopRequested and threadPool.taskHead == nullptr)
        {
            threadPool.taskAvailable.wait(threadPool.poolMutex);
        }

        // 2. If a stop has been requested let execute stop the infinite loop
        if (threadPool.stopRequested)
        {
            // Flag the task as completed to unblock waitForTask and return false to signal stopping the infinite loop
            threadPool.numWorkerThreads--;
            threadPool.taskCompleted.signal();
            return false;
        }

        // 3. Grab oldest task
        task = threadPool.taskHead;
        threadPool.numRunningTasks++;

        // 4. Update the head of the FIFO
        if (task)
        {
            if (task->next == nullptr)
            {
                // This was the last task available, now the FIFO is empty
                threadPool.taskHead = nullptr;
                threadPool.taskTail = nullptr;
            }
            else
            {
                threadPool.taskHead = task->next;
            }
        }
        return true;
    }

    static void signalCompletedTask(ThreadPool& threadPool, Task* task)
    {
        // Function is entirely protected by the mutex
        threadPool.poolMutex.lock();
        if (task)
        {
            task->threadPool = nullptr; // free the task
            task->next       = nullptr;
        }
        threadPool.numRunningTasks--;
        if (not threadPool.stopRequested) // stopRequested handling happens in waitForAvailableTask
        {
            threadPool.taskCompleted.signal();
        }
        threadPool.poolMutex.unlock();
    }
};

SC::ResultThreading SC::ThreadPool::create(size_t workerThreads)
{
    if (numWorkerThreads != 0)
        return ResultThreading(ThreadingError::ThreadPoolAlreadyCreated);
    if (workerThreads == 0)
        return ResultThreading(ThreadingError::InvalidWorkerThreadCount);

    // Creating threads and detaching them, as they will take care themselves of monitoring the incoming tasks.
    for (size_t idx = 0; idx < workerThreads; idx++)
    {
        // Not using SC::Thread to avoid needing to store Function memory
#if SC_PLATFORM_WINDOWS
        DWORD  threadID;
        HANDLE thread = ::CreateThread(0, 512 * 1024, &WorkerThread::execute, this, CREATE_SUSPENDED, &threadID);
        if (thread == nullptr)
        {
            return ResultThreading(ThreadingError::ThreadPoolThreadCreationFailed,
                                   static_cast<uint32_t>(::GetLastError()));
        }
        ::ResumeThread(thread);
        ::CloseHandle(thread);
#else
        pthread_t thread;
        const int res = ::pthread_create(&thread, nullptr, &WorkerThread::execute, this);
        if (res != 0)
        {
            return ResultThreading(ThreadingError::ThreadPoolThreadCreationFailed, static_cast<uint32_t>(res));
        }
        ::pthread_detach(thread);
#endif
    }
    numWorkerThreads = workerThreads;
    return ResultThreading(true);
}

SC::ResultThreading SC::ThreadPool::destroy()
{
    {
        poolMutex.lock();
        auto deferUnlock = MakeDeferred([this] { poolMutex.unlock(); });
        if (numWorkerThreads == 0)
        {
            return ResultThreading(true); // this was already destroyed
        }
        // 1. Free tasks that have not been executed yet
        while (taskHead)
        {
            Task* task           = taskHead->next;
            taskHead->next       = nullptr;
            taskHead->threadPool = nullptr;
            taskHead             = task;
        }
        taskTail = nullptr;

        // 2. Request all threads to stop
        stopRequested = true;
        taskAvailable.broadcast();
    }

    // 3. Wait for all tasks to stop
    ResultThreading res = waitForAllTasks();

    // 4. Reset the stop flag
    stopRequested = false;
    return res;
}

SC::ResultThreading SC::ThreadPool::waitForAllTasks()
{
    // Function is entirely protected by the mutex
    poolMutex.lock();
    auto deferUnlock = MakeDeferred([this] { poolMutex.unlock(); });
    if (numWorkerThreads == 0)
    {
        return ResultThreading(true);
    }
    for (;;)
    {
        const bool runningWithPendingTasks    = not stopRequested and (taskHead != nullptr or numRunningTasks != 0);
        const bool stoppingWithRunningThreads = stopRequested and numWorkerThreads != 0;
        if (runningWithPendingTasks or stoppingWithRunningThreads)
        {
            taskCompleted.wait(poolMutex);
        }
        else
        {
            break;
        }
    }
    return ResultThreading(true);
}

SC::ResultThreading SC::ThreadPool::waitForTask(Task& task)
{
    if (numWorkerThreads == 0)
        return ResultThreading(ThreadingError::ThreadPoolNotCreated);

    // Function is entirely protected by the mutex
    poolMutex.lock();
    auto deferUnlock = MakeDeferred([this] { poolMutex.unlock(); });

    for (;;)
    {
        if (task.threadPool == nullptr)
        {
            break; // The task being waited has been flagged as completed
        }
        if (taskHead != nullptr or numRunningTasks != 0)
        {
            taskCompleted.wait(poolMutex);
        }
        else
        {
            break; // all tasks have completed (including the task being waited)
        }
    }
    return ResultThreading(true);
}

SC::ResultThreading SC::ThreadPool::queueTask(Task& task)
{
    if (numWorkerThreads == 0)
        return ResultThreading(ThreadingError::ThreadPoolNotCreated);

    // Function is entirely protected by the mutex
    poolMutex.lock();
    auto deferUnlock = MakeDeferred([this] { poolMutex.unlock(); });

    if (task.threadPool == this)
        return ResultThreading(ThreadingError::TaskAlreadyQueued);
    if (task.threadPool != nullptr)
        return ResultThreading(ThreadingError::TaskInUseByAnotherThreadPool);

    task.threadPool = this;
    if (taskHead == nullptr)
    {
        // FIFO was empty, replace head and tail with the task
        taskHead = &task;
        taskTail = taskHead;
    }
    else
    {
        // FIFO was not empty, append task to the tail
        taskTail->next = &task;
        taskTail       = &task;
    }
    taskAvailable.broadcast();
    return ResultThreading(true);
}
