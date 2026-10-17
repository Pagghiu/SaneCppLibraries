// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/AsyncExecution/AsyncExecution.h"
#if !SC_ASYNC_EXECUTION_AVAILABLE
#error                                                                                                                 \
    "Sender/receiver <execution> is unavailable. Set SC_ASYNC_EXECUTION_STDEXEC_INCLUDE to a stdexec include directory."
#endif
#include "Examples/AsyncExecutionDemo/Base64.h"
#include "Libraries/Common/PlacementNew.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
namespace
{
namespace ex = AsyncExecutionProtocol;
#if SC_ASYNC_EXECUTION_USE_STDEXEC
using ExecutionStopToken  = ex::inplace_stop_token;
using ExecutionStopSource = ex::inplace_stop_source;
#else
using ExecutionStopToken  = std::inplace_stop_token;
using ExecutionStopSource = std::inplace_stop_source;
#endif
struct ExecutionOutcome
{
    int    values  = 0;
    int    errors  = 0;
    int    stopped = 0;
    Result error{true};

    Function<void()> onValue;
    Function<void()> onStopped;
    AsyncEventLoop*  loop = nullptr;
};
struct ExecutionEnvironment
{
    ExecutionStopToken token;

    auto query(ex::get_stop_token_t) const noexcept { return token; }
};
struct ExecutionReceiver
{
    using receiver_concept = ex::receiver_tag;

    ExecutionOutcome&  outcome;
    ExecutionStopToken token;

    auto get_env() const noexcept { return ExecutionEnvironment{token}; }
    template <typename... Values>
    void set_value(Values&&...) && noexcept
    {
        auto* loop = outcome.loop;
        ++outcome.values;
        if (outcome.onValue.isValid())
            outcome.onValue();
        if (loop)
            loop->interrupt();
    }
    void set_error(Result error) && noexcept
    {
        ++outcome.errors;
        outcome.error = error;
        if (outcome.loop)
            outcome.loop->interrupt();
    }
    void set_stopped() && noexcept
    {
        auto* loop = outcome.loop;
        ++outcome.stopped;
        if (outcome.onStopped.isValid())
            outcome.onStopped();
        if (loop)
            loop->interrupt();
    }
};

struct AsyncExecutionTest : TestCase
{
    AsyncExecutionTest(TestReport& report) : TestCase(report, "AsyncExecutionTest")
    {
        using Timer = decltype(std::declval<AsyncExecutionContext>().sleep({1}));
        static_assert(ex::sender<Timer>);
        static_assert(ex::scheduler<AsyncExecutionScheduler>);
        static_assert(ex::scheduler<AsyncExecutionPoolScheduler>);
        using Operation = decltype(ex::connect(std::declval<Timer>(), std::declval<ExecutionReceiver>()));
        static_assert(ex::operation_state<Operation>);
        static_assert(not std::is_move_constructible_v<Operation>);

        if (test_section("owning facade files pool and repeated runs"))
            owningFacade();
        if (test_section("scheduler submission from another thread"))
            scheduler();
        if (test_section("timer composition and thread pool"))
            timersAndWork();
        if (test_section("submission failure and cooperative stop"))
            failuresAndStop();
        if (test_section("prompt timer cancellation"))
            promptTimerCancellation();
        if (test_section("completion and stop races"))
            cancellationRaces();
        if (test_section("worker cancellation waits for quiescence"))
            workerCancellation();
        if (test_section("socket send receive"))
            sockets();
        if (test_section("file read write"))
            files();
        if (test_section("destroy operation during completion"))
            completionLifetime();
    }

