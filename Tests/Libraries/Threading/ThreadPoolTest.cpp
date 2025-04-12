// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Threading/ThreadPool.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct ThreadPoolTest;
}

struct SC::ThreadPoolTest : public SC::TestCase
{
    inline void testThreadPool();
    inline void testThreadPoolErrors();

    ThreadPoolTest(SC::TestReport& report) : TestCase(report, "ThreadPoolTest")
    {
        if (test_section("ThreadPool"))
        {
            testThreadPool();
        }

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

void SC::ThreadPoolTest::testThreadPoolErrors()
{
    static const size_t numTasks = 4;

    // Define tasks before threadpool to avoid threadpool destructor accessing invalid tasks
    SC::ThreadPool::Task tasks[numTasks];

    SC::ThreadPool threadPool;
    SC_TEST_EXPECT(threadPool.create(2));
    SC::ThreadPool threadPool2;
    SC_TEST_EXPECT(threadPool2.create(1));

    for (size_t idx = 0; idx < numTasks; idx++)
    {

        tasks[idx].function = []() { SC::Thread::Sleep(100); };
        SC_TEST_EXPECT(threadPool.queueTask(tasks[idx]));
    }
    // Expect error if trying to add a task to another threadpool
    SC_TEST_EXPECT(not threadPool2.queueTask(tasks[1]));
    // Expect error if trying to queue a task again
    SC_TEST_EXPECT(not threadPool.queueTask(tasks[1]));
}

namespace SC
{
void runThreadPoolTest(SC::TestReport& report) { ThreadPoolTest test(report); }
} // namespace SC
