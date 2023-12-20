// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Threading.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct ThreadingTest;
}

struct SC::ThreadingTest : public SC::TestCase
{
    inline void testThread();
    inline void testEventObject();
    inline void testMutex();

    ThreadingTest(SC::TestReport& report) : TestCase(report, "ThreadingTest")
    {
        if (test_section("Thread"))
        {
            testThread();
        }
        if (test_section("EventObject"))
        {
            testEventObject();
        }
        if (test_section("Mutex"))
        {
            testMutex();
        }
    }
};

void SC::ThreadingTest::testThread()
{
    bool   threadCalled = false;
    Thread defaultInit;
    SC_TEST_EXPECT(not defaultInit.join());
    SC_TEST_EXPECT(not defaultInit.detach());
    Thread thread;
    auto   lambda = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test thread"));
        threadCalled = true;
    };
    SC_TEST_EXPECT(thread.start(lambda));
    SC_TEST_EXPECT(thread.join());
    SC_TEST_EXPECT(not thread.detach());
    SC_TEST_EXPECT(threadCalled);
}

void SC::ThreadingTest::testEventObject()
{
    //! [eventObjectSnippet]
    EventObject eventObject;

    Thread threadWaiting;
    auto   waitingFunc = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("Thread waiting"));
        eventObject.wait();
        report.console.printLine("After waiting");
    };
    SC_TEST_EXPECT(threadWaiting.start(waitingFunc));

    Thread threadSignaling;
    auto   signalingFunc = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("Signaling thread"));
        report.console.printLine("Signal");
        eventObject.signal();
    };
    SC_TEST_EXPECT(threadSignaling.start(signalingFunc));
    SC_TEST_EXPECT(threadWaiting.join());
    SC_TEST_EXPECT(threadSignaling.join());
    // Prints:
    // Signal
    // After waiting
    //! [eventObjectSnippet]
}

void SC::ThreadingTest::testMutex()
{
    //! [mutexSnippet]
    Mutex  mutex;
    int    globalVariable = 0;
    Thread thread1;
    auto   thread1Func = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("Thread1"));
        mutex.lock();
        globalVariable++;
        mutex.unlock();
    };
    SC_TEST_EXPECT(thread1.start(thread1Func));

    Thread thread2;
    auto   thread2Func = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("Signaling2"));
        mutex.lock();
        globalVariable++;
        mutex.unlock();
    };
    SC_TEST_EXPECT(thread2.start(thread2Func));
    SC_TEST_EXPECT(thread1.join());
    SC_TEST_EXPECT(thread2.join());
    SC_TEST_EXPECT(globalVariable == 2);
    //! [mutexSnippet]
}

namespace SC
{
void runThreadingTest(SC::TestReport& report) { ThreadingTest test(report); }
} // namespace SC
