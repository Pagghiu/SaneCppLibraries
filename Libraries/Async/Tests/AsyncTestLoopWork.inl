// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"

void SC::AsyncTest::loopWork()
{
    //! [AsyncLoopWorkSnippet]
    // This test creates a thread pool with 4 thread and 16 AsyncLoopWork.
    // All the 16 AsyncLoopWork are scheduled to do some work on a background thread.
    // After work is done, their respective after-work callback is invoked on the event loop thread.

    static constexpr int NUM_THREADS = 4;
    static constexpr int NUM_WORKS   = NUM_THREADS * NUM_THREADS;

    ThreadPool threadPool;
    SC_TEST_EXPECT(threadPool.create(NUM_THREADS));

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    int numRequestsBase = 0;
    eventLoop.enumerateRequests([&](AsyncRequest&) { numRequestsBase++; });

    AsyncLoopWork works[NUM_WORKS];

    int         numAfterWorkCallbackCalls = 0;
    Atomic<int> numWorkCallbackCalls      = 0;

    for (int idx = 0; idx < NUM_WORKS; ++idx)
    {
        works[idx].work = [&]
        {
            // This work callback is called on some random threadPool thread
            Thread::Sleep(50);                 // Execute some work on the thread
            numWorkCallbackCalls.fetch_add(1); // Atomically increment this counter
            return Result(true);
        };
        works[idx].callback = [&](AsyncLoopWork::Result&)
        {
            // This after-work callback is invoked on the event loop thread.
            // More precisely this runs on the thread calling eventLoop.run().
            numAfterWorkCallbackCalls++; // No need for atomics here, callback is run inside loop thread
        };
        // Must always call setThreadPool at least once before start
        SC_TEST_EXPECT(works[idx].setThreadPool(threadPool));
        SC_TEST_EXPECT(works[idx].start(eventLoop));
    }
    int numRequests = 0;
    eventLoop.enumerateRequests([&](AsyncRequest&) { numRequests++; });
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(numRequests == NUM_WORKS + numRequestsBase);

    // Check that callbacks have been actually called
    SC_TEST_EXPECT(numWorkCallbackCalls.load() == NUM_WORKS);
    SC_TEST_EXPECT(numAfterWorkCallbackCalls == NUM_WORKS);
    //! [AsyncLoopWorkSnippet]
}
