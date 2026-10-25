// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Extra/Experimental/AsyncExecution/Tests/AsyncExecutionTestSupport.h"
#include <stdlib.h>
namespace SC
{
namespace
{
struct AsyncExecutionNotificationTest : AsyncExecutionTestCase
{
    AsyncExecutionNotificationTest(TestReport& report)
        : AsyncExecutionTestCase(report, "AsyncExecutionNotificationTest")
    {
        if (test_section("notification latch and composition"))
            latch();
        if (test_section("notification producer while loop runs"))
            concurrent();
        if (test_section("notification single waiter and context"))
            errors();
        if (test_section("notification terminal storage"))
        {
            terminal(0);
            terminal(1);
            terminal(2);
        }
    }
    void latch()
    {
        AsyncExecution execution;
        SC_TEST_EXPECT(execution.create());
        auto&                      io = execution.getContext();
        AsyncExecutionNotification signal{io};
        SC_TEST_EXPECT(signal.notify());
        SC_TEST_EXPECT(signal.notify());
        SC_TEST_EXPECT(consumeBounded(io, io.wakeUp(signal)));
        // Both publications coalesced; the next wait needs a new notification.
        bool   published = false;
        Result publication{false};
        auto   producer = io.sleep({1}) | ex::then(
                                            [&]() noexcept
                                            {
                                                published   = true;
                                                publication = signal.notify();
                                            });
        SC_TEST_EXPECT(consumeBounded(io, ex::when_all(io.wakeUp(signal), move(producer))));
        SC_TEST_EXPECT(published and publication);
        // Prestop leaves the pending bit for a later wait.
        SC_TEST_EXPECT(signal.notify());
        ExecutionStopSource stop;
        stop.request_stop();
        const Result stopped = io.run(io.wakeUp(signal), stop.get_token());
        SC_TEST_EXPECT(stopped.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(consumeBounded(io, io.wakeUp(signal)));
        SC_TEST_EXPECT(execution.close());
    }
    void concurrent()
    {
        AsyncExecution execution;
        SC_TEST_EXPECT(execution.create());
        auto&                      io = execution.getContext();
        AsyncExecutionNotification signal{io};
        struct Producer
        {
            AsyncExecutionNotification& signal;
            EventObject                 release;
            Result                      publication{false};
        } state{signal};
        Thread producer;
        SC_TEST_EXPECT(producer.start(
            [&state](Thread&)
            {
                state.release.wait();
                state.publication = state.signal.notify();
            }));
        // Release the producer from inside loop dispatch, rather than joining it before driving the loop.
        auto         release = io.sleep({1}) | ex::then([&state]() noexcept { state.release.signal(); });
        const Result status  = consumeBounded(io, ex::when_all(io.wakeUp(signal), move(release)));
        state.release.signal(); // Also release it if the bounded graph failed before the gate timer fired.
        SC_TEST_EXPECT(producer.join());
        SC_TEST_EXPECT(status and state.publication);
        SC_TEST_EXPECT(execution.close());
    }
    void errors()
    {
        AsyncExecution first;
        AsyncExecution second;
        SC_TEST_EXPECT(first.create());
        SC_TEST_EXPECT(second.create());
        auto&                      io = first.getContext();
        AsyncExecutionNotification signal{io};
        const Result               mismatch = consumeBounded(second.getContext(), second.getContext().wakeUp(signal));
        SC_TEST_EXPECT(
            mismatch.isError(AsyncExecutionResultCategory, AsyncExecutionError::NotificationContextMismatch));
        ExecutionOutcome outcome;
        outcome.loop = &io.getEventLoop();
        ExecutionStopSource stop;
        auto                operation = ex::connect(io.wakeUp(signal), ExecutionReceiver{outcome, stop.get_token()});
        ex::start(operation);
        SC_TEST_EXPECT(io.getEventLoop().runNoWait());
        const Result closing = first.close();
        SC_TEST_EXPECT(closing.isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
        const Result overlap = consumeBounded(io, io.wakeUp(signal));
        SC_TEST_EXPECT(overlap.isError(AsyncExecutionResultCategory, AsyncExecutionError::NotificationAlreadyWaiting));
        stop.request_stop();
        SC_TEST_EXPECT(runBounded(io.getEventLoop()));
        SC_TEST_EXPECT(outcome.stopped == 1 and outcome.values == 0 and outcome.errors == 0);
        SC_TEST_EXPECT(signal.notify());
        SC_TEST_EXPECT(consumeBounded(io, io.wakeUp(signal)));
        SC_TEST_EXPECT(first.close());
        SC_TEST_EXPECT(second.close());
    }
    void terminal(int mode)
    {
        AsyncExecution execution;
        SC_TEST_EXPECT(execution.create());
        auto&                      io = execution.getContext();
        AsyncExecutionNotification signal{io};
        ExecutionOutcome           outcome;
        outcome.loop = &io.getEventLoop();
        ExecutionStopSource stop;
        auto                sender = io.wakeUp(signal);
        using Operation            = decltype(ex::connect(sender, ExecutionReceiver{outcome, stop.get_token()}));
        void* allocation           = ::malloc(sizeof(Operation) + alignof(Operation) - 1);
        SC_TEST_EXPECT(allocation != nullptr);
        const uintptr_t aligned =
            (reinterpret_cast<uintptr_t>(allocation) + alignof(Operation) - 1) & ~(uintptr_t(alignof(Operation)) - 1);
        auto* operation = reinterpret_cast<Operation*>(aligned);
        placementNew(*operation, io, signal, ExecutionReceiver{outcome, stop.get_token()});
        struct Lifetime
        {
            Operation* operation;
            void*      allocation;
            bool       destroyed = false;
        } lifetime{operation, allocation};
        auto destroy = [&lifetime]()
        {
            lifetime.operation->~Operation();
            ::free(lifetime.allocation);
            lifetime.destroyed = true;
        };
        outcome.onValue   = destroy;
        outcome.onError   = destroy;
        outcome.onStopped = destroy;
        // Occupy the slot for the error case. The rejected operation must not detach this waiter.
        ExecutionOutcome    blockerOutcome;
        ExecutionStopSource blockerStop;
        auto                blocker = ex::connect(sender, ExecutionReceiver{blockerOutcome, blockerStop.get_token()});
        if (mode == 1)
        {
            ex::start(blocker);
            SC_TEST_EXPECT(io.getEventLoop().runNoWait());
        }
        ex::start(*operation);
        SC_TEST_EXPECT(io.getEventLoop().runNoWait());
        if (mode != 1)
        {
            SC_TEST_EXPECT(not lifetime.destroyed);
            struct Publication
            {
                ExecutionStopSource&        stop;
                AsyncExecutionNotification& signal;
                int                         mode;
                Result                      status{false};
            } publication{stop, signal, mode};
            Thread producer;
            SC_TEST_EXPECT(producer.start(
                [&publication](Thread&)
                {
                    if (publication.mode == 2)
                        publication.stop.request_stop();
                    // In the stopped case, queue notification behind cancellation. Detachment must remove
                    // its command before freeing operation storage and retain the pending bit for reuse.
                    publication.status = publication.signal.notify();
                }));
            SC_TEST_EXPECT(producer.join());
            SC_TEST_EXPECT(publication.status);
            SC_TEST_EXPECT(runBounded(io.getEventLoop()));
        }
        SC_TEST_EXPECT(lifetime.destroyed);
        SC_TEST_EXPECT(outcome.values == (mode == 0 ? 1 : 0));
        SC_TEST_EXPECT(outcome.errors == (mode == 1 ? 1 : 0));
        SC_TEST_EXPECT(outcome.stopped == (mode == 2 ? 1 : 0));
        if (mode == 1)
        {
            SC_TEST_EXPECT(
                outcome.error.isError(AsyncExecutionResultCategory, AsyncExecutionError::NotificationAlreadyWaiting));
            SC_TEST_EXPECT(signal.notify());
            SC_TEST_EXPECT(io.getEventLoop().runNoWait());
            SC_TEST_EXPECT(blockerOutcome.values == 1);
        }
        // Stopped delivery retained the queued notification; value/error paths need another publication.
        if (mode != 2)
            SC_TEST_EXPECT(signal.notify());
        SC_TEST_EXPECT(consumeBounded(io, io.wakeUp(signal)));
        SC_TEST_EXPECT(execution.close());
    }
};
} // namespace
void runAsyncExecutionNotificationTest(TestReport& report) { AsyncExecutionNotificationTest test(report); }
} // namespace SC
