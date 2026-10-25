// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncExecutionTestSupport.h"
#include "Libraries/Strings/StringView.h"
#include <stdlib.h>
namespace SC
{
namespace
{
// Deliberately expose alternative arities and a borrowed reference to verify owned decayed snapshots.
struct TimeoutChoiceSender
{
    using sender_concept = ex::sender_tag;
    int* source;
    bool pair;
    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        return ex::completion_signatures<ex::set_value_t(int&), ex::set_value_t(int, int)>{};
    }
    template <typename Receiver>
    struct Operation
    {
        using operation_state_concept = ex::operation_state_tag;
        Receiver receiver;
        int*     source;
        bool     pair;
        void     start() noexcept
        {
            if (pair)
                ex::set_value(move(receiver), 3, 4);
            else
                ex::set_value(move(receiver), *source);
        }
    };
    template <typename Receiver>
    auto connect(Receiver receiver) && noexcept
    {
        return Operation<Receiver>{move(receiver), source, pair};
    }
};

struct AsyncExecutionTimeoutTest : AsyncExecutionTestCase
{
    AsyncExecutionTimeoutTest(TestReport& report) : AsyncExecutionTestCase(report, "AsyncExecutionTimeoutTest")
    {
        if (test_section("joined timeout values errors and stop"))
            timeoutBasics();
        if (test_section("joined timeout running CPU and error precedence"))
        {
            timeoutCPU(false, false);
            timeoutCPU(true, false);
            timeoutCPU(false, true);
            timeoutCPU(true, true);
        }
        if (test_section("joined timeout native reuse and terminal destruction"))
            timeoutLifetime();
    }
    void timeoutCPU(bool failure, bool callerStop)
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create());
        auto& context = app.getContext();
        auto& loop    = context.getEventLoop();
        struct Gate
        {
            EventObject  entered, release;
            Atomic<bool> done         = false;
            Atomic<bool> forced       = false;
            Atomic<bool> finished     = false;
            Atomic<bool> observedStop = false;
        } gate;
        Thread guard;
        SC_TEST_EXPECT(guard.start(
            [&gate](Thread&)
            {
                for (int attempt = 0; attempt < 2000 and not gate.done.load(); ++attempt)
                    Thread::Sleep(1);
                if (not gate.done.load())
                {
                    gate.forced.store(true);
                    gate.entered.signal();
                    gate.release.signal();
                }
                for (int attempt = 0; attempt < 2000 and not gate.done.load(); ++attempt)
                    Thread::Sleep(1);
                if (not gate.done.load())
                    ::_Exit(124);
            }));
        auto work = ex::read_env(ex::get_stop_token) |
                    ex::let_value(
                        [&](auto& token) noexcept
                        {
                            return ex::schedule(app.getPoolScheduler()) |
                                   tryThen(
                                       [&gate, &token, failure]() noexcept
                                       {
                                           gate.entered.signal();
                                           gate.release.wait();
                                           gate.observedStop.store(token.stop_requested());
                                           gate.finished.store(true);
                                           return failure ? Result::Error(ResultCategory(99), 82) : Result(true);
                                       });
                        });
        ExecutionStopSource stop;
        ExecutionOutcome    outcome;
        // Either branch can be last. A worker terminal notification must wake the owner before interrupting its loop.
        AsyncLoopWakeUp terminal;
        terminal.callback = [&loop](AsyncLoopWakeUp::Result&) { loop.interrupt(); };
        SC_TEST_EXPECT(terminal.start(loop));
        auto notify       = [&]() { SC_ASYNC_TRUST_RESULT(terminal.wakeUp(loop)); };
        outcome.onValue   = notify;
        outcome.onError   = notify;
        outcome.onStopped = notify;
        auto operation    = ex::connect(timeout(context, move(work), {callerStop ? 5000 : 1}),
                                        ExecutionReceiver{outcome, stop.get_token()});
        ex::start(operation);
        gate.entered.wait();
        if (callerStop)
        {
            Thread canceller;
            SC_TEST_EXPECT(canceller.start([&stop](Thread&) { stop.request_stop(); }));
            SC_TEST_EXPECT(canceller.join());
            SC_TEST_EXPECT(loop.runNoWait());
        }
        else
            SC_TEST_EXPECT(consumeBounded(context, context.sleep({10})));
        SC_TEST_EXPECT(not gate.finished.load() and outcome.values == 0 and outcome.errors == 0 and
                       outcome.stopped == 0);
        SC_TEST_EXPECT(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
        gate.release.signal();
        SC_TEST_EXPECT(runBounded(loop));
        gate.done.store(true);
        SC_TEST_EXPECT(guard.join());
        SC_TEST_EXPECT(not gate.forced.load() and gate.finished.load() and gate.observedStop.load());
        if (failure)
            SC_TEST_EXPECT(outcome.errors == 1 and outcome.error.isError(ResultCategory(99), 82));
        else if (callerStop)
            SC_TEST_EXPECT(outcome.stopped == 1 and outcome.errors == 0);
        else
            SC_TEST_EXPECT(outcome.errors == 1 and
                           outcome.error.isError(AsyncExecutionResultCategory, AsyncExecutionError::TimedOut));
        // Worker notifications remain borrowed through their callback return.
        SC_TEST_EXPECT(app.getPoolScheduler().pool->waitForAllTasks());
        SC_TEST_EXPECT(app.close());
    }

