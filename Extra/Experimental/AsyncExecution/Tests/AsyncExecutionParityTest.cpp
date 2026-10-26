// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Extra/Experimental/AsyncExecution/Tests/AsyncExecutionTestSupport.h"
#include "Libraries/AsyncFibers/AsyncFibers.h"
#include "Libraries/Await/Await.h"
#include "Libraries/Threading/Threading.h"
#include <stdio.h>
namespace SC
{
namespace
{
struct AsyncExecutionParityTest : AsyncExecutionTestCase
{
    Result nativeError{false};
    Mutex  expectationsMutex;

    bool recordExpectation(StringSpan expression, bool status, StringSpan details = {})
    {
        expectationsMutex.lock();
        const bool recorded = TestCase::recordExpectation(expression, status, details);
        expectationsMutex.unlock();
        return recorded;
    }

    bool recordExpectation(StringSpan expression, Result status)
    {
        expectationsMutex.lock();
        const bool recorded = TestCase::recordExpectation(expression, status);
        expectationsMutex.unlock();
        return recorded;
    }

    struct Fixture
    {
        AsyncEventLoop    loop;
        SocketDescriptor  refusal, socket;
        SocketIPAddress   address;
        AsyncLoopTimeout  watchdog;
        bool              expired = false;
        uint64_t          memory[1024];
        AsyncKernelEvents events;
        Result            create(uint16_t port)
        {
            SC_TRY(loop.create());
            SC_TRY(address.fromAddressPort("127.0.0.1", port));
            SC_TRY(refusal.create(address.getAddressFamily()));
            SC_TRY(SocketServer(refusal).bind(address));
            // A bound-but-not-listening port drops SYNs on macOS. Release the reserved test port to get refusal.
            SC_TRY(refusal.close());
            SC_TRY(loop.createAsyncTCPSocket(address.getAddressFamily(), socket));
            events.eventsMemory = {reinterpret_cast<uint8_t*>(memory), sizeof(memory)};
            watchdog.callback   = [this](AsyncLoopTimeout::Result&) { expired = true; };
            return watchdog.start(loop, {10000});
        }
        Result collect()
        {
            SC_TRY(loop.submitRequests(events));
            return loop.blockingPoll(events);
        }
        Result dispatch() { return loop.dispatchCompletions(events); }
        Result close()
        {
            if (not watchdog.isFree())
                SC_TRY(watchdog.unschedule(loop));
            SC_TRY(socket.close());
            SC_TRY(refusal.close());
            return loop.close();
        }
    };
    AsyncExecutionParityTest(TestReport& report) : AsyncExecutionTestCase(report, "AsyncExecutionParityTest")
    {
        if (test_section("native connect failure ready before cancellation delivery"))
        {
            callbacks(false);
            // Publish the reference error first. The remaining cases only read it and own all of their I/O state.
            struct Scenario
            {
                AsyncExecutionParityTest* test;
                void (AsyncExecutionParityTest::*run)(bool);
                bool cancel;
            } scenarios[] = {
                {this, &AsyncExecutionParityTest::callbacks, true},
                {this, &AsyncExecutionParityTest::senders, false},
                {this, &AsyncExecutionParityTest::senders, true},
                {this, &AsyncExecutionParityTest::coroutines, false},
                {this, &AsyncExecutionParityTest::coroutines, true},
                {this, &AsyncExecutionParityTest::fibers, false},
                {this, &AsyncExecutionParityTest::fibers, true},
            };
            constexpr size_t numScenarios = sizeof(scenarios) / sizeof(scenarios[0]);

            Thread workers[numScenarios];
            for (size_t index = 0; index < numScenarios; ++index)
            {
                Scenario* scenario = &scenarios[index];
                SC_TEST_EXPECT(
                    workers[index].start([scenario](Thread&) { (scenario->test->*scenario->run)(scenario->cancel); }));
            }
            for (Thread& worker : workers)
            {
                if (worker.wasStarted())
                    SC_TEST_EXPECT(worker.join());
            }
        }
    }
    void callbacks(bool cancel)
    {
        Fixture f;
        SC_TEST_EXPECT(f.create(report.mapPort(cancel ? 5112 : 5111)));
        AsyncSocketConnect request;
        int                values = 0, stopped = 0;
        Result             status{true};
        request.callback = [&](AsyncSocketConnect::Result& result)
        {
            ++values;
            status = result.isValid();
        };
        SC_TEST_EXPECT(request.start(f.loop, f.socket, f.address));
        SC_TEST_EXPECT(f.collect());
        SC_TEST_EXPECT(values == 0);
        Function<void(AsyncResult&)> afterStop = [&](AsyncResult&) { ++stopped; };
        if (cancel)
            SC_TEST_EXPECT(request.stop(f.loop, &afterStop));
        SC_TEST_EXPECT(f.dispatch());
        while (values + stopped == 0 and not f.expired)
            SC_TEST_EXPECT(f.loop.runNoWait());
        if (f.expired or not request.isFree())
            fprintf(stderr, "callback cancel=%d values=%d stopped=%d expired=%d free=%d status=%u/%u\n", int(cancel),
                    values, stopped, int(f.expired), int(request.isFree()), status.category().value,
                    status.errorValue());
        SC_TEST_EXPECT(not f.expired and request.isFree());
        if (cancel)
            SC_TEST_EXPECT(stopped == 1 and values == 0);
        else
        {
            SC_TEST_EXPECT(values == 1 and not status);
            nativeError = status;
        }
        SC_TEST_EXPECT(f.close());
    }
    void senders(bool cancel)
    {
        Fixture f;
        SC_TEST_EXPECT(f.create(report.mapPort(cancel ? 5114 : 5113)));
        AsyncExecutionContext context(f.loop);
        SC_TEST_EXPECT(context.create());
        ExecutionStopSource stop;
        ExecutionOutcome    outcome;
        auto                operation =
            ex::connect(context.connect(f.socket, f.address), ExecutionReceiver{outcome, stop.get_token()});
        ex::start(operation);
        // Deliver the command wake-up only. The connect request is queued by that callback for the next submit phase.
        SC_TEST_EXPECT(f.collect());
        SC_TEST_EXPECT(f.dispatch());
        SC_TEST_EXPECT(not operation.request.isFree() and outcome.errors == 0);
        SC_TEST_EXPECT(f.collect());
        SC_TEST_EXPECT(outcome.errors == 0);
        if (cancel)
            stop.request_stop(); // The stop flag is set; its owner command is not part of the already-collected batch.
        SC_TEST_EXPECT(f.dispatch());
        SC_TEST_EXPECT(not f.expired and outcome.errors == 1 and outcome.stopped == 0);
        SC_TEST_EXPECT(outcome.error.isError(nativeError.category(), nativeError.errorValue()));
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(f.close());
    }
    static AwaitTask connectTask(AwaitEventLoop& io, Fixture& f) { co_return co_await io.connect(f.socket, f.address); }
    void             coroutines(bool cancel)
    {
        Fixture f;
        SC_TEST_EXPECT(f.create(report.mapPort(cancel ? 5116 : 5115)));
        char           memory[64 * 1024];
        AwaitAllocator allocator;
        SC_TEST_EXPECT(allocator.createFixed(memory));
        AwaitEventLoop io(f.loop, allocator);
        AwaitTask      task = connectTask(io, f);
        SC_TEST_EXPECT(task.isValid());
        SC_TEST_EXPECT(io.spawn(task));
        SC_TEST_EXPECT(f.collect());
        SC_TEST_EXPECT(not task.isCompleted());
        if (cancel)
            SC_TEST_EXPECT(task.cancel(io));
        SC_TEST_EXPECT(f.dispatch());
        while (not task.isCompleted() and not f.expired)
            SC_TEST_EXPECT(io.runNoWait());
        SC_TEST_EXPECT(task.isCompleted() and not f.expired);
        if (cancel)
            SC_TEST_EXPECT(AwaitIsCancelled(task.result()));
        else
            SC_TEST_EXPECT(task.result().isError(nativeError.category(), nativeError.errorValue()));
        SC_TEST_EXPECT(f.close());
    }
    void fibers(bool cancel)
    {
        Fixture f;
        SC_TEST_EXPECT(f.create(report.mapPort(cancel ? 5118 : 5117)));
        FiberScheduler scheduler;
        AsyncFiberIO   io(scheduler, f.loop);
        FiberTask      task;
        char           storage[64 * 1024];
        FiberStack     stack{storage};
        SC_TEST_EXPECT(scheduler.spawn(
            task, stack, FiberTask::Procedure([&](FiberScheduler&) { return io.connect(f.socket, f.address); })));
        SC_TEST_EXPECT(scheduler.runReadyFibers());
        SC_TEST_EXPECT(f.collect());
        SC_TEST_EXPECT(not task.isCompleted());
        if (cancel)
        {
            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(scheduler.runReadyFibers()); // Enter the adapter's cancel-and-join before dispatching I/O.
        }
        SC_TEST_EXPECT(f.dispatch());
        while (not task.isCompleted() and not f.expired)
            SC_TEST_EXPECT(io.runNoWait());
        SC_TEST_EXPECT(task.isCompleted() and not f.expired);
        if (cancel)
            SC_TEST_EXPECT(task.result().isError(AsyncFibersResultCategory, AsyncFibersError::Cancelled));
        else
            SC_TEST_EXPECT(task.result().isError(nativeError.category(), nativeError.errorValue()));
        SC_TEST_EXPECT(f.close());
    }
};
} // namespace
void runAsyncExecutionParityTest(TestReport& report) { AsyncExecutionParityTest test(report); }
} // namespace SC
