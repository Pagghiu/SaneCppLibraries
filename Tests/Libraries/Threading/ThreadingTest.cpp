// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Threading/Threading.h"
#include "Libraries/Common/TypeTraits.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Atomic.h"
#include "Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
struct ThreadingTest;

} // namespace SC

struct SC::ThreadingTest : public SC::TestCase
{
    inline void testThread();
    inline void testEventObject();
    inline void testMutex();
    inline void testRWLock();
    inline void testBarrier();
    inline void testSemaphore();
    inline void testErrorFormatter();

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
        if (test_section("Barrier"))
        {
            testBarrier();
        }
        if (test_section("Semaphore"))
        {
            testSemaphore();
        }
        if (test_section("Error formatter"))
        {
            testErrorFormatter();
        }
    }
};

void SC::ThreadingTest::testThread()
{
    bool   threadCalled = false;
    Thread defaultInit;
    Result result = defaultInit.join();
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::ThreadNotStarted));
    result = defaultInit.detach();
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::ThreadNotStarted));

    Function<void(Thread&)> invalidFunction;
    result = defaultInit.start(move(invalidFunction));
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::InvalidThreadFunction));

    Thread thread;
    auto   lambda = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("test thread"));
        threadCalled = true;
    };
    SC_TEST_EXPECT(thread.start(lambda));
    Function<void(Thread&)> secondStart = [](Thread&) {};
    result                              = thread.start(move(secondStart));
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::ThreadAlreadyStarted));
    SC_TEST_EXPECT(thread.threadID() != 0);
    SC_TEST_EXPECT(thread.join());
    SC_TEST_EXPECT(thread.threadID() == 0);
    result = thread.detach();
    SC_TEST_EXPECT(result.isError(ThreadingResultCategory, ThreadingError::ThreadNotStarted));
    SC_TEST_EXPECT(threadCalled);

    Atomic<int> atomicInt(0);

    auto lambdaDetach = [&](Thread& thread)
    {
        thread.setThreadName(SC_NATIVE_STR("detach thread"));
        atomicInt.exchange(1);
    };
    SC_TEST_EXPECT(thread.start(lambdaDetach));
    SC_TEST_EXPECT(thread.detach());
    SC_TEST_EXPECT(thread.threadID() == 0);
    while (atomicInt.load() == 0)
    {
        Thread::Sleep(1);
    }
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

void SC::ThreadingTest::testBarrier()
{
    //! [barrierSnippet]
    constexpr uint32_t numThreads          = 8;
    constexpr int      incrementsPerThread = 1000;

    Thread threads[numThreads];

    Barrier barrier(numThreads);
    struct Context
    {
        Barrier&        barrier;
        Atomic<int32_t> sharedCounter;
    } ctx = {barrier, 0};

    for (uint32_t i = 0; i < numThreads; i++)
    {
        auto threadFunc = [this, &ctx](Thread& thread)
        {
            thread.setThreadName(SC_NATIVE_STR("Barrier"));

            // Phase 1: Each thread increments the counter
            for (int j = 0; j < incrementsPerThread; ++j)
            {
                ctx.sharedCounter++;
            }
            ctx.barrier.wait();

            // Phase 2: All threads should see the final value
            SC_TEST_EXPECT(ctx.sharedCounter == numThreads * incrementsPerThread);
            ctx.barrier.wait();
        };
        SC_TEST_EXPECT(threads[i].start(threadFunc));
    }

    // Wait for all threads to finish
    for (uint32_t i = 0; i < numThreads; i++)
    {
        SC_TEST_EXPECT(threads[i].join());
    }
    //! [barrierSnippet]
}

void SC::ThreadingTest::testSemaphore()
{
    //! [semaphoreSnippet]
    constexpr int maxResources        = 2; // Only 2 threads can access resource at once
    constexpr int numThreads          = 4; // Total number of threads trying to access
    constexpr int operationsPerThread = 3; // Each thread will do 3 operations

    Semaphore semaphore(maxResources); // Initialize with 2 available resources
    struct Context
    {
        Semaphore& semaphore;
        Mutex      counterMutex;       // To protect sharedResource counter
        int        sharedResource = 0; // Counter to verify correct synchronization
    } ctx{semaphore, {}};

    Thread threads[numThreads];
    for (int i = 0; i < numThreads; i++)
    {
        auto threadFunc = [this, &ctx](Thread& thread)
        {
            thread.setThreadName(SC_NATIVE_STR("Worker Thread"));
            for (int j = 0; j < operationsPerThread; j++)
            {
                ctx.semaphore.acquire(); // Wait for resource to be available

                // Critical section
                ctx.counterMutex.lock();
                ctx.sharedResource++;
                SC_TEST_EXPECT(ctx.sharedResource <= maxResources); // Never more than maxResources threads
                Thread::Sleep(1);                                   // Simulate some work
                ctx.sharedResource--;
                ctx.counterMutex.unlock();

                ctx.semaphore.release(); // Release the resource
                Thread::Sleep(1);        // Give other threads a chance
            }
        };
        SC_TEST_EXPECT(threads[i].start(threadFunc));
    }

    // Wait for all threads to finish
    for (int i = 0; i < numThreads; i++)
    {
        SC_TEST_EXPECT(threads[i].join());
    }

    // Verify final state
    SC_TEST_EXPECT(ctx.sharedResource == 0);
    //! [semaphoreSnippet]
}