    void owningFacade()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create(2));
        const auto  ownerThread       = Thread::CurrentThreadID();
        Atomic<int> poolContinuations = 0;
        auto        cpu               = [&]()
        {
            return ex::just(21) | ex::continues_on(app.getPoolScheduler()) |
                   ex::then(
                       [&](int value) noexcept
                       {
                           if (Thread::CurrentThreadID() != ownerThread and value == 21)
                               poolContinuations.fetch_add(1);
                           return value * 2;
                       }) |
                   ex::continues_on(app.getScheduler()) |
                   ex::then(
                       [&](int value) noexcept
                       {
                           SC_TEST_EXPECT(Thread::CurrentThreadID() == ownerThread);
                           SC_TEST_EXPECT(value == 42);
                       });
        };
        SC_TEST_EXPECT(app.run(ex::when_all(cpu(), cpu())));
        SC_TEST_EXPECT(poolContinuations.load() == 2);
        ExecutionStopSource poolStop;
        poolStop.request_stop();
        SC_TEST_EXPECT(app.run(ex::schedule(app.getPoolScheduler()), poolStop.get_token())
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        // then keeps a fallible transform's checked result as a value. TCP must translate failure before connecting.
        const Result encodingFailure = Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge);
        SC_TEST_EXPECT(
            app.run(app.sendTCP("127.0.0.1", report.mapPort(19092), AsyncExecutionBufferResult{encodingFailure, {}}))
                .isError(encodingFailure.category(), encodingFailure.errorValue()));
        const char* vectors[]  = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
        const char* expected[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
        for (size_t i = 0; i < 7; ++i)
        {
            AsyncExecutionBuffer bytes;
            SC_TEST_EXPECT(bytes.resize(strlen(vectors[i])));
            if (bytes.size() != 0)
                memcpy(bytes.bytes().data(), vectors[i], bytes.size());
            auto encoded = base64Encode(move(bytes));
            SC_TEST_EXPECT(encoded.status);
            SC_TEST_EXPECT(encoded.data.size() == strlen(expected[i]));
            SC_TEST_EXPECT(encoded.data.size() == 0 or
                           memcmp(encoded.data.bytes().data(), expected[i], encoded.data.size()) == 0);
        }
        // An external synchronous sender must not leave a stale loop interrupt behind.
        SC_TEST_EXPECT(app.run(ex::just()));
        SC_TEST_EXPECT(app.run(app.sleep({1})));
        AsyncExecutionBuffer input;
        SC_TEST_EXPECT(input.resize(5));
        memcpy(input.bytes().data(), "hello", 5);
        StringPath path;
        SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory.view(), "AsyncExecutionOwnedTest.txt"}));
        SC_TEST_EXPECT(app.run(app.writeFile(path.view(), move(input))));
        bool checked = false;
        auto job     = app.readFile(path.view()) |
                   ex::let_value(
                       [&](auto& bytes)
                       {
                           return app.onPool(move(bytes),
                                             [](auto& data)
                                             {
                                                 for (auto& c : data.bytes())
                                                     c = static_cast<char>(c - 'a' + 'A');
                                             });
                       }) |
                   ex::then([&](const auto& bytes) noexcept
                            { checked = bytes.size() == 5 and memcmp(bytes.bytes().data(), "HELLO", 5) == 0; });
        SC_TEST_EXPECT(app.run(move(job)));
        SC_TEST_EXPECT(checked);
        SC_TEST_EXPECT(app.run(app.readFile(path.view(), 2))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge));
        // Owning senders preserve normal operation errors and support an empty file.
        SC_TEST_EXPECT(app.run(app.writeFile(path.view(), {})));
        bool empty = false;
        SC_TEST_EXPECT(app.run(app.readFile(path.view()) |
                               ex::then([&](const auto& bytes) noexcept { empty = bytes.size() == 0; })));
        SC_TEST_EXPECT(empty);
        SC_TEST_EXPECT(FileSystem::Operations::removeFile(path.view()));
        SC_TEST_EXPECT(not app.run(app.readFile(path.view())));
        ExecutionStopSource source;
        source.request_stop();
        SC_TEST_EXPECT(app.run(app.sleep({60000}), source.get_token())
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(app.run(app.sleep({1})));
    }

    Result runBounded(AsyncEventLoop& loop)
    {
        AsyncLoopTimeout watchdog;
        bool             timedOut = false;
        watchdog.callback         = [&](AsyncLoopTimeout::Result&)
        {
            timedOut = true;
            loop.interrupt();
        };
        SC_TRY(watchdog.start(loop, {2000}));
        const Result status = loop.run();
        if (not watchdog.isFree())
            SC_TRY(watchdog.unschedule(loop));
        return Result(status and not timedOut);
    }

    void scheduler()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        const auto       owner            = Thread::CurrentThreadID();
        uint64_t         completionThread = 0;
        ExecutionOutcome outcome;
        outcome.loop   = &loop;
        auto scheduler = context.getScheduler();
        auto sender    = ex::schedule(scheduler);
        SC_TEST_EXPECT(ex::get_completion_scheduler<ex::set_value_t>(ex::get_env(sender)) == scheduler);
        SC_TEST_EXPECT(ex::get_forward_progress_guarantee(scheduler) ==
                       ex::forward_progress_guarantee::weakly_parallel);
        auto   graph     = move(sender) | ex::then([&]() noexcept { completionThread = Thread::CurrentThreadID(); });
        auto   operation = ex::connect(move(graph), ExecutionReceiver{outcome, {}});
        Thread producer;
        SC_TEST_EXPECT(producer.start([&](Thread&) { ex::start(operation); }));
        SC_TEST_EXPECT(producer.join());
        SC_TEST_EXPECT(outcome.values == 0);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(outcome.values == 1 and completionThread == owner);
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void timersAndWork()
    {
        AsyncEventLoop loop;
        ThreadPool     pool;
        SC_TEST_EXPECT(loop.create());
        SC_TEST_EXPECT(pool.create(1));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        ExecutionOutcome outcome;
        outcome.loop               = &loop;
        Atomic<int> worked         = 0;
        int         timerCompleted = 0;

        const auto ownerThread      = Thread::CurrentThreadID();
        uint64_t   workerThread     = ownerThread;
        uint64_t   completionThread = 0;
        outcome.onValue             = [&] { completionThread = Thread::CurrentThreadID(); };

        Function<Result()> procedure = [&]()
        {
            worked.fetch_add(1);
            workerThread = Thread::CurrentThreadID();
            return Result(true);
        };

        auto work      = context.work(pool, procedure);
        auto timer     = context.sleep({1}) | ex::then([&]() noexcept { ++timerCompleted; });
        auto graph     = ex::when_all(move(timer), move(work));
        auto operation = ex::connect(move(graph), ExecutionReceiver{outcome, {}});
        SC_TEST_EXPECT(outcome.values == 0);
        SC_TEST_EXPECT(loop.getNumberOfActiveRequests() <= 1);
        ex::start(operation);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(outcome.values == 1 and outcome.errors == 0 and outcome.stopped == 0);
        SC_TEST_EXPECT(timerCompleted == 1 and worked.load() == 1);
        SC_TEST_EXPECT(workerThread != ownerThread and completionThread == ownerThread);
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
        SC_TEST_EXPECT(pool.destroy());
    }

    void completionLifetime()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        ExecutionOutcome outcome;
        outcome.loop = &loop;

        auto sender     = context.sleep({1});
        using Operation = decltype(ex::connect(sender, ExecutionReceiver{outcome, {}}));
        alignas(Operation) char storage[sizeof(Operation)];

        auto* operation = reinterpret_cast<Operation*>(storage);
        placementNew(*operation, context, sender.setup, ExecutionReceiver{outcome, {}});
        bool destroyed  = false;
        outcome.onValue = [&]
        {
            operation->~Operation();
            destroyed = true;
        };
        ex::start(*operation);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(destroyed and outcome.values == 1);
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void failuresAndStop()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SocketDescriptor invalid;
        ExecutionOutcome failed;
        failed.loop = &loop;
        char buffer[8];
        auto failure = ex::connect(context.receive(invalid, buffer), ExecutionReceiver{failed, {}});
        ex::start(failure);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(failed.errors == 1 and not failed.error);
        SC_TEST_EXPECT(failed.values == 0 and failed.stopped == 0);

        ExecutionStopSource source;
        source.request_stop();
        ExecutionOutcome stopped;
        stopped.loop     = &loop;
        auto beforeStart = ex::connect(context.sleep({1000}), ExecutionReceiver{stopped, source.get_token()});
        ex::start(beforeStart);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(stopped.stopped == 1 and stopped.values == 0 and stopped.errors == 0);

        ExecutionStopSource activeSource;
        ExecutionOutcome    active;
        active.loop     = &loop;
        auto afterStart = ex::connect(context.sleep({1}), ExecutionReceiver{active, activeSource.get_token()});
        ex::start(afterStart);
        activeSource.request_stop();
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(active.stopped == 1 and active.values == 0 and active.errors == 0);
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void promptTimerCancellation()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        ExecutionOutcome outcome;
        outcome.loop = &loop;
        ExecutionStopSource source;
        auto                sender = context.sleep({60000});
        using Operation            = decltype(ex::connect(sender, ExecutionReceiver{outcome, source.get_token()}));
        alignas(Operation) char storage[sizeof(Operation)];
        auto*                   operation = reinterpret_cast<Operation*>(storage);
        placementNew(*operation, context, sender.setup, ExecutionReceiver{outcome, source.get_token()});
        bool destroyed    = false;
        outcome.onStopped = [&]
        {
            operation->~Operation();
            destroyed = true;
        };
        ex::start(*operation);
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(operation->request.isActive());
        SC_TEST_EXPECT(not context.close()); // An active operation prevents dispatcher shutdown.
        Thread canceller;
        SC_TEST_EXPECT(canceller.start([&](Thread&) { source.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(outcome.stopped == 1 and outcome.values == 0 and outcome.errors == 0 and destroyed);
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void cancellationRaces()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        for (int iteration = 0; iteration < 32; ++iteration)
        {
            ExecutionOutcome outcome;
            outcome.loop = &loop;
            ExecutionStopSource source;
            auto operation = ex::connect(context.sleep({0}), ExecutionReceiver{outcome, source.get_token()});
            ex::start(operation);
            Thread canceller;
            SC_TEST_EXPECT(canceller.start([&](Thread&) { source.request_stop(); }));
            SC_TEST_EXPECT(runBounded(loop));
            SC_TEST_EXPECT(canceller.join());
            SC_TEST_EXPECT(loop.runNoWait());
            SC_TEST_EXPECT(outcome.values + outcome.stopped == 1 and outcome.errors == 0);
        }
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void workerCancellation()
    {
        AsyncEventLoop loop;
        ThreadPool     pool;
        SC_TEST_EXPECT(loop.create());
        SC_TEST_EXPECT(pool.create(1));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        ExecutionOutcome outcome;
        outcome.loop = &loop;
        struct State
        {
            ExecutionStopSource source;
            EventObject         started, release;
            Atomic<bool>        finished = false;
        } state;
        auto operation = ex::connect(context.work(pool,
                                                  [&]()
                                                  {
                                                      state.started.signal();
                                                      state.release.wait();
                                                      state.finished.store(true);
                                                      return Result(true);
                                                  }),
                                     ExecutionReceiver{outcome, state.source.get_token()});
        ex::start(operation);
        Thread canceller;
        SC_TEST_EXPECT(canceller.start(
            [&](Thread&)
            {
                state.started.wait();
                state.source.request_stop();
                state.release.signal();
            }));
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(canceller.join());
        SC_TEST_EXPECT(state.finished.load() and outcome.stopped == 1 and outcome.values == 0 and outcome.errors == 0);
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
        SC_TEST_EXPECT(pool.destroy());
    }

    void sockets()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SocketDescriptor listener, client, accepted;
        SocketIPAddress  address;
        const uint16_t   port = report.mapPort(5092);
        SC_TEST_EXPECT(address.fromAddressPort("127.0.0.1", port));
        SC_TEST_EXPECT(listener.create(address.getAddressFamily()));
        SC_TEST_EXPECT(SocketServer(listener).bind(address));
        SC_TEST_EXPECT(SocketServer(listener).listen(1));
        SC_TEST_EXPECT(client.create(address.getAddressFamily()));
        SC_TEST_EXPECT(SocketClient(client).connect("127.0.0.1", port));
        SC_TEST_EXPECT(SocketServer(listener).accept(address.getAddressFamily(), accepted));
        SC_TEST_EXPECT(client.setBlocking(false));
        SC_TEST_EXPECT(accepted.setBlocking(false));
        SC_TEST_EXPECT(loop.associateExternallyCreatedSocket(client));
        SC_TEST_EXPECT(loop.associateExternallyCreatedSocket(accepted));
        char                     buffer[16] = {};
        size_t                   sent       = 0;
        AsyncExecutionReadResult received;
        ExecutionOutcome         outcome;
        outcome.loop   = &loop;
        auto graph     = ex::when_all(context.send(client, StringView("slice").toCharSpan()) |
                                          ex::then([&](size_t count) noexcept { sent = count; }),
                                      context.receive(accepted, buffer) |
                                          ex::then([&](AsyncExecutionReadResult value) noexcept { received = value; }));
        auto operation = ex::connect(move(graph), ExecutionReceiver{outcome, {}});
        ex::start(operation);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(outcome.values == 1 and outcome.errors == 0);
        SC_TEST_EXPECT(sent == 5 and received.data.sizeInBytes() == 5 and not received.endOfFile);
        SC_TEST_EXPECT(memcmp(buffer, "slice", 5) == 0);
        ExecutionStopSource source;
        ExecutionOutcome    cancelled;
        cancelled.loop = &loop;
        auto idleRead =
            ex::connect(context.receive(accepted, buffer), ExecutionReceiver{cancelled, source.get_token()});
        ex::start(idleRead);
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(idleRead.request.isActive());
        source.request_stop();
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(cancelled.stopped == 1 and cancelled.values == 0 and cancelled.errors == 0);

        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void files()
    {
        AsyncEventLoop loop;
        ThreadPool     pool;
        SC_TEST_EXPECT(loop.create());
        SC_TEST_EXPECT(pool.create(1));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SmallStringNative<512> path;
        SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory.view(), "AsyncExecutionTest.txt"}));
        FileDescriptor file;
        FileOpen       options;
        options.mode = FileOpen::Write;
        SC_TEST_EXPECT(file.open(path.view(), options));
        ExecutionOutcome writeOutcome;
        writeOutcome.loop = &loop;
        size_t written    = 0;
        auto   write      = ex::connect(context.fileWrite(file, StringView("execution").toCharSpan(), pool) |
                                            ex::then([&](size_t count) noexcept { written = count; }),
                                        ExecutionReceiver{writeOutcome, {}});
        ex::start(write);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(writeOutcome.values == 1 and written == 9);
        SC_TEST_EXPECT(file.close());
        options.mode = FileOpen::Read;
        SC_TEST_EXPECT(file.open(path.view(), options));
        char             buffer[32] = {};
        ExecutionOutcome readOutcome;
        readOutcome.loop = &loop;
        AsyncExecutionReadResult result;
        auto                     read = ex::connect(context.fileRead(file, buffer, pool) |
                                                        ex::then([&](AsyncExecutionReadResult value) noexcept { result = value; }),
                                                    ExecutionReceiver{readOutcome, {}});
        ex::start(read);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(readOutcome.values == 1 and result.data.sizeInBytes() == 9);
        SC_TEST_EXPECT(memcmp(buffer, "execution", 9) == 0);
        SC_TEST_EXPECT(file.close());
        FileSystem fs;
        SC_TEST_EXPECT(fs.removeFile(path.view()));
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
        SC_TEST_EXPECT(pool.destroy());
    }
};
} // namespace
void runAsyncExecutionTest(TestReport& report) { AsyncExecutionTest test(report); }
} // namespace SC
