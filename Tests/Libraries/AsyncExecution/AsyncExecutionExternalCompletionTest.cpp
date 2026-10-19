// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Time/Time.h"
#include "Tests/Libraries/AsyncExecution/AsyncExecutionTestSupport.h"
#include <stdlib.h>
namespace SC
{
namespace
{
constexpr ResultCategory ExternalTestCategory{0x80000025u};
enum class ExternalTestError : uint32_t
{
    ProducerFailed = 1
};
struct AsyncExecutionExternalCompletionTest : AsyncExecutionTestCase
{
    AsyncExecutionExternalCompletionTest(TestReport& report)
        : AsyncExecutionTestCase(report, "AsyncExecutionExternalCompletionTest")
    {
        if (not report.isTestEnabled("AsyncExecutionExternalCompletionTest"))
            return;
        // Stop alone cannot finish an unacknowledged producer. Bound the entire test independently of the loop.
        Atomic<bool> finished = false;
        Thread       guard;
        SC_TEST_EXPECT(guard.start(
            [&finished](Thread&)
            {
                const auto start = Time::Monotonic::now().getMonotonicMilliseconds();
                while (not finished.load() and Time::Monotonic::now().getMonotonicMilliseconds() - start < 10000)
                    Thread::Sleep(1);
                if (not finished.load())
                    ::_Exit(124);
            }));
        if (test_section("external acknowledgement before wait"))
            early();
        if (test_section("external acknowledgement while loop runs"))
            concurrent();
        if (test_section("external timeout joins acknowledgement"))
            joinedTimeout();
        if (test_section("external acknowledgement rejects overlapping consumers"))
            errors();
        if (test_section("external acknowledgement joins before terminal destruction"))
        {
            terminal(0);
            terminal(1);
            terminal(2);
            terminal(3);
            terminal(4);
        }
        finished.store(true);
        SC_TEST_EXPECT(guard.join());
    }
    void early()
    {
        AsyncExecution execution;
        SC_TEST_EXPECT(execution.create());
        auto&                            io = execution.getContext();
        AsyncExecutionExternalCompletion completion{io};
        SC_TEST_EXPECT(completion.complete(Result(true), 42));
        const Result duplicate = completion.complete(Result(true), 77);
        SC_TEST_EXPECT(duplicate.isError(AsyncResultCategory, AsyncError::CompletionAlreadyPosted));
        size_t bytes = 0;
        SC_TEST_EXPECT(consumeBounded(io, io.externalCompletion(completion), bytes));
        SC_TEST_EXPECT(bytes == 42);
        const Result consumed = consumeBounded(io, io.externalCompletion(completion));
        SC_TEST_EXPECT(consumed.isError(AsyncExecutionResultCategory, AsyncExecutionError::ExternalCompletionConsumed));
        AsyncExecutionExternalCompletion failure{io};
        const Result expected = Result::Error(ExternalTestCategory, ExternalTestError::ProducerFailed);
        SC_TEST_EXPECT(failure.complete(expected, 7));
        const Result failed = consumeBounded(io, io.externalCompletion(failure));
        SC_TEST_EXPECT(failed.isError(ExternalTestCategory, ExternalTestError::ProducerFailed));
        SC_TEST_EXPECT(execution.close());
    }
    void concurrent()
    {
        AsyncExecution execution;
        SC_TEST_EXPECT(execution.create());
        auto&                            io = execution.getContext();
        AsyncExecutionExternalCompletion completion{io};
        struct Producer
        {
            AsyncExecutionExternalCompletion& completion;
            EventObject                       release;
            Result                            publication{false};
            char                              buffer = 0;
        } state{completion};
        Thread producer;
        SC_TEST_EXPECT(producer.start(
            [&state](Thread&)
            {
                state.release.wait();
                state.buffer      = 'x';
                state.publication = state.completion.complete(Result(true), 1);
            }));
        auto         release = io.sleep({1}) | ex::then([&state]() noexcept { state.release.signal(); });
        size_t       bytes   = 0;
        const Result result = consumeBounded(io, ex::when_all(io.externalCompletion(completion), move(release)), bytes);
        state.release.signal();
        SC_TEST_EXPECT(producer.join());
        SC_TEST_EXPECT(result and state.publication);
        SC_TEST_EXPECT(bytes == 1 and state.buffer == 'x');
        SC_TEST_EXPECT(execution.close());
    }
    void joinedTimeout()
    {
        AsyncExecution execution;
        SC_TEST_EXPECT(execution.create());
        auto&                            io = execution.getContext();
        AsyncExecutionExternalCompletion completion{io};
        bool                             observedStop = false;
        bool                             acknowledged = false;
        auto                             acknowledge  = io.sleep({5}) | tryThen(
                                               [&]() noexcept
                                               {
                                                   observedStop = completion.isStopRequested();
                                                   acknowledged = true;
                                                   return completion.complete();
                                               });
        const Result status =
            consumeBounded(io, ex::when_all(timeout(io, io.externalCompletion(completion), {1}), move(acknowledge)));
        SC_TEST_EXPECT(status.isError(AsyncExecutionResultCategory, AsyncExecutionError::TimedOut));
        SC_TEST_EXPECT(observedStop and acknowledged);
        SC_TEST_EXPECT(execution.close());
    }
    void errors()
    {
        AsyncExecution first;
        AsyncExecution second;
        SC_TEST_EXPECT(first.create());
        SC_TEST_EXPECT(second.create());
        auto&                            io = first.getContext();
        AsyncExecutionExternalCompletion completion{io};
        const Result mismatch = consumeBounded(second.getContext(), second.getContext().externalCompletion(completion));
        SC_TEST_EXPECT(
            mismatch.isError(AsyncExecutionResultCategory, AsyncExecutionError::ExternalCompletionContextMismatch));
        ExecutionOutcome outcome;
        outcome.loop = &io.getEventLoop();
        ExecutionStopSource stop;
        auto operation = ex::connect(io.externalCompletion(completion), ExecutionReceiver{outcome, stop.get_token()});
        ex::start(operation);
        SC_TEST_EXPECT(io.getEventLoop().runNoWait());
        const Result closing = first.close();
        SC_TEST_EXPECT(closing.isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
        ExecutionStopSource alreadyStopped;
        alreadyStopped.request_stop();
        const Result overlap = io.run(io.externalCompletion(completion), alreadyStopped.get_token());
        SC_TEST_EXPECT(
            overlap.isError(AsyncExecutionResultCategory, AsyncExecutionError::ExternalCompletionAlreadyWaiting));
        SC_TEST_EXPECT(not completion.isStopRequested());
        stop.request_stop();
        SC_TEST_EXPECT(completion.isStopRequested());
        SC_TEST_EXPECT(io.getEventLoop().runNoWait());
        SC_TEST_EXPECT(outcome.stopped == 0 and outcome.values == 0 and outcome.errors == 0);
        SC_TEST_EXPECT(completion.complete());
        SC_TEST_EXPECT(runBounded(io.getEventLoop()));
        SC_TEST_EXPECT(outcome.stopped == 1 and outcome.values == 0 and outcome.errors == 0);
        SC_TEST_EXPECT(first.close());
        SC_TEST_EXPECT(second.close());
    }
    void terminal(int mode)
    {
        AsyncExecution execution;
        if (mode != 4)
            SC_TEST_EXPECT(execution.create());
        auto&                            io = execution.getContext();
        AsyncExecutionExternalCompletion completion{io};
        ExecutionStopSource              stop;
        if (mode == 3)
            stop.request_stop();
        ExecutionOutcome outcome;
        outcome.loop    = &io.getEventLoop();
        auto sender     = io.externalCompletion(completion);
        using Operation = decltype(ex::connect(sender, ExecutionReceiver{outcome, stop.get_token()}));
        struct Lifetime
        {
            void*      allocation = nullptr;
            Operation* operation  = nullptr;
            bool       destroyed  = false;
        } lifetime;
        lifetime.allocation = ::malloc(sizeof(Operation) + alignof(Operation) - 1);
        SC_TEST_EXPECT(lifetime.allocation != nullptr);
        const uintptr_t aligned = (reinterpret_cast<uintptr_t>(lifetime.allocation) + alignof(Operation) - 1) &
                                  ~(uintptr_t(alignof(Operation)) - 1);
        lifetime.operation = reinterpret_cast<Operation*>(aligned);
        placementNew(*lifetime.operation, io, completion, ExecutionReceiver{outcome, stop.get_token()});
        auto destroy = [&lifetime]()
        {
            lifetime.operation->~Operation();
            ::free(lifetime.allocation);
            lifetime.destroyed = true;
        };
        outcome.onValue   = destroy;
        outcome.onError   = destroy;
        outcome.onStopped = destroy;
        ex::start(*lifetime.operation);
        if (mode == 4)
        {
            SC_TEST_EXPECT(lifetime.destroyed and outcome.errors == 1);
            SC_TEST_EXPECT(outcome.error.isError(AsyncResultCategory, AsyncError::NotInitialized));
            return;
        }
        SC_TEST_EXPECT(io.getEventLoop().runNoWait());
        SC_TEST_EXPECT(not lifetime.destroyed);
        struct Producer
        {
            AsyncExecutionExternalCompletion& completion;
            ExecutionStopSource&              stop;
            int                               mode;
            Result                            publication{false};
            bool                              observedStop = false;
        } state{completion, stop, mode};
        Thread producer;
        SC_TEST_EXPECT(producer.start(
            [&state](Thread&)
            {
                if (state.mode == 1 or state.mode == 2)
                    state.stop.request_stop();
                state.observedStop  = state.completion.isStopRequested();
                const Result status = state.mode == 1
                                          ? Result::Error(ExternalTestCategory, ExternalTestError::ProducerFailed)
                                          : Result(true);
                state.publication   = state.completion.complete(status, 9);
            }));
        SC_TEST_EXPECT(producer.join());
        SC_TEST_EXPECT(state.publication);
        SC_TEST_EXPECT(state.observedStop == (mode != 0));
        SC_TEST_EXPECT(not lifetime.destroyed);
        SC_TEST_EXPECT(runBounded(io.getEventLoop()));
        SC_TEST_EXPECT(lifetime.destroyed);
        SC_TEST_EXPECT(outcome.values == (mode == 0 ? 1 : 0));
        SC_TEST_EXPECT(outcome.errors == (mode == 1 ? 1 : 0));
        SC_TEST_EXPECT(outcome.stopped == (mode >= 2 ? 1 : 0));
        if (mode == 1)
            SC_TEST_EXPECT(outcome.error.isError(ExternalTestCategory, ExternalTestError::ProducerFailed));
        SC_TEST_EXPECT(io.getEventLoop().runNoWait());
        SC_TEST_EXPECT(execution.close());
    }
};
} // namespace
void runAsyncExecutionExternalCompletionTest(TestReport& report) { AsyncExecutionExternalCompletionTest test(report); }
} // namespace SC
