// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Strings/Path.h"
#include "Libraries/Time/Time.h"
#include "Tests/Libraries/AsyncExecution/AsyncExecutionTestSupport.h"
#include <stdlib.h>
#include <string.h>
namespace SC
{
namespace
{
struct AsyncExecutionFileSystemTest : AsyncExecutionTestCase
{
    struct Fixture
    {
        AsyncEventLoop        loop;
        AsyncExecutionContext io{loop};
        ThreadPool            pool;
        FileSystem            fs;
        Result                create(AsyncEventLoop::Options options, StringSpan root)
        {
            SC_TRY(loop.create(options));
            SC_TRY(io.create());
            SC_TRY(pool.create(1));
            return Result::Explicit(fs.init(root));
        }
        Result close()
        {
            SC_TRY(io.close());
            SC_TRY(loop.close());
            return Result::Explicit(pool.destroy());
        }
    };
    AsyncEventLoop::Options options;
    AsyncExecutionFileSystemTest(TestReport& report) : AsyncExecutionTestCase(report, "AsyncExecutionFileSystemTest")
    {
        if (not report.isTestEnabled("AsyncExecutionFileSystemTest"))
            return;
        Atomic<bool> done = false;
        Thread       guard;
        SC_TEST_EXPECT(guard.start(
            [&done](Thread&)
            {
                const auto start = Time::Monotonic::now().getMonotonicMilliseconds();
                while (not done.load() and Time::Monotonic::now().getMonotonicMilliseconds() - start < 10000)
                    Thread::Sleep(1);
                if (not done.load())
                    ::_Exit(124);
            }));
        using Backend    = AsyncEventLoop::Options::ApiType;
        const bool uring = AsyncEventLoop::tryProbingIOUring();
        for (int pass = 0; pass < (uring ? 2 : 1); ++pass)
        {
            options.apiType =
                uring ? (pass == 0 ? Backend::ForceUseEpoll : Backend::ForceUseIoUring) : Backend::Automatic;
            for (int forced = 0; forced < 2; ++forced)
            {
                const auto mode = forced ? AsyncThreadPoolMode::ForceThreadPool : AsyncThreadPoolMode::NativePreferred;
                if (test_section("filesystem owned file composition"))
                    composition(mode);
                if (test_section("filesystem input and close ownership errors"))
                    errors(mode);
                if (test_section("filesystem submitted open stops after cleanup"))
                    submittedOpen(mode);
            }
            if (test_section("filesystem stopped worker joins and terminal storage"))
                stoppedWorkers();
        }
        done.store(true);
        SC_TEST_EXPECT(guard.join());
    }
    StringPath path(StringSpan name)
    {
        StringPath result;
        SC_TEST_EXPECT(Path::join(result, {report.applicationRootDirectory.view(), name}));
        return result;
    }
    void composition(AsyncThreadPoolMode mode)
    {
        Fixture f;
        SC_TEST_EXPECT(f.create(options, report.applicationRootDirectory.view()));
        auto input = path("AE-FS-composition.bin");
        auto open  = f.io.fsOpen(f.pool, input.view(), FileOpen::WriteRead, mode);
        SC_TEST_EXPECT(input.assign("discarded-path-snapshot"));
        AsyncExecutionFile file;
        SC_TEST_EXPECT(consumeBounded(f.io, move(open), file));
        SC_TEST_EXPECT(file.file.isValid());
        size_t written = 0;
        SC_TEST_EXPECT(consumeBounded(f.io, f.io.fsWrite(f.pool, file.file, {"hello", 5}, 0, mode), written));
        SC_TEST_EXPECT(written == 5);
        char                     buffer[8] = {};
        AsyncExecutionReadResult read;
        SC_TEST_EXPECT(consumeBounded(f.io, f.io.fsRead(f.pool, file.file, {buffer, 3}, 1, mode), read));
        SC_TEST_EXPECT(read.data.sizeInBytes() == 3 and not read.endOfFile and memcmp(buffer, "ell", 3) == 0);
        SC_TEST_EXPECT(consumeBounded(f.io, f.io.fsRead(f.pool, file.file, buffer, 5, mode), read));
        SC_TEST_EXPECT(read.data.empty() and read.endOfFile);
        SC_TEST_EXPECT(consumeBounded(f.io, f.io.fsClose(f.pool, file.file, mode)));
        SC_TEST_EXPECT(not file.file.isValid());
        SC_TEST_EXPECT(f.fs.removeFile("AE-FS-composition.bin"));
        // Exercise move-only file ownership through standard let_value, rather than output assignment only.
        bool closed = false;
        auto graph  = f.io.fsOpen(f.pool, path("AE-FS-composition.bin").view(), FileOpen::WriteRead, mode) |
                     ex::let_value(
                         [&](auto& opened) noexcept
                         {
                             return f.io.fsClose(f.pool, opened.file, mode) |
                                    ex::then([&opened, &closed]() noexcept { closed = not opened.file.isValid(); });
                         });
        SC_TEST_EXPECT(consumeBounded(f.io, move(graph)));
        SC_TEST_EXPECT(closed);
        SC_TEST_EXPECT(f.fs.removeFile("AE-FS-composition.bin"));
        SC_TEST_EXPECT(f.close());
    }
    void errors(AsyncThreadPoolMode mode)
    {
        Fixture f;
        SC_TEST_EXPECT(f.create(options, report.applicationRootDirectory.view()));
        FileDescriptor invalid;
        char           buffer[4]    = {};
        const Result   invalidClose = consumeBounded(f.io, f.io.fsClose(f.pool, invalid, mode));
        SC_TEST_EXPECT(invalidClose.isError(AsyncResultCategory, AsyncError::InvalidFileHandle));
        const Result invalidRead = consumeBounded(f.io, f.io.fsRead(f.pool, invalid, buffer, 0, mode));
        SC_TEST_EXPECT(invalidRead.isError(AsyncResultCategory, AsyncError::InvalidFileHandle));
        const Result invalidPath = consumeBounded(f.io, f.io.fsOpen(f.pool, {}, {}, mode));
        SC_TEST_EXPECT(invalidPath.isError(AsyncResultCategory, AsyncError::InvalidPath));
        ExecutionStopSource stop;
        stop.request_stop();
        const Result prestop = f.io.run(f.io.fsClose(f.pool, invalid, mode), stop.get_token());
        SC_TEST_EXPECT(prestop.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        const auto   untouched = path("AE-FS-prestop.bin");
        const Result noOpen =
            f.io.run(f.io.fsOpen(f.pool, untouched.view(), FileOpen::WriteRead, mode), stop.get_token());
        SC_TEST_EXPECT(noOpen.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(not f.fs.exists("AE-FS-prestop.bin"));
        FileDescriptor file;
        SC_TEST_EXPECT(file.openForWriteToDevNull());
        const Result noClose = f.io.run(f.io.fsClose(f.pool, file, mode), stop.get_token());
        SC_TEST_EXPECT(noClose.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(file.isValid());
        const Result empty = consumeBounded(f.io, f.io.fsWrite(f.pool, file, {}, 0, mode));
        SC_TEST_EXPECT(empty.isError(AsyncResultCategory, AsyncError::EmptyBuffer));
        const Result offset = consumeBounded(f.io, f.io.fsRead(f.pool, file, buffer, uint64_t(-1), mode));
        SC_TEST_EXPECT(offset.isError(AsyncExecutionResultCategory, AsyncExecutionError::InvalidFileOffset));
        // A rejected pool submission has not closed anything and must preserve the caller's ownership.
        ThreadPool   missingPool;
        const Result queue =
            consumeBounded(f.io, f.io.fsClose(missingPool, file, AsyncThreadPoolMode::ForceThreadPool));
        SC_TEST_EXPECT(queue.isError(ThreadingResultCategory, ThreadingError::ThreadPoolNotCreated));
        SC_TEST_EXPECT(file.isValid());
        SC_TEST_EXPECT(file.writeString("still open"));
        // A real close attempt that fails must invalidate the descriptor; retrying an ambiguous close is unsafe.
        FileDescriptor::Handle stale = FileDescriptor::Invalid;
        SC_TEST_EXPECT(file.get(stale, Result(false)));
        SC_TEST_EXPECT(file.close());
        FileDescriptor closed(stale);
        const Result   attempted = consumeBounded(f.io, f.io.fsClose(f.pool, closed, mode));
        SC_TEST_EXPECT(not attempted);
        SC_TEST_EXPECT(attempted.isError(FileResultCategory, FileError::CloseFailed) or
                       attempted.isError(AsyncResultCategory, AsyncError::EventCompletionFailed));
        SC_TEST_EXPECT(not closed.isValid());
        SC_TEST_EXPECT(f.close());
    }
    void submittedOpen(AsyncThreadPoolMode mode)
    {
        Fixture f;
        SC_TEST_EXPECT(f.create(options, report.applicationRootDirectory.view()));
        const auto       name = path("AE-FS-submitted-open.bin");
        ExecutionOutcome outcome;
        outcome.loop = &f.loop;
        ExecutionStopSource stop;
        auto                sender    = f.io.fsOpen(f.pool, name.view(), FileOpen::WriteRead, mode);
        auto                operation = ex::connect(move(sender), ExecutionReceiver{outcome, stop.get_token()});
        bool                cleaned   = false;
        outcome.onStopped             = [&operation, &cleaned]() { cleaned = not operation.setup.opened.isValid(); };
        ex::start(operation);
        SC_TEST_EXPECT(f.loop.runNoWait());
        SC_TEST_EXPECT(operation.stopRegistered);
        stop.request_stop();
        SC_TEST_EXPECT(runBounded(f.loop));
        SC_TEST_EXPECT(outcome.stopped == 1 and outcome.values == 0 and outcome.errors == 0 and cleaned);
        SC_TEST_EXPECT(f.fs.exists("AE-FS-submitted-open.bin"));
        SC_TEST_EXPECT(f.fs.removeFile("AE-FS-submitted-open.bin"));
        SC_TEST_EXPECT(f.close());
    }
    template <typename Sender>
    void destroyOnCompletion(Fixture& f, Sender sender, bool stopped, bool shouldFail)
    {
        ExecutionOutcome outcome;
        outcome.loop = &f.loop;
        ExecutionStopSource stop;
        using Operation = decltype(ex::connect(move(sender), ExecutionReceiver{outcome, stop.get_token()}));
        struct Lifetime
        {
            void*      allocation = nullptr;
            Operation* operation  = nullptr;
            bool       destroyed  = false;
            bool       cleaned    = true;
        } lifetime;
        lifetime.allocation = ::malloc(sizeof(Operation) + alignof(Operation) - 1);
        SC_TEST_EXPECT(lifetime.allocation != nullptr);
        const uintptr_t aligned = (reinterpret_cast<uintptr_t>(lifetime.allocation) + alignof(Operation) - 1) &
                                  ~(uintptr_t(alignof(Operation)) - 1);
        lifetime.operation = reinterpret_cast<Operation*>(aligned);
        placementNew(*lifetime.operation, f.io, move(sender.setup), ExecutionReceiver{outcome, stop.get_token()});
        auto destroy = [&lifetime, stopped]()
        {
            if constexpr (requires { lifetime.operation->setup.opened; })
                if (stopped)
                    lifetime.cleaned = not lifetime.operation->setup.opened.isValid();
            lifetime.operation->~Operation();
            ::free(lifetime.allocation);
            lifetime.destroyed = true;
        };
        outcome.onValue   = destroy;
        outcome.onError   = destroy;
        outcome.onStopped = destroy;
        struct Gate
        {
            EventObject entered, release;
        } gate;
        ThreadPoolTask blocker;
        if (stopped)
        {
            blocker.function = [&gate]()
            {
                gate.entered.signal();
                gate.release.wait();
            };
            SC_TEST_EXPECT(f.pool.queueTask(blocker));
            gate.entered.wait();
        }
        ex::start(*lifetime.operation);
        SC_TEST_EXPECT(f.loop.runNoWait());
        SC_TEST_EXPECT(f.loop.runNoWait());
        if (stopped)
        {
            stop.request_stop();
            SC_TEST_EXPECT(f.loop.runNoWait());
            SC_TEST_EXPECT(not lifetime.destroyed);
            SC_TEST_EXPECT(consumeBounded(f.io, f.io.sleep({1})));
            SC_TEST_EXPECT(not lifetime.destroyed);
            gate.release.signal();
            SC_TEST_EXPECT(f.pool.waitForTask(blocker));
        }
        if (not lifetime.destroyed)
            SC_TEST_EXPECT(runBounded(f.loop));
        SC_TEST_EXPECT(lifetime.destroyed and lifetime.cleaned);
        SC_TEST_EXPECT(outcome.values == (not stopped and not shouldFail ? 1 : 0));
        SC_TEST_EXPECT(outcome.errors == (shouldFail ? 1 : 0));
        SC_TEST_EXPECT(outcome.stopped == (stopped and not shouldFail ? 1 : 0));
        SC_TEST_EXPECT(f.loop.runNoWait());
    }
    void stoppedWorkers()
    {
        Fixture f;
        SC_TEST_EXPECT(f.create(options, report.applicationRootDirectory.view()));
        constexpr auto forced = AsyncThreadPoolMode::ForceThreadPool;
        const auto     name   = path("AE-FS-stopped.bin");
        destroyOnCompletion(f, f.io.fsOpen(f.pool, name.view(), FileOpen::WriteRead, forced), true, false);
        SC_TEST_EXPECT(f.fs.exists("AE-FS-stopped.bin"));
        SC_TEST_EXPECT(f.fs.removeFile("AE-FS-stopped.bin"));
        destroyOnCompletion(f, f.io.fsOpen(f.pool, name.view(), FileOpen::WriteRead, forced), false, false);
        SC_TEST_EXPECT(f.fs.removeFile("AE-FS-stopped.bin"));
        ThreadPool missingPool;
        destroyOnCompletion(f, f.io.fsOpen(missingPool, name.view(), FileOpen::WriteRead, forced), false, true);
        SC_TEST_EXPECT(not f.fs.exists("AE-FS-stopped.bin"));
        FileDescriptor file;
        SC_TEST_EXPECT(file.open(name.view(), FileOpen::WriteRead));
        char data[4] = {'d', 'a', 't', 'a'};
        destroyOnCompletion(f, f.io.fsWrite(f.pool, file, data, 0, forced), true, false);
        char buffer[4] = {};
        destroyOnCompletion(f, f.io.fsRead(f.pool, file, buffer, 0, forced), true, false);
        SC_TEST_EXPECT(memcmp(buffer, data, 4) == 0);
        memset(buffer, '?', sizeof(buffer));
        SC_TEST_EXPECT(consumeBounded(f.io, f.io.sleep({1})));
        for (char byte : buffer)
            SC_TEST_EXPECT(byte == '?');
        destroyOnCompletion(f, f.io.fsClose(f.pool, file, forced), true, false);
        SC_TEST_EXPECT(not file.isValid());
        SC_TEST_EXPECT(f.fs.removeFile("AE-FS-stopped.bin"));
        SC_TEST_EXPECT(f.close());
    }
};
} // namespace
void runAsyncExecutionFileSystemTest(TestReport& report) { AsyncExecutionFileSystemTest test(report); }
} // namespace SC