    void timeoutLifetime()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create());
        auto& context = app.getContext();
        auto& loop    = context.getEventLoop();
        {
            SocketDescriptor         listener, client;
            AsyncExecutionConnection accepted;
            SC_TEST_EXPECT(tcpPair(context, listener, client, accepted, report.mapPort(5110)));
            char bytes[2] = {};
            SC_TEST_EXPECT(consumeBounded(context, timeout(context, context.receive(accepted.socket, bytes), {1}))
                               .isError(AsyncExecutionResultCategory, AsyncExecutionError::TimedOut));
            AsyncExecutionReadResult read;
            size_t                   sent = 0;
            SC_TEST_EXPECT(consumeBounded(context,
                                          ex::when_all(context.receiveExact(accepted.socket, bytes),
                                                       context.sendAll(client, StringView("OK").toCharSpan())),
                                          read, sent));
            SC_TEST_EXPECT(sent == 2 and read.data.sizeInBytes() == 2 and bytes[0] == 'O' and bytes[1] == 'K');
        }
        {
            ExecutionOutcome outcome;
            outcome.loop    = &loop;
            auto graph      = timeout(context, context.sleep({5000}), {1});
            using Graph     = decltype(graph);
            using Operation = decltype(ex::connect(std::declval<Graph>(), ExecutionReceiver{outcome, {}}));
            alignas(Operation) char storage[sizeof(Operation)];
            struct Holder
            {
                Operation operation;
                Holder(Graph graph, ExecutionReceiver receiver) : operation(ex::connect(move(graph), move(receiver))) {}
            };
            static_assert(sizeof(Holder) == sizeof(Operation));
            auto* holder = reinterpret_cast<Holder*>(storage);
            placementNew(*holder, move(graph), ExecutionReceiver{outcome, {}});
            bool destroyed  = false;
            outcome.onError = [&]()
            {
                holder->~Holder();
                destroyed = true;
            };
            ex::start(holder->operation);
            SC_TEST_EXPECT(runBounded(loop));
            SC_TEST_EXPECT(destroyed and outcome.errors == 1 and
                           outcome.error.isError(AsyncExecutionResultCategory, AsyncExecutionError::TimedOut));
        }
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
        {
            AsyncExecutionScope scope(context, AsyncExecutionScopeAllocator::heap());
            SC_TEST_EXPECT(scope.spawn(timeout(context, ex::schedule(app.getPoolScheduler()), {5000})));
            SC_TEST_EXPECT(consumeBounded(context, scope.join()));
            SC_TEST_EXPECT(scope.getSummary().succeeded == 1);
        }
