// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/HttpClient/Internal/HttpClientThreading.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Atomic.h"
#include "Libraries/Threading/Threading.h"

namespace SC
{
struct HttpClientThreadingTest;
}

struct SC::HttpClientThreadingTest : public SC::TestCase
{
    HttpClientThreadingTest(SC::TestReport& report) : TestCase(report, "HttpClientThreadingTest")
    {
        if (test_section("unsignaled timeout"))
        {
            HttpClientLocalMutex mutex;

            HttpClientLocalConditionVariable condition;

            bool timedOut = false;
            mutex.lock();
            // A condition variable may wake spuriously; require at least one actual timeout.
            for (int attempt = 0; attempt < 32 and not timedOut; ++attempt)
                timedOut = not condition.wait(mutex, 0);
            mutex.unlock();
            SC_TEST_EXPECT(timedOut);
        }
        if (test_section("signal"))
            wakeThreads(false);
        if (test_section("broadcast"))
            wakeThreads(true);
    }

    void wakeThreads(bool broadcast)
    {
        struct State
        {
            HttpClientLocalMutex mutex;

            HttpClientLocalConditionVariable condition;

            EventObject  ready[2];
            Atomic<bool> release = false;

            bool timedOut[2] = {};
        } state;
        struct Worker
        {
            State* state = nullptr;
            int    index = 0;

            void run(Thread&)
            {
                state->mutex.lock();
                state->ready[index].signal();
                while (not state->release.load())
                {
                    if (not state->condition.wait(state->mutex, 2000))
                    {
                        state->timedOut[index] = true;
                        break;
                    }
                }
                state->mutex.unlock();
            }
        } workers[2];
        Thread threads[2];

        const int count   = broadcast ? 2 : 1;
        int       started = 0;
        for (int idx = 0; idx < count; ++idx)
        {
            workers[idx].state = &state;
            workers[idx].index = idx;

            Worker* worker = &workers[idx];

            const Result result = threads[idx].start([worker](Thread& thread) { worker->run(thread); });
            SC_TEST_EXPECT(result);
            if (not result)
                break;
            ++started;
        }
        for (int idx = 0; idx < started; ++idx)
            state.ready[idx].wait();
        state.mutex.lock();
        state.release.store(true);
        if (broadcast)
            state.condition.broadcast();
        else
            state.condition.signal();
        state.mutex.unlock();
        for (int idx = 0; idx < started; ++idx)
        {
            SC_TEST_EXPECT(threads[idx].join());
            SC_TEST_EXPECT(not state.timedOut[idx]);
        }
    }
};

namespace SC
{
void runHttpClientThreadingTest(TestReport& report) { HttpClientThreadingTest test(report); }
} // namespace SC