void SC::ThreadingTest::testErrorFormatter()
{
    static_assert(sizeof(ResultThreading) == 16, "ResultThreading must remain 16 bytes");
    static_assert(__is_standard_layout(ResultThreading), "ResultThreading must remain standard-layout");
    static_assert(TypeTraits::IsTriviallyCopyable<ResultThreading>::value,
                  "ResultThreading must remain trivially copyable");

    ResultErrorFormat formatted = formatThreadingError(ThreadingError::ThreadNotStarted, {});
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::InsufficientCapacity);
    const size_t requiredCapacity = formatted.requiredCapacity;
    SC_TEST_EXPECT(requiredCapacity > 1 and requiredCapacity <= 64);

    char undersized[4] = {'x', 'x', 'x', '\0'};
    formatted          = formatThreadingError(ThreadingError::ThreadNotStarted, undersized);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::InsufficientCapacity);
    SC_TEST_EXPECT(formatted.requiredCapacity == requiredCapacity);
    SC_TEST_EXPECT(undersized[0] == '\0');

    char         exact[64];
    const size_t exactCapacity = requiredCapacity <= sizeof(exact) ? requiredCapacity : sizeof(exact);
    formatted = formatThreadingError(Result::Error(ThreadingResultCategory, ThreadingError::ThreadNotStarted),
                                     Span<char>{exact, exactCapacity});
    SC_TEST_EXPECT(formatted);
    SC_TEST_EXPECT(formatted.requiredCapacity == requiredCapacity);
    if (exactCapacity > 0)
        SC_TEST_EXPECT(exact[exactCapacity - 1] == '\0');

    exact[0]  = 'x';
    formatted = formatThreadingError(Result(true), exact);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::NotAnError);
    SC_TEST_EXPECT(formatted.requiredCapacity == 0);
    SC_TEST_EXPECT(exact[0] == '\0');

    formatted = formatThreadingError(Result::Error(ResultCategory(1234), 1), exact);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::ForeignCategory);
    SC_TEST_EXPECT(exact[0] == '\0');

    formatted = formatThreadingError(Result::Error(ThreadingResultCategory, 9999), exact);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::UnknownError);
    SC_TEST_EXPECT(exact[0] == '\0');

    ResultThreading detailed(ThreadingError::ThreadCreationFailed, ThreadingErrorDetail::PosixPthreadCreate, 12345);
    char            nativeMessage[128];
    formatted = formatThreadingError(detailed, nativeMessage);
    SC_TEST_EXPECT(formatted);
    SC_TEST_EXPECT(formatted.requiredCapacity <= sizeof(nativeMessage));
    SC_TEST_EXPECT(StringView::fromNullTerminated(nativeMessage, StringEncoding::Ascii).containsString("12345"));
    SC_TEST_EXPECT(detailed.isError(ThreadingError::ThreadCreationFailed));
    SC_TEST_EXPECT(detailed.detail == ThreadingErrorDetail::PosixPthreadCreate);

    ResultThreading unknownDetail(ThreadingError::ThreadCreationFailed, static_cast<ThreadingErrorDetail>(999), 12345);
    formatted = formatThreadingError(unknownDetail, nativeMessage);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::UnknownError);
    SC_TEST_EXPECT(nativeMessage[0] == '\0');

    const Result plain = detailed;
    SC_TEST_EXPECT(plain.isError(ThreadingResultCategory, ThreadingError::ThreadCreationFailed));

    const ResultThreading foreign(Result::Error(ResultCategory(1234), 7));
    SC_TEST_EXPECT(not foreign);
    SC_TEST_EXPECT(foreign.nativeError == 0);
    SC_TEST_EXPECT(foreign.detail == ThreadingErrorDetail::None);
    formatted = formatThreadingError(foreign, nativeMessage);
    SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::ForeignCategory);
}

namespace SC
{
void runThreadingTest(SC::TestReport& report) { ThreadingTest test(report); }
} // namespace SC
