// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Tests/Libraries/AsyncExecution/AsyncExecutionTestSupport.h"
#include <stdlib.h>
namespace SC
{
namespace
{
struct AsyncExecutionReadinessTest : AsyncExecutionTestCase
{
    struct Fixture
    {
        AsyncEventLoop        loop;
        AsyncExecutionContext context{loop};
        PipeDescriptor        pipe;

        Result create(AsyncEventLoop::Options options)
        {
            SC_TRY(loop.create(options));
            SC_TRY(context.create());
            PipeOptions pipeOptions;
            pipeOptions.blocking = false;
            SC_TRY(pipe.createPipe(pipeOptions));
            return Result(true);
        }
        Result close()
        {
            SC_TRY(context.close());
            SC_TRY(pipe.close());
            return loop.close();
        }
    };
    AsyncEventLoop::Options options;

    AsyncExecutionReadinessTest(TestReport& report) : AsyncExecutionTestCase(report, "AsyncExecutionReadinessTest")
    {
        using Backend    = AsyncEventLoop::Options::ApiType;
        const bool uring = AsyncEventLoop::tryProbingIOUring();
        for (int pass = 0; pass < (uring ? 2 : 1); ++pass)
        {
            options.apiType =
                uring ? (pass == 0 ? Backend::ForceUseEpoll : Backend::ForceUseIoUring) : Backend::Automatic;
            if (test_section("readiness errors and prestop"))
                errors();
            if (test_section("readiness observes without consuming"))
                ready();
            if (test_section("readiness terminal storage and cancellation"))
            {
                terminal(false, false);
                terminal(true, false);
                terminal(false, true);
            }
        }
    }
    void errors()
    {
        Fixture fixture;
        SC_TEST_EXPECT(fixture.create(options));
        FileDescriptor invalid;
        const Result   error = consumeBounded(fixture.context, fixture.context.filePoll(invalid));
        SC_TEST_EXPECT(error.isError(AsyncResultCategory, AsyncError::InvalidFileHandle));
        ExecutionStopSource stop;
        stop.request_stop();
        const Result stopped = fixture.context.run(fixture.context.filePoll(invalid), stop.get_token());
        SC_TEST_EXPECT(stopped.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(fixture.close());
    }
    void ready()
    {
        Fixture fixture;
        SC_TEST_EXPECT(fixture.create(options));
        auto& io = fixture.context;
        if (SC_PLATFORM_WINDOWS)
        {
            const Result status = consumeBounded(io, io.filePoll(fixture.pipe.readPipe));
            SC_TEST_EXPECT(status.isError(AsyncResultCategory, AsyncError::OperationUnsupported));
        }
        else
        {
            Result written{false};
            auto   producer =
                io.sleep({1}) | ex::then([&]() noexcept { written = fixture.pipe.writePipe.writeString("x"); });
            SC_TEST_EXPECT(consumeBounded(io, ex::when_all(io.filePoll(fixture.pipe.readPipe), move(producer))));
            SC_TEST_EXPECT(written);
            // A second observation still sees the same unread byte. Each wait is one-shot.
            SC_TEST_EXPECT(consumeBounded(io, io.filePoll(fixture.pipe.readPipe)));
            char       byte = 0;
            Span<char> data;
            SC_TEST_EXPECT(fixture.pipe.readPipe.read({&byte, 1}, data));
            SC_TEST_EXPECT(data.sizeInBytes() == 1 and byte == 'x');
        }
        SC_TEST_EXPECT(fixture.close());
    }
    void terminal(bool invalid, bool cancel)
    {
        Fixture fixture;
        SC_TEST_EXPECT(fixture.create(options));
        auto& io = fixture.context;
        if (not invalid and not cancel and not SC_PLATFORM_WINDOWS)
            SC_TEST_EXPECT(fixture.pipe.writePipe.writeString("x"));
        FileDescriptor   invalidFile;
        auto             sender = io.filePoll(invalid ? invalidFile : fixture.pipe.readPipe);
        ExecutionOutcome outcome;
        outcome.loop = &fixture.loop;
        ExecutionStopSource stop;
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
        placementNew(*lifetime.operation, io, sender.setup, ExecutionReceiver{outcome, stop.get_token()});
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
        if (cancel and not invalid and not SC_PLATFORM_WINDOWS)
        {
            SC_TEST_EXPECT(fixture.loop.runNoWait());
            SC_TEST_EXPECT(fixture.loop.runNoWait());
            SC_TEST_EXPECT(not lifetime.destroyed and lifetime.operation->request.isActive());
            Thread producer;
            SC_TEST_EXPECT(producer.start([&stop](Thread&) { stop.request_stop(); }));
            SC_TEST_EXPECT(producer.join());
        }
        SC_TEST_EXPECT(runBounded(fixture.loop));
        SC_TEST_EXPECT(lifetime.destroyed);
        if (invalid or SC_PLATFORM_WINDOWS)
        {
            const auto expected = invalid ? AsyncError::InvalidFileHandle : AsyncError::OperationUnsupported;
            SC_TEST_EXPECT(outcome.errors == 1 and outcome.values == 0 and outcome.stopped == 0);
            SC_TEST_EXPECT(outcome.error.isError(AsyncResultCategory, expected));
        }
        else if (cancel)
        {
            SC_TEST_EXPECT(outcome.stopped == 1 and outcome.errors == 0 and outcome.values == 0);
            // Write after stopped delivery and freeing the old request. A stale watcher would target freed storage.
            SC_TEST_EXPECT(fixture.pipe.writePipe.writeString("y"));
            SC_TEST_EXPECT(consumeBounded(io, io.sleep({5})));
            SC_TEST_EXPECT(consumeBounded(io, io.filePoll(fixture.pipe.readPipe)));
            char       byte = 0;
            Span<char> data;
            SC_TEST_EXPECT(fixture.pipe.readPipe.read({&byte, 1}, data));
            SC_TEST_EXPECT(data.sizeInBytes() == 1 and byte == 'y');
        }
        else
            SC_TEST_EXPECT(outcome.values == 1 and outcome.errors == 0 and outcome.stopped == 0);
        SC_TEST_EXPECT(fixture.close());
    }
};
} // namespace
void runAsyncExecutionReadinessTest(TestReport& report) { AsyncExecutionReadinessTest test(report); }
} // namespace SC
