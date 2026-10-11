// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Threading/ThreadPool.h"
#include "Libraries/Common/PlacementNew.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct ThreadPoolTest;
}

struct SC::ThreadPoolTest : public SC::TestCase
{
    inline void testThreadPool();
    inline void testThreadPoolErrors();
    void        testReleaseNotification();

    ThreadPoolTest(SC::TestReport& report) : TestCase(report, "ThreadPoolTest")
    {
        if (test_section("ThreadPool"))
        {
            testThreadPool();
        }

        if (test_section("release notification can destroy and reuse task"))
            testReleaseNotification();

        if (test_section("ThreadPool errors"))
        {
            testThreadPoolErrors();
        }
    }
};

void SC::ThreadPoolTest::testThreadPool()
{
    //! [threadPoolSnippet]
    static const size_t wantedThreads = 4;
    static const size_t numTasks      = 100;

    // 1. Create the threadpool with the wanted number of threads
    SC::ThreadPool threadPool;
    SC_TEST_EXPECT(threadPool.create(wantedThreads));

    size_t values[numTasks];

    // 2. Allocate the wanted number of tasks. Tasks memory should be valid until a task is finished.
    SC::ThreadPool::Task tasks[numTasks];

    for (size_t idx = 0; idx < numTasks; idx++)
    {
        size_t* value = values + idx;
        *value        = idx;

        // 3. Setup the task function to execute on some random thread
        tasks[idx].function = [value]()
        {
            if (*value % 2)
            {
                SC::Thread::Sleep(10);
            }
            *value *= 100;
        };
        // 4. Queue the task in thread pool
        SC_TEST_EXPECT(threadPool.queueTask(tasks[idx]));
    }

    // 5. [Optional] Wait for a single task
    SC_TEST_EXPECT(threadPool.waitForTask(tasks[1]));
    SC_TEST_EXPECT(values[1] == 100);

    // 6. [Optional] Wait for all remaining tasks to be finished
    SC_TEST_EXPECT(threadPool.waitForAllTasks());

    // Checking Results
    bool allGood = true;
    for (size_t idx = 0; idx < numTasks; idx++)
    {
        allGood = allGood && (values[idx] == idx * 100);
    }
    SC_TEST_EXPECT(allGood);

    // 6. [Optional] Destroy the threadpool.
    // Note: destructor will wait for tasks to finish, but this avoids it from accessing invalid tasks,
    // as stack objects are reclaimed in inverse declaration order
    SC_TEST_EXPECT(threadPool.destroy());
    //! [threadPoolSnippet]
}

void SC::ThreadPoolTest::testReleaseNotification()
{
    using Task = ThreadPool::Task;
    alignas(Task) char storage[sizeof(Task)];
    auto*              task = reinterpret_cast<Task*>(storage);
    placementNew(*task);
    ThreadPool pool;
    SC_TEST_EXPECT(pool.create(1));
    struct State
    {
        ThreadPool* pool;
        Task*       task;
        int         bodies   = 0;
        int         releases = 0;
        Result      queued{false};
        uint64_t    worker = 0;
    } state{&pool, task};
    task->function     = [&state]() { ++state.bodies; };
    task->afterRelease = [&state]()
    {
        ++state.releases;
        state.worker = Thread::CurrentThreadID();
        // The callback runs from a local copy, after the pool's final access to the old Task object.
        state.task->~Task();
        placementNew(*state.task);
        state.task->function = [&state]() { ++state.bodies; };
        state.queued         = state.pool->queueTask(*state.task);
    };
    SC_TEST_EXPECT(pool.queueTask(*task));
    SC_TEST_EXPECT(pool.waitForAllTasks());
    SC_TEST_EXPECT(state.queued and state.bodies == 2 and state.releases == 1);
    SC_TEST_EXPECT(state.worker != Thread::CurrentThreadID());
    task->~Task();
    SC_TEST_EXPECT(pool.destroy());
}

void SC::ThreadPoolTest::testThreadPoolErrors()
{
    static const size_t numTasks = 4;

    // Define tasks before threadpool to avoid threadpool destructor accessing invalid tasks
    SC::ThreadPool::Task tasks[numTasks];

    SC::ThreadPool threadPool;
    Result         result = threadPool.create(0);
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::InvalidWorkerThreadCount));

    SC_TEST_EXPECT(threadPool.create(2));
    result = threadPool.create(2);
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::ThreadPoolAlreadyCreated));

    SC::ThreadPool threadPool2;
    SC_TEST_EXPECT(threadPool2.create(1));

    SC::ThreadPool uninitializedThreadPool;
    result = uninitializedThreadPool.queueTask(tasks[0]);
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::ThreadPoolNotCreated));
    result = uninitializedThreadPool.waitForTask(tasks[0]);
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::ThreadPoolNotCreated));

    for (size_t idx = 0; idx < numTasks; idx++)
    {

        tasks[idx].function = []() { SC::Thread::Sleep(100); };
        SC_TEST_EXPECT(threadPool.queueTask(tasks[idx]));
    }
    // Expect error if trying to add a task to another threadpool
    result = threadPool2.queueTask(tasks[1]);
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::TaskInUseByAnotherThreadPool));
    // Expect error if trying to queue a task again
    result = threadPool.queueTask(tasks[1]);
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::TaskAlreadyQueued));
}

namespace SC
{
void runThreadPoolTest(SC::TestReport& report) { ThreadPoolTest test(report); }
} // namespace SC