#endif
        SC_TEST_EXPECT(app.close());
    }

    void timeoutBasics()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create());
        auto& context = app.getContext();
        SC_TEST_EXPECT(consumeBounded(context, timeout(context, ex::just(), {5000})));
        int number = 0;
        SC_TEST_EXPECT(consumeBounded(context, timeout(context, ex::just(42), {5000}), number));
        SC_TEST_EXPECT(number == 42);
        ExecutionTestBytes owned;
        SC_TEST_EXPECT(owned.resize(3));
        owned.bytes()[0] = 'A';
        ExecutionTestBytes received;
        number = 0;
        SC_TEST_EXPECT(consumeBounded(context, timeout(context, ex::just(7, move(owned)), {5000}), number, received));
        SC_TEST_EXPECT(number == 7 and received.size() == 3 and received.bytes()[0] == 'A');
        bool retained = false;
        auto delayed =
            timeout(context, ex::just(move(received)), {5000}) |
            ex::let_value(
                [&](ExecutionTestBytes& bytes) noexcept
                {
                    return context.sleep({1}) | ex::then([&bytes, &retained]() noexcept
                                                         { retained = bytes.size() == 3 and bytes.bytes()[0] == 'A'; });
                });
        SC_TEST_EXPECT(consumeBounded(context, move(delayed)));
        SC_TEST_EXPECT(retained);
        auto environment = timeout(
            context,
            ex::read_env(ex::get_scheduler) |
                ex::then([&context](auto scheduler) noexcept { return scheduler == context.getScheduler() ? 19 : 0; }),
            {5000});
        SC_TEST_EXPECT(consumeBounded(context, move(environment), number));
        SC_TEST_EXPECT(number == 19);
        for (int mode = 0; mode < 2; ++mode)
        {
            const bool       pair   = mode != 0;
            int              source = 11;
            int              sum    = 0;
            int              count  = 0;
            ExecutionOutcome outcome;
            outcome.loop = &context.getEventLoop();
            auto choice =
                timeout(context, TimeoutChoiceSender{&source, pair}, {5000}) | ex::then(
                                                                                   [&](auto... values) noexcept
                                                                                   {
                                                                                       count = sizeof...(values);
                                                                                       sum   = (values + ... + 0);
                                                                                   });
            auto operation = ex::connect(move(choice), ExecutionReceiver{outcome, {}});
            ex::start(operation);
            source = 99;
            SC_TEST_EXPECT(runBounded(context.getEventLoop()));
            SC_TEST_EXPECT(outcome.values == 1 and count == (pair ? 2 : 1) and sum == (pair ? 7 : 11));
        }
        {
            detail::AsyncExecutionTimeoutState state;
            SC_TEST_EXPECT(state.beginCallerStop());
            SC_TEST_EXPECT(not state.joined());
            SC_TEST_EXPECT(state.endCallerStop());
            bool stopped = false;
            SC_TEST_EXPECT(state.outcome(stopped) and stopped and not state.beginCallerStop());
        }
        const Result timerError = Result::Error(ResultCategory(99), 83);
        auto brokenTimer = detail::AsyncExecutionTimeoutSender{context.sleep({5000}), ex::just_error(timerError)};
        SC_TEST_EXPECT(consumeBounded(context, move(brokenTimer)).isError(ResultCategory(99), 83));
        auto bothErrors = detail::AsyncExecutionTimeoutSender{ex::just_error(Result::Error(ResultCategory(99), 84)),
                                                              ex::just_error(timerError)};
        SC_TEST_EXPECT(consumeBounded(context, move(bothErrors)).isError(ResultCategory(99), 84));
        const Result error = Result::Error(ResultCategory(99), 81);
        SC_TEST_EXPECT(
            consumeBounded(context, timeout(context, ex::just_error(error), {5000})).isError(ResultCategory(99), 81));
        const Result explicitStopped = Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::Stopped);
        SC_TEST_EXPECT(consumeBounded(context, timeout(context, ex::just_error(explicitStopped), {5000}))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(consumeBounded(context, timeout(context, ex::just_stopped(), {5000}))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(consumeBounded(context, timeout(context, context.sleep({5000}), {1}))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::TimedOut));
        SC_TEST_EXPECT(consumeBounded(context, timeout(context, context.sleep({1}), {5000})));
        ExecutionStopSource stopped;
        stopped.request_stop();
        SC_TEST_EXPECT(context.run(timeout(context, context.sleep({5000}), {5000}), stopped.get_token())
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(app.close());
    }
};
} // namespace
void runAsyncExecutionTimeoutTest(TestReport& report) { AsyncExecutionTimeoutTest test(report); }
} // namespace SC
