// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/AsyncFibers/AsyncFibers.h"
#include "Libraries/Await/Await.h"
#include "Tests/Libraries/AsyncExecution/AsyncExecutionTestSupport.h"
#include <stdio.h>
namespace SC
{
namespace
{
struct AsyncExecutionParityTest : AsyncExecutionTestCase
{
    Result nativeError{false};
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
            callbacks(true);
            senders(false);
            senders(true);
            coroutines(false);
            coroutines(true);
            fibers(false);
            fibers(true);
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
