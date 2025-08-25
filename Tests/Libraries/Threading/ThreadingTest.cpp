// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Threading/Threading.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct ThreadingTest;
}

struct SC::ThreadingTest : public SC::TestCase
{
    inline void testThread();
    inline void testEventObject();
    inline void testMutex();
    inline void testRWLock();

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
        if (test_section("RWLock"))
        {
            testRWLock();
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

void SC::ThreadingTest::testRWLock()
{
    //! [rwlockSnippet]
    constexpr int numReaders    = 3;
    constexpr int numIterations = 100;

    RWLock rwlock;
    int    sharedData = 0;

    // Start multiple reader threads
    Thread readers[numReaders];
    for (int i = 0; i < numReaders; i++)
    {
        auto readerFunc = [&](Thread& thread)
        {
            thread.setThreadName(SC_NATIVE_STR("Reader"));
            for (int j = 0; j < numIterations; j++)
            {
                rwlock.lockRead();
                volatile int value = sharedData; // Prevent optimization
                (void)value;
                rwlock.unlockRead();
                Thread::Sleep(1); // Small delay to increase contention
            }
        };
        SC_TEST_EXPECT(readers[i].start(readerFunc));
    }

    // Start a writer thread
    Thread writer;
    auto   writerFunc = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("Writer"));
        for (int i = 0; i < numIterations; i++)
        {
            rwlock.lockWrite();
            sharedData++;
            rwlock.unlockWrite();
            Thread::Sleep(1); // Small delay to increase contention
        }
    };
    SC_TEST_EXPECT(writer.start(writerFunc));

    // Wait for all threads to finish
    for (int i = 0; i < numReaders; i++)
    {
        SC_TEST_EXPECT(readers[i].join());
    }
    SC_TEST_EXPECT(writer.join());
    SC_TEST_EXPECT(sharedData == numIterations);
    //! [rwlockSnippet]
}

namespace SC
{
void runThreadingTest(SC::TestReport& report) { ThreadingTest test(report); }
} // namespace SC
