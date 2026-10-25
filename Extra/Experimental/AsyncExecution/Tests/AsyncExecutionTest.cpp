// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Extra/Experimental/AsyncExecution/AsyncExecution.h"
#if !SC_ASYNC_EXECUTION_AVAILABLE
#error                                                                                                                 \
    "Sender/receiver <execution> is unavailable. Set SC_ASYNC_EXECUTION_STDEXEC_INCLUDE to a stdexec include directory."
#endif
#include "Extra/Experimental/AsyncExecution/Examples/AsyncExecutionDemo/Base64.h"
#include "Libraries/Common/Deferred.h"
#include "Libraries/Common/PlacementNew.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Time/Time.h"
#include <signal.h>
#include <stdlib.h> // _Exit (independent native-pipe deadline)
#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

#include "AsyncExecutionTestSupport.h"

namespace SC
{
namespace
{
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
#if SC_PLATFORM_WINDOWS
static LONG WINAPI scopeAssertionExit(EXCEPTION_POINTERS* exception)
{
    if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
        ::_Exit(77);
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
struct ScopeTestAllocator
{
    alignas(256) char bytes[32768];
    Atomic<bool> busy        = false;
    Atomic<int>  allocations = 0;
    Atomic<int>  releases    = 0;
    Atomic<bool> correct     = true;
    size_t       size        = 0;
    size_t       alignment   = 0;

    AsyncExecutionScopeAllocator callbacks()
    {
        return {this,
                [](void* user, size_t size, size_t alignment) noexcept -> void*
                {
                    auto& state = *static_cast<ScopeTestAllocator*>(user);
                    state.allocations.fetch_add(1);
                    if (size > sizeof(state.bytes) or alignment > alignof(ScopeTestAllocator) or
                        state.busy.exchange(true))
                        return nullptr;
                    state.size      = size;
                    state.alignment = alignment;
                    return state.bytes;
                },
                [](void* user, void* memory, size_t size, size_t alignment) noexcept
                {
                    auto& state = *static_cast<ScopeTestAllocator*>(user);
                    state.correct.store(state.correct.load() and memory == state.bytes and size == state.size and
                                        alignment == state.alignment and state.busy.load());
                    state.busy.store(false);
                    state.releases.fetch_add(1);
                }};
    }
};

#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // Intentional padding exercises over-aligned connected operation allocation.
#endif
struct ScopeProbeSender
{
    using sender_concept = ex::sender_tag;
    bool* aligned;
    bool* destroyed;
    bool* scopeEnvironment;
    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        return ex::completion_signatures<ex::set_value_t()>{};
    }
    template <typename Receiver>
    struct alignas(256) Operation
    {
        using operation_state_concept = ex::operation_state_tag;
        Receiver receiver;
        bool*    aligned;
        bool*    destroyed;
        bool*    scopeEnvironment;
        Operation(Receiver receiver, bool* aligned, bool* destroyed, bool* scopeEnvironment)
            : receiver(move(receiver)), aligned(aligned), destroyed(destroyed), scopeEnvironment(scopeEnvironment)
        {}
        Operation(const Operation&) = delete;
        ~Operation() { *destroyed = true; }
        void start() noexcept
        {
            *aligned = reinterpret_cast<size_t>(this) % 256 == 0;
            if constexpr (requires { detail::AsyncExecutionScopeQuery{}(ex::get_env(receiver)); })
            {
                auto association  = detail::AsyncExecutionScopeQuery{}(ex::get_env(receiver)).try_associate();
                *scopeEnvironment = bool(association);
            }
            ex::set_value(move(receiver));
        }
    };
    template <typename Receiver>
    auto connect(Receiver receiver) const noexcept
    {
        return Operation<Receiver>(move(receiver), aligned, destroyed, scopeEnvironment);
    }
};

#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif

struct ScopeHeldState
{
    Function<void()> complete;
    Atomic<int>      destroyed = 0;
};
struct ScopeHeldSender
{
    using sender_concept = ex::sender_tag;
    ScopeHeldState& state;
    int&            moves;
    ScopeHeldSender(ScopeHeldState& state, int& moves) : state(state), moves(moves) {}
    ScopeHeldSender(ScopeHeldSender&& other) noexcept : state(other.state), moves(other.moves) { ++moves; }
    ScopeHeldSender(const ScopeHeldSender&) = delete;
    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        return ex::completion_signatures<ex::set_value_t()>{};
    }
    template <typename Receiver>
    struct Operation
    {
        using operation_state_concept = ex::operation_state_tag;
        ScopeHeldState& state;
        Receiver        receiver;
        Operation(ScopeHeldState& state, Receiver receiver) : state(state), receiver(move(receiver)) {}
        Operation(const Operation&) = delete;
        ~Operation() { state.destroyed.fetch_add(1); }
        void start() noexcept
        {
            state.complete = [self = this]() noexcept { ex::set_value(move(self->receiver)); };
        }
    };
    template <typename Receiver>
    auto connect(Receiver receiver) && noexcept
    {
        return Operation<Receiver>(state, move(receiver));
    }
};

#endif // SC_ASYNC_EXECUTION_SCOPES_AVAILABLE

struct AsyncExecutionTest : AsyncExecutionTestCase
{
    AsyncExecutionTest(TestReport& report) : AsyncExecutionTestCase(report, "AsyncExecutionTest")
    {
        using Timer = decltype(std::declval<AsyncExecutionContext>().sleep({1}));
        static_assert(ex::sender<Timer>);
        static_assert(ex::scheduler<AsyncExecutionScheduler>);
        static_assert(ex::scheduler<AsyncExecutionPoolScheduler>);
        using Operation = decltype(ex::connect(std::declval<Timer>(), std::declval<ExecutionReceiver>()));
        static_assert(ex::operation_state<Operation>);
        static_assert(not std::is_move_constructible_v<Operation>);

        if (test_section("windows sender signal child", TestCase::Execute::OnlyExplicit))
            windowsSignalChild();
        if (test_section("borrowed process exits and cancellation"))
            processWatches();
        if (test_section("one delivery signals and cancellation"))
            signalWatches();
        if (test_section("borrowed file ranges and complete transfers"))
            fileRanges();
        if (test_section("borrowed file send native and forced pool"))
        {
            fileSenders(false);
            fileSenders(true);
        }
        if (test_section("native borrowed files ignore unrelated busy pool"))
            fileNativeProgress();
        if (test_section("native pipe file transfers"))
            filePipes();
        if (test_section("UDP datagrams and cancellation"))
        {
            datagrams(false);
            datagrams(true);
        }
        if (test_section("borrowed TCP accept connect and scatter transfers"))
            borrowedTCP();
        if (test_section("TCP exact and line receive boundaries"))
            tcpReceiveBoundaries();
        if (test_section("TCP cancellation and terminal destruction"))
            tcpCancellation();
        if (test_section("TCP cursor short writes and progress errors"))
            tcpCursors();
        if (test_section("generic fallible transforms"))
            fallibleTransforms();
        if (test_section("standard composition and retained values"))
            composition();
        if (test_section("borrowed loop integration and external consumer"))
            loopIntegration();
        if (test_section("pool scheduler preserves loop progress"))
            poolProgress();
        if (test_section("pool scheduler stop races and terminal destruction"))
            poolLifetime();
        if (test_section("owning facade files pool and repeated runs"))
            owningFacade();
        if (test_section("observable owning close and external driving"))
            owningClose();
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
        if (test_section("typed scope storage capacity reuse and lifetime"))
            scopeTypedStorage();
        if (test_section("structured scope values errors and allocation"))
            scopeBasics();
        if (test_section("structured scope stop join and pool progress"))
            scopeProgress();
        if (test_section("structured scope concurrent join and terminal destruction"))
            scopeJoinLifetime();
        if (test_section("structured scope active destruction rejection"))
            scopeDestruction();
        if (test_section("scope active destruction child", TestCase::Execute::OnlyExplicit))
            scopeDestructionChild();
#endif
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

    void fileRanges()
    {
        AsyncEventLoop loop;
        ThreadPool     pool;
        SC_TEST_EXPECT(loop.create());
        SC_TEST_EXPECT(pool.create(1));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        StringPath path;
        SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory.view(), "AsyncExecutionRanges.txt"}));
        FileDescriptor file;
        SC_TEST_EXPECT(file.open(path.view(), FileOpen::WriteRead));
        SC_TEST_EXPECT(file.write(StringView("......").toCharSpan()));
        AsyncExecutionFileOptions options{&pool};
        size_t                    written = 0;
        SC_TEST_EXPECT(
            consumeBounded(context, context.fileWriteAt(file, 1, StringView("AB").toCharSpan(), options), written));
        SC_TEST_EXPECT(written == 2);
        Span<const char> parts[] = {{}, StringView("C").toCharSpan(), {}, StringView("DE").toCharSpan(), {}};
        SC_TEST_EXPECT(consumeBounded(context, context.fileWriteAt(file, 3, parts, options), written));
        SC_TEST_EXPECT(written == 3 and parts[1].sizeInBytes() == 1 and parts[3].sizeInBytes() == 2);
        char                     small[6] = {};
        AsyncExecutionReadResult result;
        SC_TEST_EXPECT(consumeBounded(context, context.fileReadExactAt(file, 0, small, options), result));
        SC_TEST_EXPECT(result.data.sizeInBytes() == 6 and memcmp(small, ".ABCDE", 6) == 0);
        char                     extra[8] = {};
        AsyncExecutionReadResult progress;
        options.useOffset = true;
        options.offset    = 2;
        SC_TEST_EXPECT(
            consumeBounded(context, context.fileReadUntilFullOrEOF(file, extra, options, &progress), result));
        SC_TEST_EXPECT(result.data.sizeInBytes() == 4 and result.endOfFile and progress.endOfFile);
        SC_TEST_EXPECT(memcmp(extra, "BCDE", 4) == 0);
        SC_TEST_EXPECT(consumeBounded(context, context.fileReadExact(file, extra, options, &progress), result)
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::UnexpectedEndOfFile));
        SC_TEST_EXPECT(progress.data.sizeInBytes() == 4 and progress.endOfFile);
        ExecutionTestBytes source, reply;
        SC_TEST_EXPECT(source.resize(35000));
        SC_TEST_EXPECT(reply.resize(35000));
        for (size_t index = 0; index < source.size(); ++index)
            source.bytes()[index] = static_cast<char>('A' + index % 23);
        const auto       bytes        = source.bytes();
        Span<const char> largeParts[] = {{bytes.data(), 18000}, {}, {bytes.data() + 18000, 17000}};
        size_t           transferred  = 99;
        auto             write        = context.fileWriteAllAt(file, 0, largeParts, options, &transferred);
        SC_TEST_EXPECT(transferred == 99);
        SC_TEST_EXPECT(consumeBounded(context, move(write), written));
        SC_TEST_EXPECT(written == 35000 and transferred == 35000);
        SC_TEST_EXPECT(largeParts[0].sizeInBytes() == 18000 and largeParts[2].sizeInBytes() == 17000);
        SC_TEST_EXPECT(consumeBounded(context, context.fileReadExactAt(file, 0, reply.bytes(), options), result));
        SC_TEST_EXPECT(result.data.sizeInBytes() == 35000 and
                       memcmp(source.bytes().data(), reply.bytes().data(), 35000) == 0);
        SC_TEST_EXPECT(consumeBounded(
            context, context.fileWriteAllAt(file, 35000, StringView("!").toCharSpan(), options), written));
        char last[1];
        SC_TEST_EXPECT(consumeBounded(context, context.fileReadAt(file, 35000, last, options), result));
        SC_TEST_EXPECT(last[0] == '!' and written == 1);
        // Legacy calls remain absolute-offset, native-preferred operations.
        SC_TEST_EXPECT(consumeBounded(context, context.fileWrite(file, StringView("L").toCharSpan(), pool), written));
        SC_TEST_EXPECT(consumeBounded(context, context.fileRead(file, last, pool), result));
        SC_TEST_EXPECT(last[0] == 'L');
        SC_TEST_EXPECT(consumeBounded(context, context.fileReadAt(file, uint64_t(-1), last, options))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::InvalidFileOffset));
        AsyncExecutionFileOptions missingPool;
        missingPool.threadPoolMode = AsyncThreadPoolMode::ForceThreadPool;
        SC_TEST_EXPECT(consumeBounded(context, context.fileRead(file, last, missingPool))
                           .isError(AsyncResultCategory, AsyncError::MissingThreadPool));
        FileDescriptor invalid;
        SC_TEST_EXPECT(consumeBounded(context, context.fileReadExact(invalid, Span<char>{}), result));
        SC_TEST_EXPECT(result.data.empty() and not result.endOfFile);
        SC_TEST_EXPECT(consumeBounded(context, context.fileWriteAll(invalid, Span<const char>{}), written));
        SC_TEST_EXPECT(written == 0);
        SC_TEST_EXPECT(consumeBounded(context, context.fileRead(invalid, last))
                           .isError(AsyncResultCategory, AsyncError::InvalidFileHandle));
        SC_TEST_EXPECT(consumeBounded(context, context.fileWrite(invalid, StringView("x").toCharSpan()))
                           .isError(AsyncResultCategory, AsyncError::InvalidFileHandle));
        if constexpr (sizeof(size_t) > sizeof(uint32_t))
        {
            Span<const char> oversized{last, size_t(uint64_t(1) << 32)};
            SC_TEST_EXPECT(consumeBounded(context, context.fileWrite(file, oversized, options))
                               .isError(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow));
        }
        detail::AsyncExecutionFileReadMany exact{invalid, extra, {}, &progress, true};
        exact.initialize();
        SC_TEST_EXPECT(exact.advanceBytes(0, false).isError(AsyncExecutionResultCategory,
                                                            AsyncExecutionError::FileReadNoProgress));
        SC_TEST_EXPECT(exact.advanceBytes(2, true).isError(AsyncExecutionResultCategory,
                                                           AsyncExecutionError::UnexpectedEndOfFile));
        SC_TEST_EXPECT(progress.data.sizeInBytes() == 2 and progress.endOfFile);
        size_t                             partial = 0;
        detail::AsyncExecutionFileWriteAll cursor{invalid, StringView("abcd").toCharSpan(), {}, {}, &partial, true};
        cursor.initialize();
        SC_TEST_EXPECT(cursor.advanceBytes(1));
        SC_TEST_EXPECT(cursor.advanceBytes(0).isError(AsyncResultCategory, AsyncError::FileWriteIncomplete));
        SC_TEST_EXPECT(cursor.advanceBytes(4).isError(AsyncResultCategory, AsyncError::FileWriteIncomplete));
        SC_TEST_EXPECT(cursor.advanceBytes(3));
        SC_TEST_EXPECT(partial == 4 and not cursor.hasRemaining());
        SC_TEST_EXPECT(pool.waitForAllTasks());
        SC_TEST_EXPECT(file.close());
        SC_TEST_EXPECT(FileSystem::Operations::removeFile(path.view()));
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
        SC_TEST_EXPECT(pool.destroy());
    }

    void fileSenders(bool forcePool)
    {
        AsyncEventLoop loop;
        ThreadPool     pool;
        SC_TEST_EXPECT(loop.create());
        SC_TEST_EXPECT(pool.create(1));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SocketDescriptor         listener, client;
        AsyncExecutionConnection accepted;
        SC_TEST_EXPECT(tcpPair(context, listener, client, accepted, report.mapPort(uint16_t(5115 + forcePool))));
        StringPath path;
        SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory.view(), "AsyncExecutionFileSend.txt"}));
        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.write(path.view(), StringView("xxpayloadyy").toCharSpan()));
        FileDescriptor file;
        FileOpen       openMode;
        openMode.mode     = FileOpen::Read;
        openMode.blocking = forcePool;
        SC_TEST_EXPECT(file.open(path.view(), openMode));
        if (not forcePool)
            SC_TEST_EXPECT(loop.associateExternallyCreatedFileDescriptor(file));
        AsyncExecutionFileSendOptions options;
        options.offset     = 2;
        options.length     = 7;
        options.threadPool = &pool;
        options.threadPoolMode =
            forcePool ? AsyncThreadPoolMode::ForceThreadPool : AsyncThreadPoolMode::NativePreferred;
        AsyncExecutionFileSendResult sent, progress{99, true, true};
        char                         reply[7];
        AsyncExecutionReadResult     read;
        auto                         transfer = context.fileSend(file, client, options, &progress);
        SC_TEST_EXPECT(progress.bytesTransferred == 99);
        SC_TEST_EXPECT(consumeBounded(
            context, ex::when_all(move(transfer), context.receiveExact(accepted.socket, reply)), sent, read));
        SC_TEST_EXPECT(sent.bytesTransferred == 7 and sent.complete and progress.complete);
        SC_TEST_EXPECT(progress.bytesTransferred == 7 and read.data.sizeInBytes() == 7 and
                       memcmp(reply, "payload", 7) == 0);
#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(sent.usedZeroCopy == not forcePool);
#else
        SC_TEST_EXPECT(sent.usedZeroCopy);
#endif
        // Cross the native splice pipe/chunk boundary with an independent explicit range.
        SC_TEST_EXPECT(file.close());
        ExecutionTestBytes source, received;
        SC_TEST_EXPECT(source.resize(131081));
        SC_TEST_EXPECT(received.resize(131075));
        for (size_t index = 0; index < source.size(); ++index)
            source.bytes()[index] = static_cast<char>('a' + index % 23);
        SC_TEST_EXPECT(fs.write(path.view(), source.bytes()));
        SC_TEST_EXPECT(file.open(path.view(), openMode));
        if (not forcePool)
            SC_TEST_EXPECT(loop.associateExternallyCreatedFileDescriptor(file));
        options.offset = 3;
        options.length = received.size();
        SC_TEST_EXPECT(consumeBounded(context,
                                      ex::when_all(context.fileSend(file, client, options, &progress),
                                                   context.receiveExact(accepted.socket, received.bytes())),
                                      sent, read));
        SC_TEST_EXPECT(sent.complete and sent.bytesTransferred == received.size() and progress.complete);
        SC_TEST_EXPECT(read.data.sizeInBytes() == received.size() and
                       memcmp(source.bytes().data() + 3, received.bytes().data(), received.size()) == 0);
        auto invalidOptions   = options;
        invalidOptions.offset = -1;
        SC_TEST_EXPECT(consumeBounded(context, context.fileSend(file, client, invalidOptions))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::InvalidFileOffset));
        invalidOptions.offset = int64_t(uint64_t(-1) >> 1);
        invalidOptions.length = 2;
        SC_TEST_EXPECT(consumeBounded(context, context.fileSend(file, client, invalidOptions))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::InvalidFileOffset));
        invalidOptions        = options;
        invalidOptions.length = 2147483647;
        SC_TEST_EXPECT(consumeBounded(context, context.fileSend(file, client, invalidOptions))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow));
        invalidOptions        = options;
        invalidOptions.length = 0;
        SC_TEST_EXPECT(consumeBounded(context, context.fileSend(file, client, invalidOptions))
                           .isError(AsyncResultCategory, AsyncError::EmptyTransfer));
        invalidOptions                = options;
        invalidOptions.threadPoolMode = AsyncThreadPoolMode::ForceThreadPool;
        invalidOptions.threadPool     = nullptr;
        SC_TEST_EXPECT(consumeBounded(context, context.fileSend(file, client, invalidOptions))
                           .isError(AsyncResultCategory, AsyncError::MissingThreadPool));
        FileDescriptor   invalidFile;
        SocketDescriptor invalidSocket;
        SC_TEST_EXPECT(consumeBounded(context, context.fileSend(invalidFile, client, options))
                           .isError(AsyncResultCategory, AsyncError::InvalidFileHandle));
        SC_TEST_EXPECT(consumeBounded(context, context.fileSend(file, invalidSocket, options))
                           .isError(AsyncResultCategory, AsyncError::InvalidSocketHandle));
        ExecutionStopSource stop;
        stop.request_stop();
        progress = {99, true, true};
        SC_TEST_EXPECT(
            context.runIntoWithStop(context.fileSend(file, client, options, &progress), stop.get_token(), sent)
                .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(progress.bytesTransferred == 0 and not progress.complete and not progress.usedZeroCopy);
        options.offset   = static_cast<int64_t>(source.size() - 2);
        options.length   = 4;
        const Result eof = consumeBounded(context, context.fileSend(file, client, options, &progress), sent);
        SC_TEST_EXPECT(not eof and not eof.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(progress.bytesTransferred <= 2 and not progress.complete);
#if SC_PLATFORM_WINDOWS
        if (forcePool)
#endif
        {
            SC_TEST_EXPECT(eof.isError(AsyncResultCategory, AsyncError::FileSendIncomplete));
            SC_TEST_EXPECT(progress.bytesTransferred == 2);
        }
        SC_TEST_EXPECT(pool.waitForAllTasks());
        SC_TEST_EXPECT(file.close());
        SC_TEST_EXPECT(accepted.socket.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(listener.close());
        SC_TEST_EXPECT(fs.removeFile(path.view()));
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
        SC_TEST_EXPECT(pool.destroy());
    }

    void fileNativeProgress()
    {
        AsyncEventLoop loop;
        ThreadPool     pool;
        SC_TEST_EXPECT(loop.create());
        SC_TEST_EXPECT(pool.create(1));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SocketDescriptor         listener, client;
        AsyncExecutionConnection accepted;
        SC_TEST_EXPECT(tcpPair(context, listener, client, accepted, report.mapPort(5118)));
        StringPath path;
        SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory.view(), "AsyncExecutionNative.txt"}));
        FileDescriptor file;
        FileOpen       openMode;
        openMode.mode     = FileOpen::WriteRead;
        openMode.blocking = false;
        SC_TEST_EXPECT(file.open(path.view(), openMode));
        SC_TEST_EXPECT(loop.associateExternallyCreatedFileDescriptor(file));
        struct Gate
        {
            EventObject  release;
            Atomic<bool> entered = false;
            Atomic<bool> done    = false;
            Atomic<bool> forced  = false;
        } gate;
        ThreadPoolTask blocker;
        blocker.function = [&gate]()
        {
            gate.entered.store(true);
            gate.release.wait();
        };
        SC_TEST_EXPECT(pool.queueTask(blocker));
        Thread failsafe;
        SC_TEST_EXPECT(failsafe.start(
            [&gate](Thread&)
            {
                for (int attempt = 0; attempt < 3000 and not gate.done.load(); ++attempt)
                    Thread::Sleep(1);
                if (not gate.done.load())
                {
                    gate.forced.store(true);
                    gate.release.signal();
                }
            }));
        for (int attempt = 0; attempt < 1000 and not gate.entered.load(); ++attempt)
            Thread::Sleep(1);
        size_t       written = 0;
        const Result wrote =
            consumeBounded(context, context.fileWriteAllAt(file, 0, StringView("native").toCharSpan()), written);
        char                          local[6] = {}, remote[6] = {};
        AsyncExecutionReadResult      read, received;
        AsyncExecutionFileSendResult  sent;
        AsyncExecutionFileSendOptions sendOptions;
        sendOptions.length     = 6;
        sendOptions.threadPool = &pool; // A native-capable file send must not join the unrelated worker.
        bool   timer           = false;
        Result ran{false};
        if (wrote)
            ran = consumeBounded(context,
                                 ex::when_all(context.fileReadExactAt(file, 0, local),
                                              context.fileSend(file, client, sendOptions),
                                              context.receiveExact(accepted.socket, remote),
                                              context.sleep({1}) | ex::then([&timer]() noexcept { timer = true; })),
                                 read, sent, received);
        gate.done.store(true);
        gate.release.signal();
        SC_TEST_EXPECT(failsafe.join());
        SC_TEST_EXPECT(pool.waitForAllTasks());
        SC_TEST_EXPECT(gate.entered.load() and not gate.forced.load());
        SC_TEST_EXPECT(wrote and ran and written == 6 and timer and sent.complete and sent.bytesTransferred == 6);
        SC_TEST_EXPECT(read.data.sizeInBytes() == 6 and received.data.sizeInBytes() == 6);
        SC_TEST_EXPECT(memcmp(local, "native", 6) == 0 and memcmp(remote, "native", 6) == 0);

        // A blocking, unassociated descriptor separately exercises explicit worker routing.
        SC_TEST_EXPECT(file.close());
        openMode.mode     = FileOpen::ReadWrite;
        openMode.blocking = true;
        SC_TEST_EXPECT(file.open(path.view(), openMode));
        AsyncExecutionFileReadOptions forced{&pool};
        forced.threadPoolMode = AsyncThreadPoolMode::ForceThreadPool;
        SC_TEST_EXPECT(consumeBounded(
            context, context.fileWriteAllAt(file, 0, StringView("worker").toCharSpan(), forced), written));
        SC_TEST_EXPECT(consumeBounded(context, context.fileReadExactAt(file, 0, local, forced), read));
        SC_TEST_EXPECT(written == 6 and read.data.sizeInBytes() == 6 and memcmp(local, "worker", 6) == 0);

        // Stop pending worker I/O, reuse its buffer at terminal notification, then release the syscall.
        PipeDescriptor pipe;
        SC_TEST_EXPECT(pipe.createPipe());
        char                loan[1] = {};
        ExecutionStopSource stop;
        ExecutionOutcome    outcome;
        outcome.loop      = &loop;
        outcome.onStopped = [&loan]() { loan[0] = 'z'; };
        auto operation    = ex::connect(context.fileReadExact(pipe.readPipe, loan, forced),
                                        ExecutionReceiver{outcome, stop.get_token()});
        struct Release
        {
            ExecutionStopSource& stop;
            FileDescriptor&      writer;
            Atomic<bool>         armed   = false;
            Atomic<bool>         expired = false;
            Result               wrote{false};
        } release{stop, pipe.writePipe};
        Thread writer;
        SC_TEST_EXPECT(writer.start(
            [&release](Thread&)
            {
                for (int attempt = 0; attempt < 1000 and not release.armed.load(); ++attempt)
                    Thread::Sleep(1);
                release.expired.store(not release.armed.load());
                release.stop.request_stop();
                Thread::Sleep(20);
                release.wrote = release.writer.write(StringView("x").toCharSpan());
                if (not release.wrote)
                    ::_Exit(124);
            }));
        ex::start(operation);
        const Result first  = loop.runNoWait();
        const Result second = loop.runNoWait();
        const bool   worker = operation.request.usesThreadPool() and not operation.request.isFree();
        release.armed.store(true);
        const Result cancelled = runBounded(loop);
        SC_TEST_EXPECT(writer.join());
        SC_TEST_EXPECT(pool.waitForAllTasks());
        SC_TEST_EXPECT(first and second and cancelled and worker and not release.expired.load() and release.wrote);
        SC_TEST_EXPECT(outcome.stopped == 1 and outcome.values == 0 and outcome.errors == 0 and
                       operation.request.isFree());
        SC_TEST_EXPECT(loan[0] == 'z'); // No worker may overwrite storage after set_stopped.
        SC_TEST_EXPECT(pipe.close());
        SC_TEST_EXPECT(file.close());
        SC_TEST_EXPECT(FileSystem::Operations::removeFile(path.view()));
        SC_TEST_EXPECT(accepted.socket.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(listener.close());
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
        SC_TEST_EXPECT(pool.destroy());
    }

    void filePipes()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        Atomic<bool> finished = false;
        Thread       deadline;
        SC_TEST_EXPECT(deadline.start(
            [&finished](Thread&)
            {
                for (int attempt = 0; attempt < 800 and not finished.load(); ++attempt)
                    Thread::Sleep(10);
                // Independent of the event loop, which a backend regression could starve.
                if (not finished.load())
                    ::_Exit(124);
            }));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        PipeDescriptor pipe;
        PipeOptions    options;
        options.blocking = false;
        SC_TEST_EXPECT(pipe.createPipe(options));
        SC_TEST_EXPECT(loop.associateExternallyCreatedFileDescriptor(pipe.readPipe));
        SC_TEST_EXPECT(loop.associateExternallyCreatedFileDescriptor(pipe.writePipe));
        Span<const char>         parts[]   = {{}, StringView("pi").toCharSpan(), {}, StringView("pe").toCharSpan(), {}};
        char                     buffer[4] = {};
        size_t                   written   = 0;
        AsyncExecutionReadResult read;
        SC_TEST_EXPECT(consumeBounded(
            context,
            ex::when_all(context.fileWrite(pipe.writePipe, parts), context.fileReadExact(pipe.readPipe, buffer)),
            written, read));
        SC_TEST_EXPECT(written == 4 and read.data.sizeInBytes() == 4 and memcmp(buffer, "pipe", 4) == 0);
        ExecutionOutcome outcome;
        outcome.loop = &loop;
        ExecutionStopSource stop;
        auto                operation =
            ex::connect(context.fileReadExact(pipe.readPipe, buffer), ExecutionReceiver{outcome, stop.get_token()});
        ex::start(operation);
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(not operation.request.usesThreadPool() and not operation.request.isFree());
        Thread canceller;
        SC_TEST_EXPECT(canceller.start([&](Thread&) { stop.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(outcome.stopped == 1 and outcome.values == 0 and operation.request.isFree());
        SC_TEST_EXPECT(pipe.close());
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
        finished.store(true);
        SC_TEST_EXPECT(deadline.join());
    }

    void processWatches()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        Process success, failure;
#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(success.launch({"cmd", "/C", "exit", "/B", "0"}));
        SC_TEST_EXPECT(failure.launch({"cmd", "/C", "exit", "/B", "7"}));
#else
        SC_TEST_EXPECT(success.launch({"sh", "-c", "sleep 0.2; exit 0"}));
        SC_TEST_EXPECT(failure.launch({"sh", "-c", "sleep 0.1; exit 7"}));
#endif
        AsyncExecutionProcessExitResult successful, failed, observedSuccess, observedFailure;
        SC_TEST_EXPECT(consumeBounded(context,
                                      ex::when_all(context.processExit(success.handle, &observedSuccess),
                                                   context.processExit(failure.handle, &observedFailure)),
                                      successful, failed));
        SC_TEST_EXPECT(successful.exitStatus == 0 and failed.exitStatus == 7 and successful.complete and
                       failed.complete);
        SC_TEST_EXPECT(observedSuccess.complete and observedSuccess.exitStatus == 0);
        SC_TEST_EXPECT(observedFailure.complete and observedFailure.exitStatus == 7);
        // Successful POSIX waits have reaped these children. Do not wait for them a second time.
        AsyncExecutionProcessExitResult untouched{123};
        const Result invalid = context.runInto(context.processExit(FileDescriptor::Invalid), untouched);
        SC_TEST_EXPECT(invalid.isError(AsyncResultCategory, AsyncError::InvalidHandle));
        SC_TEST_EXPECT(untouched.exitStatus == 123);
        Process child;
#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(child.launch({"cmd", "/C", "ping", "-n", "3", "127.0.0.1", ">NUL"}));
#else
        SC_TEST_EXPECT(child.launch({"sleep", "1"}));
#endif
        ExecutionStopSource prestop;
        prestop.request_stop();
        const Result stopped =
            context.runIntoWithStop(context.processExit(child.handle), prestop.get_token(), untouched);
        SC_TEST_EXPECT(stopped.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(untouched.exitStatus == 123);
        AsyncExecutionProcessExitResult cancelledProgress;
        ExecutionStopSource             stop;
        ExecutionOutcome                outcome;
        outcome.loop   = &loop;
        auto operation = ex::connect(context.processExit(child.handle, &cancelledProgress),
                                     ExecutionReceiver{outcome, stop.get_token()});
        ex::start(operation);
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(operation.request.isActive());
        Thread canceller;
        SC_TEST_EXPECT(canceller.start([&stop](Thread&) { stop.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(operation.request.isFree() and outcome.stopped == 1 and outcome.values == 0 and
                       outcome.errors == 0);
        SC_TEST_EXPECT(not cancelledProgress.complete);
        // Stopping this pending wait did not reap the child: the caller can still wait normally.
        SC_TEST_EXPECT(child.waitForExitSync());
        SC_TEST_EXPECT(child.getExitStatus() == 0);
        Process racing;
#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(racing.launch({"cmd", "/C", "exit", "/B", "9"}));
#else
        SC_TEST_EXPECT(racing.launch({"sh", "-c", "sleep 0.1; exit 9"}));
#endif
        ExecutionStopSource             raceStop;
        AsyncExecutionProcessExitResult raceProgress;
        struct ObserveThenStop : detail::AsyncExecutionProcessExit
        {
            ExecutionStopSource& stop;
            void                 observe(Request::Result& result)
            {
                detail::AsyncExecutionProcessExit::observe(result);
                stop.request_stop();
            }
        };
        // Force stop after native completion/reaping, before the generic sender selects its terminal channel.
        auto raced = AsyncExecutionSender<ObserveThenStop>{context, {{racing.handle, &raceProgress}, raceStop}};
        AsyncLoopTimeout raceDeadline;
        raceDeadline.callback = [&raceStop](AsyncLoopTimeout::Result&) { raceStop.request_stop(); };
        SC_TEST_EXPECT(raceDeadline.start(loop, {2000}));
        const Result race = context.runIntoWithStop(move(raced), raceStop.get_token(), untouched);
        if (not raceDeadline.isFree())
            SC_TEST_EXPECT(raceDeadline.unschedule(loop));
        SC_TEST_EXPECT(race.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(raceProgress.complete and raceProgress.exitStatus == 9);
        SC_TEST_EXPECT(untouched.exitStatus == 123);
        // The native completion is known even though the graph stopped; do not wait for this child again.
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void signalWatches()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        AsyncExecutionSignalResult delivered{91, 17};
        SC_TEST_EXPECT(
            context.runInto(context.signal(0), delivered).isError(AsyncResultCategory, AsyncError::InvalidSignal));
        SC_TEST_EXPECT(delivered.signalNumber == 91 and delivered.deliveryCount == 17);
#if SC_PLATFORM_WINDOWS
        constexpr int watchedSignal     = 21;
        constexpr int unsupportedSignal = 999;
#else
        constexpr int watchedSignal     = SIGUSR1;
        constexpr int unsupportedSignal = SIGKILL;
        bool          raised            = false;
        auto          trigger =
            context.sleep({1}) | ex::then([&raised]() noexcept { raised = ::kill(::getpid(), SIGUSR1) == 0; });
        SC_TEST_EXPECT(consumeBounded(context, ex::when_all(context.signal(watchedSignal), move(trigger)), delivered));
        SC_TEST_EXPECT(raised and delivered.signalNumber == watchedSignal and delivered.deliveryCount >= 1);
#endif
        SC_TEST_EXPECT(
            context.run(context.signal(unsupportedSignal)).isError(AsyncResultCategory, AsyncError::UnsupportedSignal));
        ExecutionStopSource prestop;
        prestop.request_stop();
        delivered = {91, 17};
        SC_TEST_EXPECT(context.runIntoWithStop(context.signal(watchedSignal), prestop.get_token(), delivered)
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(delivered.signalNumber == 91 and delivered.deliveryCount == 17);
        ExecutionStopSource stop;
        ExecutionOutcome    outcome;
        outcome.loop = &loop;
        AsyncSignalOptions options;
        options.mode = AsyncSignalOptions::Mode::OneShot;
        auto operation =
            ex::connect(context.signal(watchedSignal, options), ExecutionReceiver{outcome, stop.get_token()});
        ex::start(operation);
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(operation.request.isActive());
        SC_TEST_EXPECT(stop.request_stop());
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(operation.request.isFree() and outcome.stopped == 1 and outcome.values == 0 and
                       outcome.errors == 0);
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
#if SC_PLATFORM_WINDOWS
        windowsSignalDelivery();
#endif
    }

    void windowsSignalDelivery()
    {
#if SC_PLATFORM_WINDOWS
        const bool allocatedConsole = ::GetConsoleCP() == 0 and ::AllocConsole() != FALSE;
        auto       closeConsole     = MakeDeferred(
            [allocatedConsole]
            {
                if (allocatedConsole)
                    ::FreeConsole();
            });
        if (::GetConsoleCP() == 0)
        {
            report.console.printLine("AsyncExecutionTest - real console unavailable; signal delivery not tested");
            return;
        }
        char eventName[96];
        ::snprintf(eventName, sizeof(eventName), "Local\\SCAsyncExecutionSignalReady-%u", report.mapPort(5164));
        HANDLE ready = ::CreateEventA(nullptr, TRUE, FALSE, eventName);
        SC_TEST_EXPECT(ready != nullptr);
        if (ready == nullptr)
            return;
        auto            closeReady = MakeDeferred([ready] { ::CloseHandle(ready); });
        SmallString<32> offset;
        SC_TEST_EXPECT(StringBuilder::format(offset, "{}", report.mapPort(0)));
        StringSpan arguments[] = {
            report.executableFile.view(),  "--quiet",       "--test",     "AsyncExecutionTest", "--test-section",
            "windows sender signal child", "--port-offset", offset.view()};
        Process child;
        child.options.windowsHide                  = false;
        child.options.windowsCreateNewProcessGroup = true;
        const Result launched =
            child.launch(arguments, Process::StdOut::Ignore(), Process::StdIn(), Process::StdErr::Ignore());
        SC_TEST_EXPECT(launched);
        if (not launched)
            return;
        const bool published = ::WaitForSingleObject(ready, 5000) == WAIT_OBJECT_0;
        SC_TEST_EXPECT(published);
        if (published)
            SC_TEST_EXPECT(::GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, static_cast<DWORD>(child.processID.pid)) !=
                           FALSE);
        const bool exited = ::WaitForSingleObject(child.handle, 5000) == WAIT_OBJECT_0;
        if (not exited)
            SC_TEST_EXPECT(::TerminateProcess(child.handle, 124) != FALSE);
        SC_TEST_EXPECT(exited);
        SC_TEST_EXPECT(child.waitForExitSync());
        SC_TEST_EXPECT(child.getExitStatus() == 0);
#endif
    }

    void windowsSignalChild()
    {
#if SC_PLATFORM_WINDOWS
        char eventName[96];
        ::snprintf(eventName, sizeof(eventName), "Local\\SCAsyncExecutionSignalReady-%u", report.mapPort(5164));
        HANDLE ready = ::OpenEventA(EVENT_MODIFY_STATE, FALSE, eventName);
        SC_TEST_EXPECT(ready != nullptr);
        if (ready == nullptr)
            return;
        auto           closeReady = MakeDeferred([ready] { ::CloseHandle(ready); });
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        bool                       published = false;
        AsyncExecutionSignalResult delivered;
        auto                       announce =
            context.sleep({10}) | ex::then([ready, &published]() noexcept { published = ::SetEvent(ready) != FALSE; });
        SC_TEST_EXPECT(consumeBounded(context, ex::when_all(context.signal(21), move(announce)), delivered));
        SC_TEST_EXPECT(published and delivered.signalNumber == 21 and delivered.deliveryCount >= 1);
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
#endif
    }

    void datagrams(bool ipv6)
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SocketIPAddress target, source;
        StringView      host = ipv6 ? StringView("::1") : StringView("127.0.0.1");
        const uint16_t  port = report.mapPort(ipv6 ? 5152 : 5150);
        SC_TEST_EXPECT(target.fromAddressPort(host, port));
        SC_TEST_EXPECT(source.fromAddressPort(host, port + 1));
        SocketDescriptor server, client;
        SC_TEST_EXPECT(loop.createAsyncUDPSocket(target.getAddressFamily(), server));
        SC_TEST_EXPECT(loop.createAsyncUDPSocket(source.getAddressFamily(), client));
        SC_TEST_EXPECT(SocketServer(server).bind(target));
        SC_TEST_EXPECT(SocketServer(client).bind(source));
        char                                  storage[8] = {};
        size_t                                sent       = 0;
        AsyncExecutionSocketReceiveFromResult received;
        SocketAddress                         copiedAddress(target);
        auto lazy     = context.sendTo(client, copiedAddress, StringView("abc").toCharSpan());
        copiedAddress = {};
        static_assert(ex::sender<decltype(lazy)>);
        SC_TEST_EXPECT(
            consumeBounded(context, ex::when_all(move(lazy), context.receiveFrom(server, storage)), sent, received));
        SC_TEST_EXPECT(sent == 3 and received.data.sizeInBytes() == 3 and not received.truncated);
        SC_TEST_EXPECT(memcmp(received.data.data(), "abc", 3) == 0);
        SC_TEST_EXPECT(received.sourceAddress.getPort() == source.getPort());
        SocketIPAddress observedSource;
        SC_TEST_EXPECT(received.sourceSocketAddress.getIPAddress(observedSource));
        SC_TEST_EXPECT(observedSource.getPort() == source.getPort());
        // Vector elements form one datagram. No sendAll-style retry can split it into messages.
        Span<const char> parts[] = {StringView("de").toCharSpan(), {}, StringView("fg").toCharSpan()};
        SC_TEST_EXPECT(consumeBounded(
            context, ex::when_all(context.sendTo(client, target, parts), context.receiveFrom(server, {storage, 3})),
            sent, received));
        SC_TEST_EXPECT(sent == 4 and received.truncated and received.data.sizeInBytes() == 3);
        SC_TEST_EXPECT(memcmp(storage, "def", 3) == 0);
        SC_TEST_EXPECT(parts[0].sizeInBytes() == 2 and parts[1].empty() and parts[2].sizeInBytes() == 2);
        SC_TEST_EXPECT(consumeBounded(context,
                                      ex::when_all(context.sendTo(client, target, StringView("next").toCharSpan()),
                                                   context.receiveFrom(server, storage)),
                                      sent, received));
        SC_TEST_EXPECT(sent == 4 and not received.truncated and received.data.sizeInBytes() == 4);
        SC_TEST_EXPECT(memcmp(storage, "next", 4) == 0);
        SC_TEST_EXPECT(consumeBounded(
            context,
            ex::when_all(context.sendTo(client, target, Span<const char>{}), context.receiveFrom(server, storage)),
            sent, received));
        SC_TEST_EXPECT(sent == 0 and received.data.empty() and not received.truncated);
        SC_TEST_EXPECT(
            consumeBounded(context,
                           ex::when_all(context.sendTo(client, SocketAddress(target), StringView("x").toCharSpan()),
                                        context.receiveFrom(server, {storage, 0})),
                           sent, received));
        SC_TEST_EXPECT(sent == 1 and received.data.empty() and received.truncated);

        SC_TEST_EXPECT(consumeBounded(context,
                                      ex::when_all(context.sendTo(client, target, Span<Span<const char>>{}),
                                                   context.receiveFrom(server, storage)),
                                      sent, received));
        SC_TEST_EXPECT(sent == 0 and received.data.empty() and not received.truncated);

        // An active receive joins cancellation before its receiver releases operation/buffer storage.
        ExecutionStopSource stop;
        ExecutionOutcome    outcome;
        outcome.loop    = &loop;
        auto sender     = context.receiveFrom(server, storage);
        using Operation = decltype(ex::connect(move(sender), ExecutionReceiver{outcome, stop.get_token()}));
        alignas(Operation) char operationStorage[sizeof(Operation)];
        auto*                   operation = reinterpret_cast<Operation*>(operationStorage);
        placementNew(*operation, context, move(sender.setup), ExecutionReceiver{outcome, stop.get_token()});
        struct Terminal
        {
            Operation* operation;
            char*      buffer;
            bool       destroyed = false;
        } terminal{operation, storage};
        outcome.onStopped = [&terminal]
        {
            const bool free = terminal.operation->request.isFree();
            terminal.operation->~Operation();
            terminal.buffer[0] = 'z';
            terminal.destroyed = free;
        };
        ex::start(*operation);
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(operation->request.isActive());
        Thread canceller;
        SC_TEST_EXPECT(canceller.start([&stop](Thread&) { stop.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(terminal.destroyed and outcome.stopped == 1 and outcome.values == 0 and outcome.errors == 0);
        SC_TEST_EXPECT(storage[0] == 'z');
        // Reuse the same socket after cancellation; no stale completion may consume this datagram.
        SC_TEST_EXPECT(consumeBounded(context,
                                      ex::when_all(context.sendTo(client, target, StringView("reuse").toCharSpan()),
                                                   context.receiveFrom(server, storage)),
                                      sent, received));
        SC_TEST_EXPECT(received.data.sizeInBytes() == 5 and not received.truncated);
        SC_TEST_EXPECT(memcmp(storage, "reuse", 5) == 0);
        const Result stopped =
            context.run(context.sendTo(client, target, StringView("drop").toCharSpan()), stop.get_token());
        SC_TEST_EXPECT(stopped.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        const Result prestoppedRead =
            context.runIntoWithStop(context.receiveFrom(server, storage), stop.get_token(), received);
        SC_TEST_EXPECT(prestoppedRead.isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(received.data.sizeInBytes() == 5);
        SC_TEST_EXPECT(consumeBounded(context,
                                      ex::when_all(context.sendTo(client, target, StringView("after").toCharSpan()),
                                                   context.receiveFrom(server, storage)),
                                      sent, received));
        SC_TEST_EXPECT(received.data.sizeInBytes() == 5 and memcmp(storage, "after", 5) == 0);
        if constexpr (sizeof(size_t) > 4)
        {
            const size_t tooLarge  = size_t(uint64_t(1) << 32);
            const Result oversized = context.run(context.sendTo(client, target, Span<const char>{storage, tooLarge}));
            SC_TEST_EXPECT(oversized.isError(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow));
            const Result largeRead = context.run(context.receiveFrom(server, {storage, tooLarge}));
            SC_TEST_EXPECT(largeRead.isError(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow));
            Span<const char> overflow[]  = {{storage, tooLarge - 1}, {storage, 1}};
            const Result     largeVector = context.run(context.sendTo(client, target, overflow));
            SC_TEST_EXPECT(
                largeVector.isError(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow));
        }
        SocketDescriptor invalid;
        const Result     invalidRead = context.run(context.receiveFrom(invalid, storage));
        SC_TEST_EXPECT(invalidRead.isError(AsyncResultCategory, AsyncError::InvalidSocketHandle));
        const Result invalidSend = context.run(context.sendTo(invalid, target, StringView("x").toCharSpan()));
        SC_TEST_EXPECT(invalidSend.isError(AsyncResultCategory, AsyncError::InvalidSocketHandle));
        SC_TEST_EXPECT(server.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void borrowedTCP()
    {
        static_assert(std::is_nothrow_move_assignable_v<AsyncExecutionConnection>);
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SocketDescriptor         listener, client;
        AsyncExecutionConnection accepted;
        const uint16_t           port = report.mapPort(5100);
        SC_TEST_EXPECT(tcpPair(context, listener, client, accepted, port, true));
        SC_TEST_EXPECT(accepted.socket.isValid() and client.isValid());
        Span<const char> parts[] = {{},
                                    StringView("hel").toCharSpan(),
                                    {},
                                    StringView("lo\n").toCharSpan(),
                                    StringView("next\r\n").toCharSpan(),
                                    {}};

        char   storage[16] = {};
        size_t sent = 0, progress = 99;

        AsyncExecutionLineResult line;
        auto                     lazySend = context.sendAll(client, parts, &progress);
        SC_TEST_EXPECT(progress == 99);
        SC_TEST_EXPECT(consumeBounded(
            context, ex::when_all(move(lazySend), context.receiveLine(accepted.socket, storage)), sent, line));
        SC_TEST_EXPECT(sent == 12 and progress == 12);
        SC_TEST_EXPECT(line.lineComplete and not line.disconnected and line.line.sizeInBytes() == 5);
        SC_TEST_EXPECT(memcmp(line.line.data(), "hello", 5) == 0);
        SC_TEST_EXPECT(parts[0].empty() and parts[1].sizeInBytes() == 3 and parts[3].sizeInBytes() == 3 and
                       parts[4].sizeInBytes() == 6 and parts[5].empty());
        SC_TEST_EXPECT(consumeBounded(context, context.receiveLine(accepted.socket, storage), line));
        SC_TEST_EXPECT(line.lineComplete and line.line.sizeInBytes() == 4 and memcmp(line.line.data(), "next", 4) == 0);
        Span<const char> oneShot[] = {{}, StringView("!").toCharSpan(), {}, StringView("?").toCharSpan(), {}};

        char exactStorage[2];

        AsyncExecutionReadResult exact;
        SC_TEST_EXPECT(consumeBounded(
            context, ex::when_all(context.send(client, oneShot), context.receiveExact(accepted.socket, exactStorage)),
            sent, exact));
        SC_TEST_EXPECT(sent == 2 and exact.data.sizeInBytes() == 2 and not exact.endOfFile);
        SC_TEST_EXPECT(memcmp(exact.data.data(), "!?", 2) == 0 and oneShot[1].sizeInBytes() == 1 and
                       oneShot[3].sizeInBytes() == 1);
        size_t contiguousProgress = 99;
        SC_TEST_EXPECT(
            consumeBounded(context,
                           ex::when_all(context.sendAll(client, StringView("ab").toCharSpan(), &contiguousProgress),
                                        context.receiveExact(accepted.socket, exactStorage)),
                           sent, exact));
        SC_TEST_EXPECT(sent == 2 and contiguousProgress == 2 and memcmp(exactStorage, "ab", 2) == 0);
        ExecutionTestBytes largeSource, largeReply;
        SC_TEST_EXPECT(largeSource.resize(35000));
        SC_TEST_EXPECT(largeReply.resize(35000));
        for (size_t index = 0; index < largeSource.size(); ++index)
            largeSource.bytes()[index] = static_cast<char>('a' + index % 23);
        const auto       sourceBytes  = largeSource.bytes();
        Span<const char> largeParts[] = {{sourceBytes.data(), 18000}, {}, {sourceBytes.data() + 18000, 17000}};
        SC_TEST_EXPECT(consumeBounded(context,
                                      ex::when_all(context.sendAll(client, largeParts, &progress),
                                                   context.receiveExact(accepted.socket, largeReply.bytes())),
                                      sent, exact));
        SC_TEST_EXPECT(sent == 35000 and progress == 35000 and exact.data.sizeInBytes() == 35000);
        SC_TEST_EXPECT(memcmp(largeSource.bytes().data(), largeReply.bytes().data(), 35000) == 0);
        SC_TEST_EXPECT(largeParts[0].sizeInBytes() == 18000 and largeParts[2].sizeInBytes() == 17000);

        SocketDescriptor       invalid;
        Span<Span<const char>> emptyParts;
        SC_TEST_EXPECT(consumeBounded(context, context.sendAll(invalid, emptyParts), sent));
        SC_TEST_EXPECT(sent == 0);
        SC_TEST_EXPECT(consumeBounded(context, context.send(client, emptyParts))
                           .isError(AsyncResultCategory, AsyncError::EmptyBuffer));
        SC_TEST_EXPECT(consumeBounded(context, context.receiveExact(invalid, Span<char>{}), exact));
        SC_TEST_EXPECT(exact.data.empty() and not exact.endOfFile);
        ExecutionStopSource prestopped;
        prestopped.request_stop();
        progress = 99;
        SC_TEST_EXPECT(
            context.run(context.sendAll(invalid, StringView("unused").toCharSpan(), &progress), prestopped.get_token())
                .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(progress == 0);
        AsyncExecutionReadResult stoppedProgress{storage, true};
        SC_TEST_EXPECT(context.run(context.receiveExact(invalid, storage, &stoppedProgress), prestopped.get_token())
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(stoppedProgress.data.empty() and not stoppedProgress.endOfFile);

        SC_TEST_EXPECT(consumeBounded(context, context.receiveLine(client, Span<char>{}))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::EmptyReceiveBuffer));
        SocketIPAddress address;
        SC_TEST_EXPECT(address.fromAddressPort("127.0.0.1", port));
        SC_TEST_EXPECT(consumeBounded(context, context.connect(invalid, address))
                           .isError(AsyncResultCategory, AsyncError::InvalidSocketHandle));
        SC_TEST_EXPECT(consumeBounded(context, context.accept(invalid))
                           .isError(AsyncResultCategory, AsyncError::InvalidSocketHandle));
        // Replacing an owned connection retires its previous descriptor before consumption returns.
        SocketDescriptor second;
        SC_TEST_EXPECT(loop.createAsyncTCPSocket(address.getAddressFamily(), second));
        SC_TEST_EXPECT(consumeBounded(context, ex::when_all(context.accept(listener), context.connect(second, address)),
                                      accepted));
        SC_TEST_EXPECT(consumeBounded(context, context.receive(client, storage), exact));
        SC_TEST_EXPECT(exact.endOfFile and exact.data.empty());
        SC_TEST_EXPECT(second.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(accepted.socket.close());
        SC_TEST_EXPECT(listener.close());
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void tcpReceiveBoundaries()
    {
        for (int test = 0; test < 4; ++test)
        {
            AsyncEventLoop loop;
            SC_TEST_EXPECT(loop.create());
            AsyncExecutionContext context{loop};
            SC_TEST_EXPECT(context.create());
            SocketDescriptor         listener, client;
            AsyncExecutionConnection accepted;
            SC_TEST_EXPECT(tcpPair(context, listener, client, accepted, report.mapPort(uint16_t(5101 + test))));
            char                     storage[4] = {};
            AsyncExecutionReadResult exact, exactProgress;
            AsyncExecutionLineResult line, lineProgress;
            if (test == 0)
            {
                auto send = context.sendAll(client, StringView("xy").toCharSpan()) |
                            SC::tryThen([&](size_t) noexcept -> Result { return client.close(); });
                const Result status = consumeBounded(
                    context, ex::when_all(move(send), context.receiveExact(accepted.socket, storage, &exactProgress)),
                    exact);
                SC_TEST_EXPECT(
                    status.isError(AsyncExecutionResultCategory, AsyncExecutionError::SocketReceiveIncomplete));
                SC_TEST_EXPECT(exactProgress.data.sizeInBytes() == 2 and exactProgress.endOfFile);
                SC_TEST_EXPECT(memcmp(storage, "xy", 2) == 0);
                SC_TEST_EXPECT(consumeBounded(context, context.receive(accepted.socket, storage), exact));
                SC_TEST_EXPECT(exact.endOfFile and exact.data.empty());
            }
            else if (test == 1)
            {
                SC_TEST_EXPECT(consumeBounded(
                    context, ex::when_all(context.sendAll(client, StringView("abcd\n").toCharSpan()),
                                          context.receiveLine(accepted.socket, storage, &lineProgress))));
                SC_TEST_EXPECT(lineProgress.lineComplete and lineProgress.line.sizeInBytes() == 4);
                SC_TEST_EXPECT(memcmp(storage, "abcd", 4) == 0);
            }
            else if (test == 2)
            {
                const Result status =
                    consumeBounded(context, ex::when_all(context.sendAll(client, StringView("abcdX\n").toCharSpan()),
                                                         context.receiveLine(accepted.socket, storage, &lineProgress)));
                SC_TEST_EXPECT(
                    status.isError(AsyncExecutionResultCategory, AsyncExecutionError::ReceiveLineBufferExhausted));
                SC_TEST_EXPECT(not lineProgress.lineComplete and not lineProgress.disconnected and
                               lineProgress.line.sizeInBytes() == 4);
                SC_TEST_EXPECT(memcmp(storage, "abcd", 4) == 0);
                SC_TEST_EXPECT(consumeBounded(context, context.receiveLine(accepted.socket, storage), line));
                SC_TEST_EXPECT(line.lineComplete and line.line.empty()); // Only the overflowing byte was consumed.
            }
            else
            {
                auto send = context.sendAll(client, StringView("tail").toCharSpan()) |
                            SC::tryThen([&](size_t) noexcept -> Result { return client.close(); });
                SC_TEST_EXPECT(consumeBounded(
                    context, ex::when_all(move(send), context.receiveLine(accepted.socket, storage, &lineProgress)),
                    line));
                SC_TEST_EXPECT(line.disconnected and not line.lineComplete and line.line.sizeInBytes() == 4);
                SC_TEST_EXPECT(lineProgress.disconnected and memcmp(storage, "tail", 4) == 0);
            }
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(accepted.socket.close());
            SC_TEST_EXPECT(listener.close());
            SC_TEST_EXPECT(context.close());
            SC_TEST_EXPECT(loop.close());
        }
    }

    void tcpCancellation()
    {
        // Pending accept: stop from another thread, then reuse the same listener for a connection.
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SocketNetworking::initNetworking();
        SocketIPAddress address;
        SC_TEST_EXPECT(address.fromAddressPort("127.0.0.1", report.mapPort(5106)));
        SocketDescriptor listener, client;
        SC_TEST_EXPECT(loop.createAsyncTCPSocket(address.getAddressFamily(), listener));
        SC_TEST_EXPECT(SocketServer(listener).bind(address));
        SC_TEST_EXPECT(SocketServer(listener).listen(2));
        ExecutionStopSource acceptStop;
        ExecutionOutcome    acceptOutcome;
        acceptOutcome.loop = &loop;
        auto acceptSender  = context.accept(listener);
        using AcceptOperation =
            decltype(ex::connect(move(acceptSender), ExecutionReceiver{acceptOutcome, acceptStop.get_token()}));
        alignas(AcceptOperation) char acceptStorage[sizeof(AcceptOperation)];
        auto*                         accept = reinterpret_cast<AcceptOperation*>(acceptStorage);
        placementNew(*accept, context, move(acceptSender.setup),
                     ExecutionReceiver{acceptOutcome, acceptStop.get_token()});
        bool acceptDestroyed    = false;
        acceptOutcome.onStopped = [&]
        {
            const bool released = accept->request.isFree();
            accept->~AcceptOperation();
            acceptDestroyed = released;
        };
        ex::start(*accept);
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(accept->request.isActive());
        Thread canceller;
        SC_TEST_EXPECT(canceller.start([&](Thread&) { acceptStop.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        const Result acceptCancelled = runBounded(loop);
        SC_TEST_EXPECT(acceptCancelled);
        SC_TEST_EXPECT(acceptDestroyed and acceptOutcome.stopped == 1 and acceptOutcome.values == 0);
        AsyncExecutionConnection accepted;
        SC_TEST_EXPECT(loop.createAsyncTCPSocket(address.getAddressFamily(), client));
        SC_TEST_EXPECT(consumeBounded(context, ex::when_all(context.accept(listener), context.connect(client, address)),
                                      accepted));
        SC_TEST_EXPECT(consumeBounded(context, context.sendAll(client, StringView("xy").toCharSpan())));
        char                     buffer[4] = {};
        AsyncExecutionReadResult progress;
        ExecutionStopSource      readStop;
        ExecutionOutcome         readOutcome;
        readOutcome.loop = &loop;
        auto read        = ex::connect(context.receiveExact(accepted.socket, buffer, &progress),
                                       ExecutionReceiver{readOutcome, readStop.get_token()});
        ex::start(read);
        Result pumped{true};
        for (int i = 0; i < 2000 and progress.data.sizeInBytes() < 2; ++i)
        {
            pumped = loop.runNoWait();
            if (not pumped)
                break;
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(pumped and progress.data.sizeInBytes() == 2 and not progress.endOfFile);
        SC_TEST_EXPECT(canceller.start([&](Thread&) { readStop.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        const Result readCancelled = runBounded(loop);
        SC_TEST_EXPECT(readCancelled);
        SC_TEST_EXPECT(read.request.isFree() and readOutcome.stopped == 1 and readOutcome.values == 0);
        SC_TEST_EXPECT(progress.data.sizeInBytes() == 2 and memcmp(buffer, "xy", 2) == 0);
        char next[1];
        SC_TEST_EXPECT(consumeBounded(context, ex::when_all(context.sendAll(client, StringView("z").toCharSpan()),
                                                            context.receiveExact(accepted.socket, next))));
        SC_TEST_EXPECT(next[0] == 'z');
        SC_TEST_EXPECT(consumeBounded(context, context.sendAll(client, StringView("q").toCharSpan())));
        ExecutionStopSource lineStop;
        ExecutionOutcome    lineOutcome;
        lineOutcome.loop = &loop;
        AsyncExecutionLineResult lineProgress;
        auto                     lineSender = context.receiveLine(accepted.socket, buffer, &lineProgress);
        using LineOperation =
            decltype(ex::connect(move(lineSender), ExecutionReceiver{lineOutcome, lineStop.get_token()}));
        alignas(LineOperation) char lineStorage[sizeof(LineOperation)];
        auto*                       lineOperation = reinterpret_cast<LineOperation*>(lineStorage);
        placementNew(*lineOperation, context, move(lineSender.setup),
                     ExecutionReceiver{lineOutcome, lineStop.get_token()});
        bool lineDestroyed    = false;
        lineOutcome.onStopped = [&]
        {
            const bool released = lineOperation->request.isFree();
            lineOperation->~LineOperation();
            lineDestroyed = released;
        };
        ex::start(*lineOperation);
        for (int i = 0; i < 2000 and lineProgress.line.empty(); ++i)
        {
            pumped = loop.runNoWait();
            if (not pumped)
                break;
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(pumped and lineProgress.line.sizeInBytes() == 1 and buffer[0] == 'q');
        SC_TEST_EXPECT(canceller.start([&](Thread&) { lineStop.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        const Result lineCancelled = runBounded(loop);
        SC_TEST_EXPECT(lineCancelled);
        SC_TEST_EXPECT(lineDestroyed and lineOutcome.stopped == 1 and not lineProgress.lineComplete);
        AsyncExecutionLineResult nextLine;
        size_t                   sentLineBytes = 0;
        SC_TEST_EXPECT(consumeBounded(context,
                                      ex::when_all(context.sendAll(client, StringView("\n").toCharSpan()),
                                                   context.receiveLine(accepted.socket, buffer)),
                                      sentLineBytes, nextLine));
        SC_TEST_EXPECT(nextLine.lineComplete and nextLine.line.empty());
        // Cancel between bounded send chunks; the progress count covers observed completions, not unreported OS bytes.
        ExecutionTestBytes payload;
        SC_TEST_EXPECT(payload.resize(65536));
        memset(payload.bytes().data(), 'p', payload.size());
        Span<const char>    parts[] = {payload.bytes(), {}};
        size_t              sent    = 0;
        ExecutionStopSource writeStop;
        ExecutionOutcome    writeOutcome;
        writeOutcome.loop = &loop;
        auto write =
            ex::connect(context.sendAll(client, parts, &sent), ExecutionReceiver{writeOutcome, writeStop.get_token()});
        ex::start(write);
        for (int i = 0; i < 2000 and sent == 0; ++i)
        {
            pumped = loop.runNoWait();
            if (not pumped)
                break;
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(pumped and sent > 0 and sent < payload.size());
        SC_TEST_EXPECT(canceller.start([&](Thread&) { writeStop.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        const Result writeCancelled = runBounded(loop);
        SC_TEST_EXPECT(writeCancelled);
        SC_TEST_EXPECT(write.request.isFree() and writeOutcome.stopped == 1 and writeOutcome.values == 0);
        SC_TEST_EXPECT(client.isValid() and accepted.socket.isValid() and sent < payload.size());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(accepted.socket.close());
        SC_TEST_EXPECT(listener.close());
        // Race stop against an already-submitted connect without relying on an unreachable external host.
        SocketDescriptor cancelListener, cancelClient;
        SC_TEST_EXPECT(address.fromAddressPort("127.0.0.1", report.mapPort(5108)));
        SC_TEST_EXPECT(loop.createAsyncTCPSocket(address.getAddressFamily(), cancelListener));
        SC_TEST_EXPECT(loop.createAsyncTCPSocket(address.getAddressFamily(), cancelClient));
        SC_TEST_EXPECT(SocketServer(cancelListener).bind(address));
        SC_TEST_EXPECT(SocketServer(cancelListener).listen(1));
        ExecutionStopSource connectStop;
        ExecutionOutcome    connectOutcome;
        connectOutcome.loop = &loop;
        auto connecting     = ex::connect(context.connect(cancelClient, address),
                                          ExecutionReceiver{connectOutcome, connectStop.get_token()});
        ex::start(connecting);
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(not connecting.request.isFree());
        SC_TEST_EXPECT(canceller.start([&](Thread&) { connectStop.request_stop(); }));
        SC_TEST_EXPECT(canceller.join());
        const Result connectCancelled = runBounded(loop);
        SC_TEST_EXPECT(connectCancelled);
        SC_TEST_EXPECT(connecting.request.isFree() and connectOutcome.stopped == 1 and connectOutcome.values == 0);
        SC_TEST_EXPECT(cancelClient.isValid()); // Cancellation does not close the borrowed descriptor.
        SC_TEST_EXPECT(cancelClient.close());
        SC_TEST_EXPECT(cancelListener.close());
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());

        // A full accept/connect graph may be destroyed by its terminal receiver, closing the dropped accepted value.
        AsyncEventLoop lifetimeLoop;
        SC_TEST_EXPECT(lifetimeLoop.create());
        AsyncExecutionContext lifetime{lifetimeLoop};
        SC_TEST_EXPECT(lifetime.create());
        SocketDescriptor server, sender;
        SC_TEST_EXPECT(address.fromAddressPort("127.0.0.1", report.mapPort(5107)));
        SC_TEST_EXPECT(lifetimeLoop.createAsyncTCPSocket(address.getAddressFamily(), server));
        SC_TEST_EXPECT(lifetimeLoop.createAsyncTCPSocket(address.getAddressFamily(), sender));
        SC_TEST_EXPECT(SocketServer(server).bind(address));
        SC_TEST_EXPECT(SocketServer(server).listen(1));
        auto graph      = ex::when_all(lifetime.accept(server), lifetime.connect(sender, address));
        using Graph     = decltype(graph);
        using Operation = decltype(ex::connect(std::declval<Graph>(), std::declval<ExecutionReceiver>()));
        struct Holder
        {
            Operation operation;
            Holder(Graph inputGraph, ExecutionReceiver receiver)
                : operation(ex::connect(move(inputGraph), move(receiver)))
            {}
        };
        alignas(Holder) char holderStorage[sizeof(Holder)];
        auto*                holder = reinterpret_cast<Holder*>(holderStorage);
        ExecutionOutcome     outcome;
        outcome.loop = &lifetimeLoop;
        placementNew(*holder, move(graph), ExecutionReceiver{outcome, {}});
        bool destroyed  = false;
        outcome.onValue = [&]
        {
            holder->~Holder();
            destroyed = true;
        };
        ex::start(holder->operation);
        const Result driven = runBounded(lifetimeLoop);
        SC_ASYNC_ASSERT_RELEASE(driven);
        SC_TEST_EXPECT(driven and destroyed and outcome.values == 1 and outcome.errors == 0);
        AsyncExecutionReadResult closed;
        SC_TEST_EXPECT(consumeBounded(lifetime, lifetime.receive(sender, buffer), closed));
        SC_TEST_EXPECT(closed.endOfFile and closed.data.empty());
        SC_TEST_EXPECT(sender.close());
        SC_TEST_EXPECT(server.close());
        SC_TEST_EXPECT(lifetime.close());
        SC_TEST_EXPECT(lifetimeLoop.close());
    }

    void tcpCursors()
    {
        SocketDescriptor unused;
        Span<const char> parts[]  = {{}, StringView("abcd").toCharSpan(), {}, StringView("xy").toCharSpan(), {}};
        size_t           progress = 99;
        detail::AsyncExecutionSendAllBuffers cursor{unused, parts, &progress};
        cursor.initialize();
        SC_TEST_EXPECT(progress == 0 and cursor.index == 1 and cursor.offset == 0);
        SC_TEST_EXPECT(cursor.advanceBytes(0).isError(AsyncResultCategory, AsyncError::SocketSendIncomplete));
        SC_TEST_EXPECT(cursor.advanceBytes(5).isError(AsyncResultCategory, AsyncError::SocketSendIncomplete));
        SC_TEST_EXPECT(cursor.advanceBytes(1));
        SC_TEST_EXPECT(cursor.offset == 1 and progress == 1);
        SC_TEST_EXPECT(cursor.advanceBytes(2));
        SC_TEST_EXPECT(cursor.offset == 3 and progress == 3);
        SC_TEST_EXPECT(cursor.advanceBytes(1));
        SC_TEST_EXPECT(cursor.index == 3 and cursor.offset == 0 and progress == 4);
        SC_TEST_EXPECT(cursor.advanceBytes(1));
        SC_TEST_EXPECT(cursor.advanceBytes(1));
        SC_TEST_EXPECT(not cursor.hasRemaining() and progress == 6 and parts[1].sizeInBytes() == 4 and
                       parts[3].sizeInBytes() == 2);
        char                                 byte       = 0;
        Span<const char>                     enormous[] = {{&byte, size_t(-1)}, {&byte, 1}};
        detail::AsyncExecutionSendAllBuffers overflow{unused, enormous};
        overflow.initialize();
        SC_TEST_EXPECT(
            overflow.initialization.isError(AsyncExecutionResultCategory, AsyncExecutionError::TransferSizeOverflow));
        SC_TEST_EXPECT(not overflow.completesImmediately());
        char                               data[4] = {};
        AsyncExecutionReadResult           exactProgress;
        detail::AsyncExecutionReceiveExact exact{unused, data, &exactProgress};
        exact.initialize();
        SC_TEST_EXPECT(exact.advanceBytes(0, false).isError(AsyncExecutionResultCategory,
                                                            AsyncExecutionError::SocketReceiveNoProgress));
        SC_TEST_EXPECT(exact.advanceBytes(2, true).isError(AsyncExecutionResultCategory,
                                                           AsyncExecutionError::SocketReceiveIncomplete));
        SC_TEST_EXPECT(exactProgress.data.sizeInBytes() == 2 and exactProgress.endOfFile);
        detail::AsyncExecutionReceiveLine line{unused, data};
        line.initialize();
        SC_TEST_EXPECT(line.advanceByte(0, false).isError(AsyncExecutionResultCategory,
                                                          AsyncExecutionError::SocketReceiveNoProgress));
    }

    void fallibleTransforms()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create(2));
        const auto  owner            = Thread::CurrentThreadID();
        Atomic<int> encodersOnWorker = 0;
        char        encodedStorage[4];
        auto        encode = [&](Span<const char> bytes) noexcept
        {
            if (Thread::CurrentThreadID() != owner)
                encodersOnWorker.fetch_add(1);
            return base64Encode(bytes, encodedStorage);
        };
        auto   source = [&]() { return app.onPool([]() noexcept { return Span<const char>{"foo", 3}; }); };
        size_t count  = 0;
        SC_TEST_EXPECT(app.runInto(source() | ex::continues_on(app.getPoolScheduler()) | SC::tryThen(encode) |
                                       ex::then([](auto bytes) noexcept { return bytes.sizeInBytes(); }),
                                   count));
        SC_TEST_EXPECT(count == 4);
        StringPath path;
        SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory.view(), "AsyncExecutionFallibleTest.txt"}));
        SC_TEST_EXPECT(app.run(source() | ex::continues_on(app.getPoolScheduler()) | SC::tryThen(encode) |
                               ex::let_value([&](auto& bytes) { return app.writeFile(path.view(), bytes); })));
        char             destination[4];
        Span<const char> read;
        SC_TEST_EXPECT(app.runInto(app.readFile(path.view(), destination), read));
        SC_TEST_EXPECT(read.sizeInBytes() == 4 and memcmp(read.data(), "Zm9v", 4) == 0);
        SC_TEST_EXPECT(FileSystem::Operations::removeFile(path.view()));
        SC_TEST_EXPECT(encodersOnWorker.load() == 2);
        const Result failure    = Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge);
        int          downstream = 0;
        auto         fail = [failure]() noexcept { return AsyncExecutionValueResult<Span<const char>>{failure, {}}; };
        SC_TEST_EXPECT(app.run(ex::just() | SC::tryThen(fail) |
                               ex::let_value(
                                   [&](auto& bytes)
                                   {
                                       ++downstream;
                                       return app.writeFile(path.view(), bytes);
                                   }))
                           .isError(failure.category(), failure.errorValue()));
        SC_TEST_EXPECT(downstream == 0);
        SC_TEST_EXPECT(app.run(ex::just() | SC::tryThen(fail) | ex::then([&](auto) noexcept { ++downstream; }))
                           .isError(failure.category(), failure.errorValue()));
        SC_TEST_EXPECT(downstream == 0);
        SC_TEST_EXPECT(app.run(ex::just() | SC::tryThen(fail) |
                               ex::let_value(
                                   [&](auto& bytes)
                                   {
                                       ++downstream;
                                       return app.sendTCP("127.0.0.1", report.mapPort(19095), bytes);
                                   }))
                           .isError(failure.category(), failure.errorValue()));
        SC_TEST_EXPECT(downstream == 0);
        // Result-only transforms have a zero-value success signature and preserve Result identity on failure.
        SC_TEST_EXPECT(app.run(SC::tryThen(ex::just(7), [](int value) noexcept { return Result(value == 7); }) |
                               ex::then([&]() noexcept { ++downstream; })));
        SC_TEST_EXPECT(downstream == 1);
        SC_TEST_EXPECT(app.run(ex::just() | SC::tryThen([&]() noexcept { return failure; }))
                           .isError(failure.category(), failure.errorValue()));
        int sum = 0;
        SC_TEST_EXPECT(
            app.runInto(ex::just(17, 25) | SC::tryThen([](int a, int b) noexcept
                                                       { return AsyncExecutionValueResult<int>{Result(true), a + b}; }),
                        sum));
        SC_TEST_EXPECT(sum == 42);
        struct MoveOnly
        {
            int value;
            explicit MoveOnly(int value) noexcept : value(value) {}
            MoveOnly(MoveOnly&&) noexcept            = default;
            MoveOnly& operator=(MoveOnly&&) noexcept = default;
            MoveOnly(const MoveOnly&)                = delete;
        } retained{0};
        SC_TEST_EXPECT(app.runInto(
            ex::just() |
                SC::tryThen([]() noexcept { return AsyncExecutionValueResult<MoveOnly>{Result(true), MoveOnly{42}}; }),
            retained));
        SC_TEST_EXPECT(retained.value == 42);
        // Destroy the complete let_value parent from synchronous value/stopped notification.
        auto selfDestroying = []()
        {
            return ex::just(7) |
                   SC::tryThen([](int value) noexcept { return AsyncExecutionValueResult<int>{Result(true), value}; });
        };
        using Sender = decltype(selfDestroying());
        using Op     = decltype(ex::connect(std::declval<Sender>(), std::declval<ExecutionReceiver>()));
        struct Holder
        {
            Op operation;
            Holder(Sender sender, ExecutionReceiver receiver) : operation(ex::connect(move(sender), move(receiver))) {}
        };
        for (int stop = 0; stop < 2; ++stop)
        {
            ExecutionStopSource terminalStop;
            if (stop)
                terminalStop.request_stop();
            ExecutionOutcome     outcome;
            alignas(Holder) char storage[sizeof(Holder)];
            auto*                holder = reinterpret_cast<Holder*>(storage);
            placementNew(*holder, selfDestroying(), ExecutionReceiver{outcome, terminalStop.get_token()});
            bool destroyed  = false;
            outcome.onValue = [&]
            {
                holder->~Holder();
                destroyed = true;
            };
            outcome.onStopped = outcome.onValue;
            ex::start(holder->operation);
            SC_TEST_EXPECT(destroyed and outcome.values == 1 - stop and outcome.stopped == stop and
                           outcome.errors == 0);
        }
        ExecutionStopSource stopped;
        stopped.request_stop();
        int calls = 0;
        SC_TEST_EXPECT(app.run(ex::just() | SC::tryThen(
                                                [&]() noexcept
                                                {
                                                    ++calls;
                                                    return Result(true);
                                                }),
                               stopped.get_token())
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(calls == 0);
        ExecutionStopSource during;
        SC_TEST_EXPECT(app.run(ex::just() | ex::continues_on(app.getPoolScheduler()) |
                                   SC::tryThen(
                                       [&]() noexcept
                                       {
                                           ++calls;
                                           during.request_stop();
                                           return Result(true);
                                       }),
                               during.get_token())
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(calls == 1);
        ExecutionStopSource errorAndStop;
        SC_TEST_EXPECT(app.run(ex::just() | SC::tryThen(
                                                [&]() noexcept
                                                {
                                                    errorAndStop.request_stop();
                                                    return failure;
                                                }),
                               errorAndStop.get_token())
                           .isError(failure.category(), failure.errorValue()));
    }

    void composition()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create(2));
        const auto owner = Thread::CurrentThreadID();
        auto       loop  = app.getScheduler();
        auto       pool  = app.getPoolScheduler();
        static_assert(ex::sender_in<decltype(ex::schedule(loop))>);
        static_assert(ex::sender_in<decltype(ex::schedule(pool))>);
        SC_TEST_EXPECT(ex::get_completion_scheduler<ex::set_value_t>(ex::get_env(ex::schedule(loop))) == loop);
        SC_TEST_EXPECT(ex::get_completion_scheduler<ex::set_value_t>(ex::get_env(ex::schedule(pool))) == pool);
        SC_TEST_EXPECT(ex::get_forward_progress_guarantee(loop) == ex::forward_progress_guarantee::weakly_parallel);
        SC_TEST_EXPECT(ex::get_forward_progress_guarantee(pool) == ex::forward_progress_guarantee::weakly_parallel);

        int          answer   = -1;
        Atomic<bool> onWorker = false;
        auto         compute  = [&]()
        {
            return ex::just(21) | ex::then(
                                      [&](int value) noexcept
                                      {
                                          onWorker.store(Thread::CurrentThreadID() != owner);
                                          return value * 2;
                                      });
        };
        // on uses the root consumer's start scheduler to restore the owner thread.
        bool restored = false;
        SC_TEST_EXPECT(app.runInto(ex::on(pool, compute()) | ex::then(
                                                                 [&](int value) noexcept
                                                                 {
                                                                     restored = Thread::CurrentThreadID() == owner;
                                                                     return value;
                                                                 }),
                                   answer));
        SC_TEST_EXPECT(onWorker.load() and restored and answer == 42);
        onWorker.store(false);
        SC_TEST_EXPECT(app.runInto(ex::starts_on(pool, compute()) | ex::continues_on(loop), answer));
        SC_TEST_EXPECT(onWorker.load() and answer == 42);
        onWorker.store(false);
        SC_TEST_EXPECT(app.runInto(ex::schedule(loop) | ex::then([]() noexcept { return 21; }) |
                                       ex::on(pool, ex::then(
                                                        [&](int value) noexcept
                                                        {
                                                            onWorker.store(Thread::CurrentThreadID() != owner);
                                                            return value * 2;
                                                        })) |
                                       ex::then(
                                           [&](int value) noexcept
                                           {
                                               restored = Thread::CurrentThreadID() == owner;
                                               return value;
                                           }),
                                   answer));
        SC_TEST_EXPECT(onWorker.load() and restored and answer == 42);
        // Retain multiple outputs, including a move-only value, from standard composition.
        ExecutionTestBytes buffer;
        SC_TEST_EXPECT(buffer.resize(3));
        int                first = 0, second = 0;
        ExecutionTestBytes output;
        SC_TEST_EXPECT(
            app.runInto(ex::when_all(ex::just(17), ex::just(25), ex::just(move(buffer))), first, second, output));
        SC_TEST_EXPECT(first == 17 and second == 25 and output.size() == 3);
        SC_TEST_EXPECT(app.runInto(ex::just(99), first));
        SC_TEST_EXPECT(first == 99);
        const Result failure = Result::Error(AsyncResultCategory, AsyncError::InvalidAddress);
        SC_TEST_EXPECT(app.runInto(ex::just_error(failure), first).isError(failure.category(), failure.errorValue()));
        SC_TEST_EXPECT(first == 99);
        SC_TEST_EXPECT(
            app.runInto(ex::just_stopped(), first).isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(first == 99);
        SC_TEST_EXPECT(app.runInto(ex::just_error(failure) | ex::upon_error([](Result) noexcept { return 7; }), first));
        SC_TEST_EXPECT(first == 7);
        SC_TEST_EXPECT(app.runInto(ex::just_stopped() | ex::upon_stopped([]() noexcept { return 8; }), first));
        SC_TEST_EXPECT(first == 8);
        ExecutionStopSource stop;
        stop.request_stop();
        SC_TEST_EXPECT(
            app.runIntoWithStop(ex::schedule(pool) | ex::then([]() noexcept { return 1; }), stop.get_token(), first)
                .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(first == 8);
        // Worker completion publishes its output before notifying the owner loop.
        SC_TEST_EXPECT(app.runInto(ex::starts_on(pool, ex::just(55)), first));
        SC_TEST_EXPECT(first == 55);
        Result nested{true};
        SC_TEST_EXPECT(app.run(app.sleep({1}) | ex::then([&]() noexcept { nested = app.run(ex::just()); })));
        SC_TEST_EXPECT(nested.isError(AsyncExecutionResultCategory, AsyncExecutionError::AlreadyRunning));
    }

    void loopIntegration()
    {
        auto graph = [](AsyncExecutionContext& context)
        { return context.sleep({1}) | ex::then([]() noexcept { return 42; }); };
        AsyncExecution app;
        SC_TEST_EXPECT(app.create());
        int output = 0;
        SC_TEST_EXPECT(app.runInto(graph(app.getContext()), output));
        SC_TEST_EXPECT(output == 42);

        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.run(ex::just()).isError(AsyncResultCategory, AsyncError::NotInitialized));
        SC_TEST_EXPECT(context.create());
        SC_TEST_EXPECT(&context.getEventLoop() == &loop);
        SC_TEST_EXPECT(context.runInto(graph(context), output));
        SC_TEST_EXPECT(output == 42);
        AsyncLoopTimeout interruptor;
        interruptor.callback = [&](AsyncLoopTimeout::Result&) { loop.interrupt(); };
        SC_TEST_EXPECT(interruptor.start(loop, {0}));
        SC_TEST_EXPECT(context.runInto(graph(context), output));
        SC_TEST_EXPECT(output == 42);
        Result foreignRun{true};
        Thread foreign;
        SC_TEST_EXPECT(foreign.start([&](Thread&) { foreignRun = context.run(ex::just()); }));
        SC_TEST_EXPECT(foreign.join());
        SC_TEST_EXPECT(foreignRun.isError(AsyncResultCategory, AsyncError::InvalidState));
        const auto owner = Thread::CurrentThreadID();
        for (int mode = 0; mode < 4; ++mode)
        {
            ExecutionOutcome outcome;
            bool             timedOut = false, correct = false;
            AsyncLoopTimeout watchdog;
            watchdog.callback = [&](AsyncLoopTimeout::Result&)
            {
                timedOut = true;
                loop.interrupt();
            };
            SC_TEST_EXPECT(watchdog.start(loop, {2000}));
            auto sender    = graph(context) | ex::then([&](int value) noexcept
                                                    { correct = value == 42 and Thread::CurrentThreadID() == owner; });
            auto operation = ex::connect(move(sender), ExecutionReceiver{outcome, {}});
            if (mode == 0)
                outcome.loop = &loop;
            ex::start(operation);
            if (mode == 0)
                SC_TEST_EXPECT(loop.run());
            else
            {
                const int64_t deadline = Time::Monotonic::now().getMonotonicMilliseconds() + 2500;
                while (outcome.values == 0 and not timedOut)
                {
                    if (Time::Monotonic::now().getMonotonicMilliseconds() >= deadline)
                    {
                        timedOut = true;
                        break;
                    }
                    if (mode == 1)
                        SC_TEST_EXPECT(loop.runOnce());
                    else if (mode == 2)
                    {
                        SC_TEST_EXPECT(loop.runNoWait());
                        Thread::Sleep(1);
                    }
                    else
                    {
                        alignas(uint64_t) uint8_t eventStorage[8 * 1024];
                        AsyncKernelEvents         events;
                        events.eventsMemory = eventStorage;
                        SC_TEST_EXPECT(loop.submitRequests(events));
                        SC_TEST_EXPECT(loop.blockingPoll(events));
                        SC_TEST_EXPECT(loop.dispatchCompletions(events));
                    }
                }
            }
            if (not watchdog.isFree())
                SC_TEST_EXPECT(watchdog.unschedule(loop));
            if (timedOut or not correct or outcome.values != 1 or outcome.errors != 0)
            {
                report.console.print("Integration mode {}, timeout={}\n", size_t(mode), size_t(timedOut));
                report.console.print("correct={} values={}\n", size_t(correct), size_t(outcome.values));
                report.console.print("errors={}\n", size_t(outcome.errors));
            }
            SC_ASYNC_ASSERT_RELEASE(not timedOut and outcome.values == 1);
            SC_TEST_EXPECT(correct and outcome.errors == 0);
        }
        // sync_wait owns its own wait loop; this separate thread waits while the SC owner drives SC I/O.
        AsyncLoopWakeUp finished;
        finished.callback = [&](AsyncLoopWakeUp::Result&) { loop.interrupt(); };
        SC_TEST_EXPECT(finished.start(loop));
        struct WaitState
        {
            AsyncExecutionContext& context;
            AsyncLoopWakeUp&       finished;
            int                    value    = 0;
            bool                   complete = false;
        } wait{context, finished};
        Thread consumer;
        SC_TEST_EXPECT(consumer.start(
            [&wait](Thread&)
            {
                auto result   = ex::sync_wait(wait.context.sleep({1}) | ex::then([]() noexcept { return 64; }));
                wait.complete = result.has_value();
                if (result)
                    wait.value = std::get<0>(*result);
                SC_ASYNC_TRUST_RESULT(wait.finished.wakeUp(wait.context.getEventLoop()));
            }));
        const Result driven = runBounded(loop);
        SC_ASYNC_ASSERT_RELEASE(driven); // A failed driver cannot safely unwind a consumer still waiting for I/O.
        SC_TEST_EXPECT(driven);
        SC_TEST_EXPECT(consumer.join());
        SC_TEST_EXPECT(wait.complete and wait.value == 64);
        if (not finished.isFree())
            SC_TEST_EXPECT(finished.stop(loop));
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

    void poolProgress()
    {
        AsyncEventLoop loop;
        ThreadPool     pool;
        SC_TEST_EXPECT(loop.create());
        SC_TEST_EXPECT(pool.create(1));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        SocketDescriptor listener, client, accepted;
        SocketIPAddress  address;
        const uint16_t   port = report.mapPort(5094);
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

        EventObject      releaseCPU;
        Atomic<bool>     started = false, ioFinished = false, forcedRelease = false;
        bool             timerProgress = false, socketProgress = false;
        ExecutionOutcome cpuOutcome, ioOutcome;
        ioOutcome.loop = &loop;
        auto cpu       = ex::connect(ex::schedule(AsyncExecutionPoolScheduler{&context, &pool}) | ex::then(
                                                                                                [&]() noexcept
                                                                                                {
                                                                                                    started.store(true);
                                                                                                    releaseCPU.wait();
                                                                                                }),
                                     ExecutionReceiver{cpuOutcome, {}});
        ex::start(cpu);
        for (int i = 0; i < 1000 and not started.load(); ++i)
            Thread::Sleep(1);

        // This guard is independent of the owner loop. A stalled loop cannot run its own watchdog timer.
        struct Guard
        {
            Atomic<bool>& done;
            Atomic<bool>& forced;
            EventObject&  release;
        } guard{ioFinished, forcedRelease, releaseCPU};
        Thread failsafe;
        SC_TEST_EXPECT(failsafe.start(
            [&guard](Thread&)
            {
                for (int i = 0; i < 1000 and not guard.done.load(); ++i)
                    Thread::Sleep(1);
                if (not guard.done.load())
                {
                    guard.forced.store(true);
                    guard.release.signal();
                }
            }));
        char buffer[16] = {};
        auto io         = ex::when_all(
                      context.sleep({10}) | ex::then([&]() noexcept { timerProgress = not forcedRelease.load(); }),
                      context.sendAll(client, StringView("progress").toCharSpan()) | ex::then([](size_t) noexcept {}),
                      context.receive(accepted, buffer) |
                          ex::then(
                              [&](AsyncExecutionReadResult data) noexcept
                              {
                                  socketProgress = not forcedRelease.load() and data.data.sizeInBytes() == 8 and
                                                   memcmp(data.data.data(), "progress", 8) == 0;
                              })) |
                  ex::then(
                      [&]() noexcept
                      {
                          ioFinished.store(true);
                          releaseCPU.signal();
                      });
        auto operation = ex::connect(move(io), ExecutionReceiver{ioOutcome, {}});
        ex::start(operation);
        const Result ran = runBounded(loop);
        SC_TEST_EXPECT(failsafe.join());
        SC_TEST_EXPECT(pool.waitForAllTasks());
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
        SC_TEST_EXPECT(pool.destroy());
        SC_TEST_EXPECT(ran and started.load() and ioOutcome.values == 1 and cpuOutcome.values == 1);
        SC_TEST_EXPECT(not forcedRelease.load());
        SC_TEST_EXPECT(timerProgress and socketProgress);
    }

    void poolLifetime()
    {
        AsyncEventLoop loop;
        ThreadPool     pool;
        SC_TEST_EXPECT(loop.create());
        SC_TEST_EXPECT(pool.create(1));
        AsyncExecutionContext context{loop};
        SC_TEST_EXPECT(context.create());
        const AsyncExecutionPoolScheduler scheduler{&context, &pool};
        const auto                        loopScheduler = context.getScheduler();
        SC_TEST_EXPECT(ex::get_completion_scheduler<ex::set_value_t>(loopScheduler) ==
                       ex::get_completion_scheduler<ex::set_value_t>(ex::get_env(ex::schedule(loopScheduler))));
        SC_TEST_EXPECT(ex::get_completion_scheduler<ex::set_value_t>(scheduler) ==
                       ex::get_completion_scheduler<ex::set_value_t>(ex::get_env(ex::schedule(scheduler))));
        // Queue stop before the worker can dequeue the schedule request.
        ThreadPoolTask blocker;
        EventObject    entered, release;
        struct Gate
        {
            EventObject& entered;
            EventObject& release;
        } gate{entered, release};
        blocker.function = [&gate]()
        {
            gate.entered.signal();
            gate.release.wait();
        };
        SC_TEST_EXPECT(pool.queueTask(blocker));
        entered.wait();
        ExecutionStopSource queuedStop;
        ExecutionOutcome    queuedOutcome;
        auto queued = ex::connect(ex::schedule(scheduler), ExecutionReceiver{queuedOutcome, queuedStop.get_token()});
        ex::start(queued);
        queuedStop.request_stop();
        release.signal();
        SC_TEST_EXPECT(pool.waitForAllTasks());
        SC_TEST_EXPECT(queuedOutcome.stopped == 1 and queuedOutcome.values == 0 and queuedOutcome.errors == 0);

        for (int i = 0; i < 32; ++i)
        {
            ExecutionStopSource source;
            ExecutionOutcome    outcome;
            auto   operation = ex::connect(ex::schedule(scheduler), ExecutionReceiver{outcome, source.get_token()});
            Thread canceller;
            SC_TEST_EXPECT(canceller.start([&](Thread&) { source.request_stop(); }));
            ex::start(operation);
            SC_TEST_EXPECT(canceller.join());
            SC_TEST_EXPECT(pool.waitForAllTasks());
            SC_TEST_EXPECT(outcome.values + outcome.stopped == 1 and outcome.errors == 0);
        }
        for (bool stop : {false, true})
        {
            ExecutionStopSource source;
            ExecutionOutcome    outcome;
            using Operation =
                decltype(ex::connect(ex::schedule(scheduler), ExecutionReceiver{outcome, source.get_token()}));
            alignas(Operation) char storage[sizeof(Operation)];
            auto*                   operation = reinterpret_cast<Operation*>(storage);
            placementNew(*operation, context, pool, ExecutionReceiver{outcome, source.get_token()});
            bool destroyed  = false;
            outcome.onValue = [operation, &destroyed]()
            {
                operation->~Operation();
                destroyed = true;
            };
            outcome.onStopped = outcome.onValue;
            if (stop)
                source.request_stop();
            ex::start(*operation);
            SC_TEST_EXPECT(pool.waitForAllTasks());
            SC_TEST_EXPECT(destroyed and outcome.values + outcome.stopped == 1 and outcome.errors == 0);
        }
        struct RunningCPU
        {
            ExecutionStopSource source;
            EventObject         entered, release;
            Atomic<bool>        finished = false;
        } running;
        ExecutionOutcome runningOutcome;
        runningOutcome.loop = &loop;
        auto graph          = ex::schedule(scheduler) |
                     ex::then(
                         [&running]() noexcept
                         {
                             running.entered.signal();
                             running.release.wait();
                             running.finished.store(true);
                         }) |
                     ex::continues_on(context.getScheduler());
        auto runningOperation = ex::connect(move(graph), ExecutionReceiver{runningOutcome, running.source.get_token()});
        Thread canceller;
        SC_TEST_EXPECT(canceller.start(
            [&running](Thread&)
            {
                running.entered.wait();
                running.source.request_stop();
                running.release.signal();
            }));
        ex::start(runningOperation);
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(canceller.join());
        SC_TEST_EXPECT(pool.waitForAllTasks());
        SC_TEST_EXPECT(running.finished.load() and runningOutcome.stopped == 1 and runningOutcome.values == 0 and
                       runningOutcome.errors == 0);
        // Task release precedes receiver return. Whole-pool joins must include that notification tail.
        ExecutionOutcome tailOutcome;
        struct TailState
        {
            EventObject  entered, release;
            Atomic<bool> finished = false;
        } tailState;
        tailOutcome.onValue = [&tailState]()
        {
            tailState.entered.signal();
            tailState.release.wait();
            tailState.finished.store(true);
        };
        auto tail = ex::connect(ex::schedule(scheduler), ExecutionReceiver{tailOutcome, {}});
        ex::start(tail);
        tailState.entered.wait();
        SC_TEST_EXPECT(pool.waitForTask(tail.task.task));
        SC_TEST_EXPECT(not tailState.finished.load());
        Thread releaseTail;
        SC_TEST_EXPECT(releaseTail.start(
            [&](Thread&)
            {
                Thread::Sleep(10);
                tailState.release.signal();
            }));
        SC_TEST_EXPECT(pool.waitForAllTasks());
        SC_TEST_EXPECT(tailState.finished.load());
        SC_TEST_EXPECT(releaseTail.join());

        tailState.finished.store(false);
        auto destroyTail = ex::connect(ex::schedule(scheduler), ExecutionReceiver{tailOutcome, {}});
        ex::start(destroyTail);
        tailState.entered.wait();
        SC_TEST_EXPECT(releaseTail.start(
            [&](Thread&)
            {
                Thread::Sleep(10);
                tailState.release.signal();
            }));
        SC_TEST_EXPECT(pool.destroy());
        SC_TEST_EXPECT(tailState.finished.load());
        SC_TEST_EXPECT(releaseTail.join());

        // Submission failure must release context accounting and permit terminal self-destruction too.
        ExecutionOutcome failedOutcome;
        using FailedOperation = decltype(ex::connect(ex::schedule(scheduler), ExecutionReceiver{failedOutcome, {}}));
        alignas(FailedOperation) char failedStorage[sizeof(FailedOperation)];
        auto*                         failed = reinterpret_cast<FailedOperation*>(failedStorage);
        placementNew(*failed, context, pool, ExecutionReceiver{failedOutcome, {}});
        bool failedDestroyed  = false;
        failedOutcome.onError = [failed, &failedDestroyed]()
        {
            failed->~FailedOperation();
            failedDestroyed = true;
        };
        ex::start(*failed);
        SC_TEST_EXPECT(failedDestroyed and failedOutcome.errors == 1 and failedOutcome.values == 0 and
                       failedOutcome.stopped == 0);
        SC_TEST_EXPECT(failedOutcome.error.isError(ThreadingResultCategory, ThreadingError::ThreadPoolNotCreated));
        SC_TEST_EXPECT(context.close());
        SC_TEST_EXPECT(loop.close());
    }

#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
    void scopeTypedStorage()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create(2));
        auto& context = app.getContext();
        {
            AsyncExecutionScope unconfigured(context);
            ScopeHeldState      state;
            int                 moves = 0;
            ScopeHeldSender     sender{state, moves};
            SC_TEST_EXPECT(unconfigured.spawn(move(sender))
                               .isError(AsyncExecutionResultCategory, AsyncExecutionError::InvalidAllocator));
            SC_TEST_EXPECT(moves == 0 and not state.complete.isValid());
        }
        {
            AsyncExecutionScopePool<ScopeHeldSender> pool;
            AsyncExecutionScope                      scope(context);
            ScopeHeldState                           state;
            int                                      moves = 0;
            ScopeHeldSender                          sender{state, moves};
            SC_TEST_EXPECT(scope.spawn(pool, move(sender)).isError(AsyncResultCategory, AsyncError::NotInitialized));
            SC_TEST_EXPECT(pool.create({}));
            SC_TEST_EXPECT(scope.spawn(pool, move(sender))
                               .isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeCapacityExhausted));
            SC_TEST_EXPECT(moves == 0 and scope.getSummary().accepted == 0);
            SC_TEST_EXPECT(pool.close());
        }
        {
            AsyncExecutionScopeSlot<ScopeHeldSender> slots[1];
            AsyncExecutionScopePool<ScopeHeldSender> pool, other;
            SC_TEST_EXPECT(pool.create(slots));
            SC_TEST_EXPECT(pool.capacity() == 1 and pool.availableCount() == 1 and pool.activeCount() == 0);
            SC_TEST_EXPECT(
                other.create(slots).isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeStorageInUse));
            SC_TEST_EXPECT(other.capacity() == 0);
            // Even an explicit heap policy must never spill a typed overload into allocation.
            ScopeTestAllocator  allocator;
            AsyncExecutionScope scope(context, allocator.callbacks());
            ScopeHeldState      first, rejected, second;
            int                 firstMoves = 0, rejectedMoves = 0, secondMoves = 0;
            ScopeHeldSender     a{first, firstMoves}, b{rejected, rejectedMoves};
            SC_TEST_EXPECT(scope.spawn(slots[0], move(b))
                               .isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeStorageInUse));
            SC_TEST_EXPECT(rejectedMoves == 0);
            SC_TEST_EXPECT(scope.spawn(pool, move(a)));
            SC_TEST_EXPECT(firstMoves > 0 and pool.activeCount() == 1 and pool.availableCount() == 0);
            SC_TEST_EXPECT(
                pool.create(slots).isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeStorageInUse));
            SC_TEST_EXPECT(scope.spawn(pool, move(b))
                               .isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeCapacityExhausted));
            SC_TEST_EXPECT(rejectedMoves == 0 and not rejected.complete.isValid());
            SC_TEST_EXPECT(pool.close().isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeStorageInUse));
            first.complete();
            SC_TEST_EXPECT(first.destroyed.load() == 1 and pool.availableCount() == 1);
            SC_TEST_EXPECT(scope.spawn(pool, ScopeHeldSender{second, secondMoves}));
            Thread completion;
            SC_TEST_EXPECT(completion.start([&second](Thread&) { second.complete(); }));
            SC_TEST_EXPECT(completion.join());
            SC_TEST_EXPECT(second.destroyed.load() == 1 and pool.activeCount() == 0);
            scope.requestStop();
            SC_TEST_EXPECT(
                scope.spawn(pool, move(b)).isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeClosed));
            SC_TEST_EXPECT(rejectedMoves == 0 and pool.availableCount() == 1);
            SC_TEST_EXPECT(consumeBounded(context, scope.join()));
            SC_TEST_EXPECT(scope.getSummary().accepted == 2 and allocator.allocations.load() == 0);
            SC_TEST_EXPECT(pool.close());
            // Unbound storage can now serve a different scope directly.
            AsyncExecutionScope direct(context);
            SC_TEST_EXPECT(direct.spawn(slots[0], move(b)));
            int anotherMoves = 0;
            SC_TEST_EXPECT(direct.spawn(slots[0], ScopeHeldSender{first, anotherMoves})
                               .isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeStorageInUse));
            SC_TEST_EXPECT(anotherMoves == 0);
            SC_TEST_EXPECT(
                other.create(slots).isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeStorageInUse));
            rejected.complete();
            SC_TEST_EXPECT(consumeBounded(context, direct.join()));
        }
        {
            bool                                      aligned = false, destroyed = false, environment = false;
            AsyncExecutionScopeSlot<ScopeProbeSender> slot;
            static_assert(alignof(decltype(slot)) >= 256);
            AsyncExecutionScope scope(context);
            SC_TEST_EXPECT(scope.spawn(slot, ScopeProbeSender{&aligned, &destroyed, &environment}));
            SC_TEST_EXPECT(aligned and destroyed and environment);
            SC_TEST_EXPECT(consumeBounded(context, scope.join()));
        }
        {
            // Synchronous reclamation supports concurrent producers without an allocator or an owner-loop hop.
            using Sender = decltype(ex::just(1));
            AsyncExecutionScopeSlot<Sender> slots[2];
            AsyncExecutionScopePool<Sender> pool;
            SC_TEST_EXPECT(pool.create(slots));
            AsyncExecutionScope scope(context);
            Result              one{true}, two{true};
            auto                submit = [&](Result& result)
            {
                for (int i = 0; i < 128 and result; ++i)
                    result = scope.spawn(pool, ex::just(1));
            };
            Thread a, b;
            SC_TEST_EXPECT(a.start([&](Thread&) { submit(one); }));
            SC_TEST_EXPECT(b.start([&](Thread&) { submit(two); }));
            SC_TEST_EXPECT(a.join());
            SC_TEST_EXPECT(b.join());
            SC_TEST_EXPECT(one and two);
            SC_TEST_EXPECT(pool.availableCount() == 2);
            SC_TEST_EXPECT(consumeBounded(context, scope.join()));
            SC_TEST_EXPECT(scope.getSummary().succeeded == 256);
            SC_TEST_EXPECT(pool.close());
        }
        {
            // Reentrant acceptance of a different graph type within one scope, with caller-owned storage.
            AsyncExecutionScope                           scope(context);
            AsyncExecutionScopeSlot<decltype(ex::just())> inner;
            Result                                        nested{false};
            auto sender = ex::just() | ex::then([&]() noexcept { nested = scope.spawn(inner, ex::just()); });
            AsyncExecutionScopeSlot<decltype(sender)> outer;
            SC_TEST_EXPECT(scope.spawn(outer, move(sender)));
            SC_TEST_EXPECT(nested);
            SC_TEST_EXPECT(consumeBounded(context, scope.join()));
            SC_TEST_EXPECT(scope.getSummary().succeeded == 2);
        }
        {
            // Scope join also covers worker notification tails after the typed operation slot is reclaimed.
            Atomic<int> ran      = 0;
            auto        makeWork = [&]()
            { return ex::schedule(app.getPoolScheduler()) | ex::then([&]() noexcept { ran.fetch_add(1); }); };
            using Sender = decltype(makeWork());
            AsyncExecutionScopeSlot<Sender> slots[2];
            AsyncExecutionScopePool<Sender> pool;
            SC_TEST_EXPECT(pool.create(slots));
            AsyncExecutionScope scope(context);
            SC_TEST_EXPECT(scope.spawn(pool, makeWork()));
            const Result second = scope.spawn(pool, makeWork());
            if (not second)
                scope.requestStop();
            SC_TEST_EXPECT(consumeBounded(context, scope.join()));
            SC_TEST_EXPECT(second and ran.load() == 2 and pool.activeCount() == 0);
            SC_TEST_EXPECT(pool.close());
        }
        {
            // Join's terminal receiver can destroy pool, slots, scope and its own operation immediately.
            using Sender = decltype(context.sleep({1}));
            struct Owner
            {
                AsyncExecutionScopeSlot<Sender> slots[1];
                AsyncExecutionScopePool<Sender> pool;
                AsyncExecutionScope             scope;
                explicit Owner(AsyncExecutionContext& context) : scope(context) {}
            };
            AlignedStorage<sizeof(Owner), alignof(Owner)> storage;
            auto*                                         owner = reinterpret_cast<Owner*>(&storage);
            placementNew(*owner, context);
            SC_TEST_EXPECT(owner->pool.create(owner->slots));
            SC_TEST_EXPECT(owner->scope.spawn(owner->pool, context.sleep({1})));
            ExecutionOutcome outcome;
            outcome.loop = &context.getEventLoop();
            using Join   = decltype(ex::connect(owner->scope.join(), ExecutionReceiver{outcome, {}}));
            AlignedStorage<sizeof(Join), alignof(Join)> joinStorage;
            auto*                                       join = reinterpret_cast<Join*>(&joinStorage);
            placementNew(*join, owner->scope, ExecutionReceiver{outcome, {}});
            struct Destroy
            {
                Owner* owner;
                Join*  join;
                bool   done = false;
            } destroy{owner, join};
            outcome.onValue = [this, &destroy]()
            {
                SC_TEST_EXPECT(destroy.owner->pool.availableCount() == 1);
                destroy.join->~Join();
                destroy.owner->~Owner();
                destroy.done = true;
            };
            ex::start(*join);
            SC_TEST_EXPECT(runBounded(context.getEventLoop()));
            SC_TEST_EXPECT(destroy.done and outcome.values == 1);
        }
        SC_TEST_EXPECT(app.close());
    }

    void scopeBasics()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create(2));
        auto& context = app.getContext();
        {
            AsyncExecutionScope        empty(context, AsyncExecutionScopeAllocator::heap());
            AsyncExecutionScopeSummary summary;
            SC_TEST_EXPECT(consumeBounded(context, empty.join(), summary));
            SC_TEST_EXPECT(summary.accepted == 0 and summary.firstError);
            SC_TEST_EXPECT(
                empty.spawn(ex::just()).isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeClosed));
        }
        {
            struct Allocation
            {
                int attempts = 0;
                int freed    = 0;
            } allocation;
            AsyncExecutionScopeAllocator allocator;
            allocator.userData = &allocation;
            allocator.allocate = [](void* user, size_t, size_t) noexcept -> void*
            {
                ++static_cast<Allocation*>(user)->attempts;
                return nullptr;
            };
            allocator.deallocate = [](void* user, void*, size_t, size_t) noexcept
            { ++static_cast<Allocation*>(user)->freed; };
            AsyncExecutionScope failed(context, allocator);
            SC_TEST_EXPECT(
                failed.spawn(ex::just()).isError(AsyncExecutionResultCategory, AsyncExecutionError::OutOfMemory));
            SC_TEST_EXPECT(failed.getSummary().accepted == 0 and allocation.attempts == 1 and allocation.freed == 0);
            // No provider association was acquired, so a failed first allocation does not require join.
        }
        {
            AsyncExecutionScopeAllocator allocator;
            allocator.allocate = [](void*, size_t, size_t) noexcept -> void* { return nullptr; };
            AsyncExecutionScope invalid(context, allocator);
            SC_TEST_EXPECT(
                invalid.spawn(ex::just()).isError(AsyncExecutionResultCategory, AsyncExecutionError::InvalidAllocator));
        }
        {
            AsyncExecutionScope scope(context, AsyncExecutionScopeAllocator::heap());
            const Result        error = Result::Error(ResultCategory(99), 71);
            SC_TEST_EXPECT(scope.spawn(ex::just(1)));
            SC_TEST_EXPECT(scope.spawn(ex::just_error(error)));
            SC_TEST_EXPECT(scope.spawn(ex::just_error(Result::Error(ResultCategory(99), 72))));
            SC_TEST_EXPECT(scope.spawn(ex::just_stopped()));
            // Even synchronous roots need an explicit join before the borrowing context can close.
            SC_TEST_EXPECT(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
            SC_TEST_EXPECT(consumeBounded(context, scope.join()).isError(ResultCategory(99), 71));
            const auto summary = scope.getSummary();
            SC_TEST_EXPECT(summary.accepted == 4 and summary.succeeded == 1 and summary.failed == 2 and
                           summary.stopped == 1);
            SC_TEST_EXPECT(summary.firstError.isError(ResultCategory(99), 71));
            SC_TEST_EXPECT(consumeBounded(context, scope.join()).isError(ResultCategory(99), 71));
        }
        {
            AsyncExecutionScope scope(context, AsyncExecutionScopeAllocator::heap());
            auto                lazy    = scope.join();
            bool                aligned = false, destroyed = false, environment = false;
            SC_TEST_EXPECT(
                scope.spawn(ScopeProbeSender{&aligned, &destroyed, &environment} | ex::then([]() noexcept {})));
            SC_TEST_EXPECT(consumeBounded(context, move(lazy)));
            SC_TEST_EXPECT(aligned and destroyed and environment);
        }
        {
            ScopeTestAllocator  allocation;
            AsyncExecutionScope scope(context, allocation.callbacks());
            SC_TEST_EXPECT(scope.spawn(context.sleep({5000})));
            SC_TEST_EXPECT(
                scope.spawn(ex::just()).isError(AsyncExecutionResultCategory, AsyncExecutionError::OutOfMemory));
            SC_TEST_EXPECT(allocation.busy.load() and allocation.releases.load() == 0);
            scope.requestStop();
            AsyncExecutionScopeSummary summary;
            SC_TEST_EXPECT(consumeBounded(context, scope.join(), summary));
            SC_TEST_EXPECT(summary.accepted == 1 and summary.stopped == 1);
            SC_TEST_EXPECT(allocation.allocations.load() == 2 and allocation.releases.load() == 1 and
                           not allocation.busy.load() and allocation.correct.load());
        }
        {
            ThreadPool          uncreated;
            AsyncExecutionScope scope(context, AsyncExecutionScopeAllocator::heap());
            SC_TEST_EXPECT(scope.spawn(ex::schedule(AsyncExecutionPoolScheduler{&context, &uncreated})));
            SC_TEST_EXPECT(consumeBounded(context, scope.join())
                               .isError(ThreadingResultCategory, ThreadingError::ThreadPoolNotCreated));
            SC_TEST_EXPECT(scope.getSummary().accepted == 1 and scope.getSummary().failed == 1);
        }
        {
            AsyncExecutionScope grouped(context, AsyncExecutionScopeAllocator::heap());
            bool                branches[2] = {};
            SC_TEST_EXPECT(grouped.spawn(ex::when_all(
                context.sleep({1}) | ex::then([&branches]() noexcept { branches[0] = true; }),
                ex::schedule(app.getPoolScheduler()) | ex::then([&branches]() noexcept { branches[1] = true; }))));
            AsyncExecutionScopeSummary summary;
            SC_TEST_EXPECT(consumeBounded(context, grouped.join(), summary));
            SC_TEST_EXPECT(branches[0] and branches[1] and summary.accepted == 1 and summary.succeeded == 1);
        }
        SC_TEST_EXPECT(app.close());
    }

    void scopeProgress()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create(2));
        auto&               context = app.getContext();
        AsyncExecutionScope scope(context, AsyncExecutionScopeAllocator::heap());
        struct Gate
        {
            EventObject  entered, release;
            Atomic<int>  cpu    = 0;
            Atomic<bool> done   = false;
            Atomic<bool> forced = false;
            bool         timer  = false;
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
        SC_TEST_EXPECT(scope.spawn(ex::schedule(app.getPoolScheduler()) | ex::then(
                                                                              [&gate]() noexcept
                                                                              {
                                                                                  gate.entered.signal();
                                                                                  gate.release.wait();
                                                                                  gate.cpu.fetch_add(1);
                                                                              })));
        gate.entered.wait();
        SC_TEST_EXPECT(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
        SC_TEST_EXPECT(scope.spawn(context.sleep({10}) | ex::then(
                                                             [&gate]() noexcept
                                                             {
                                                                 gate.timer = true;
                                                                 gate.release.signal();
                                                             })));
        SC_TEST_EXPECT(scope.spawn(context.sleep({1}) | ex::continues_on(app.getPoolScheduler()) |
                                   ex::then([&gate]() noexcept { gate.cpu.fetch_add(1); })));
        AsyncExecutionScopeSummary summary;
        SC_TEST_EXPECT(consumeBounded(context, scope.join(), summary));
        gate.done.store(true);
        SC_TEST_EXPECT(guard.join());
        SC_TEST_EXPECT(not gate.forced.load());
        SC_TEST_EXPECT(gate.timer and gate.cpu.load() == 2 and summary.accepted == 3 and summary.succeeded == 3);
        SC_TEST_EXPECT(app.close());

        SC_TEST_EXPECT(app.create(2));
        {
            AsyncExecutionScope cpuScope(context, AsyncExecutionScopeAllocator::heap());
            struct CPU
            {
                EventObject  entered, release;
                Atomic<bool> done     = false;
                Atomic<bool> finished = false;
                Atomic<bool> forced   = false;
            } cpu;
            Thread cpuGuard;
            SC_TEST_EXPECT(cpuGuard.start(
                [&cpu](Thread&)
                {
                    for (int attempt = 0; attempt < 2000 and not cpu.done.load(); ++attempt)
                        Thread::Sleep(1);
                    if (not cpu.done.load())
                    {
                        cpu.forced.store(true);
                        cpu.entered.signal();
                        cpu.release.signal();
                    }
                    for (int attempt = 0; attempt < 2000 and not cpu.done.load(); ++attempt)
                        Thread::Sleep(1);
                    if (not cpu.done.load())
                        ::_Exit(124);
                }));
            SC_TEST_EXPECT(cpuScope.spawn(ex::schedule(app.getPoolScheduler()) |
                                          ex::then(
                                              [&cpu]() noexcept
                                              {
                                                  cpu.entered.signal();
                                                  cpu.release.wait();
                                                  cpu.finished.store(true);
                                              }) |
                                          ex::continues_on(context.getScheduler())));
            cpu.entered.wait();
            cpuScope.requestStop();
            SC_TEST_EXPECT(cpuScope.getSummary().accepted == 1 and cpuScope.getSummary().stopped == 0);
            SC_TEST_EXPECT(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
            ExecutionOutcome outcome;
            outcome.loop = &context.getEventLoop();
            auto join    = ex::connect(cpuScope.join(), ExecutionReceiver{outcome, {}});
            ex::start(join);
            SC_TEST_EXPECT(outcome.values == 0 and outcome.errors == 0);
            cpu.release.signal();
            SC_TEST_EXPECT(runBounded(context.getEventLoop()));
            cpu.done.store(true);
            SC_TEST_EXPECT(cpuGuard.join());
            SC_TEST_EXPECT(not cpu.forced.load() and cpu.finished.load());
            SC_TEST_EXPECT(outcome.values == 1 and cpuScope.getSummary().stopped == 1);
        }
        {
            AsyncExecutionScope stopped(context, AsyncExecutionScopeAllocator::heap());
            SC_TEST_EXPECT(stopped.spawn(context.sleep({5000})));
            ExecutionStopSource stop;
            stop.request_stop();
            SC_TEST_EXPECT(context.runIntoWithStop(stopped.join(), stop.get_token(), summary));
            SC_TEST_EXPECT(summary.stopped == 1 and summary.failed == 0);
            SC_TEST_EXPECT(
                stopped.spawn(ex::just()).isError(AsyncExecutionResultCategory, AsyncExecutionError::ScopeClosed));
        }
        {
            AsyncExecutionScope parallel(context, AsyncExecutionScopeAllocator::heap());
            Atomic<int>         ran = 0;
            Result              first{true}, second{true};
            const auto          scheduler = app.getPoolScheduler();
            struct Spawn
            {
                AsyncExecutionScope&        scope;
                AsyncExecutionPoolScheduler scheduler;
                Atomic<int>&                ran;
                Result&                     status;
            } one{parallel, scheduler, ran, first}, two{parallel, scheduler, ran, second};
            auto work = [](Spawn& spawn)
            {
                for (int i = 0; i < 16 and spawn.status; ++i)
                    spawn.status = spawn.scope.spawn(ex::schedule(spawn.scheduler) |
                                                     ex::then([&ran = spawn.ran]() noexcept { ran.fetch_add(1); }));
            };
            Thread a, b;
            SC_TEST_EXPECT(a.start([&one, &work](Thread&) { work(one); }));
            SC_TEST_EXPECT(b.start([&two, &work](Thread&) { work(two); }));
            SC_TEST_EXPECT(a.join());
            SC_TEST_EXPECT(b.join());
            SC_TEST_EXPECT(first and second);
            SC_TEST_EXPECT(consumeBounded(context, parallel.join(), summary));
            SC_TEST_EXPECT(ran.load() == 32 and summary.accepted == 32 and summary.succeeded == 32);
        }
        SC_TEST_EXPECT(app.close());
    }

    void scopeJoinLifetime()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.create());
        auto& context = app.getContext();
        auto& loop    = context.getEventLoop();
        {
            AsyncExecutionScope scope(context, AsyncExecutionScopeAllocator::heap());
            SC_TEST_EXPECT(scope.spawn(context.sleep({5000})));
            SC_TEST_EXPECT(loop.runNoWait());
            ExecutionStopSource stop;
            ExecutionOutcome    first, second;
            first.loop = &loop;
            auto left  = ex::connect(scope.join(), ExecutionReceiver{first, stop.get_token()});
            auto right = ex::connect(scope.join(), ExecutionReceiver{second, {}});
            ex::start(left);
            ex::start(right);
            SC_TEST_EXPECT(second.errors == 1 and
                           second.error.isError(AsyncExecutionResultCategory, AsyncExecutionError::AlreadyRunning));
            Thread canceller;
            SC_TEST_EXPECT(canceller.start([&stop](Thread&) { stop.request_stop(); }));
            SC_TEST_EXPECT(runBounded(loop));
            SC_TEST_EXPECT(canceller.join());
            SC_TEST_EXPECT(first.values == 1 and first.errors == 0 and first.stopped == 0);
            SC_TEST_EXPECT(scope.getSummary().stopped == 1);
        }
        {
            alignas(AsyncExecutionScope) char scopeStorage[sizeof(AsyncExecutionScope)];
            auto*                             scope = reinterpret_cast<AsyncExecutionScope*>(scopeStorage);
            placementNew(*scope, context, AsyncExecutionScopeAllocator::heap());
            SC_TEST_EXPECT(scope->spawn(context.sleep({1})));
            ExecutionOutcome outcome;
            outcome.loop = &loop;
            using Join   = decltype(ex::connect(scope->join(), ExecutionReceiver{outcome, {}}));
            alignas(Join) char joinStorage[sizeof(Join)];
            auto*              join = reinterpret_cast<Join*>(joinStorage);
            placementNew(*join, *scope, ExecutionReceiver{outcome, {}});
            struct Destroy
            {
                AsyncExecutionScope* scope;
                Join*                join;
                bool                 done = false;
            } destroy{scope, join};
            outcome.onValue = [&destroy]()
            {
                destroy.join->~Join();
                destroy.scope->~AsyncExecutionScope();
                destroy.done = true;
            };
            ex::start(*join);
            SC_TEST_EXPECT(runBounded(loop));
            SC_TEST_EXPECT(destroy.done and outcome.values == 1 and outcome.errors == 0);
        }
        SC_TEST_EXPECT(app.close());
    }

    void scopeDestructionChild()
    {
#if SC_PLATFORM_WINDOWS
        ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        ::SetUnhandledExceptionFilter(scopeAssertionExit);
#else
        ::signal(SIGTRAP, [](int) { ::_Exit(77); });
        ::signal(SIGILL, [](int) { ::_Exit(77); });
#endif
        AsyncExecution app;
        if (not app.create())
            ::_Exit(75);
        {
            AsyncExecutionScope scope(app.getContext(), AsyncExecutionScopeAllocator::heap());
            if (not scope.spawn(app.sleep({5000})))
                ::_Exit(76);
        }
        ::_Exit(78); // Reaching here would mean active destruction was silently accepted.
    }

    void scopeDestruction()
    {
        StringSpan arguments[] = {report.executableFile.view(), "--quiet",        "--test",
                                  "AsyncExecutionTest",         "--test-section", "scope active destruction child"};
        Process    child;
        SC_TEST_EXPECT(child.launch(arguments, Process::StdOut::Ignore(), Process::StdIn(), Process::StdErr::Ignore()));
        struct Wait
        {
            Process&     child;
            Result       status{false};
            Atomic<bool> done = false;
        } wait{child};
        Thread waiter;
        SC_TEST_EXPECT(waiter.start(
            [&wait](Thread&)
            {
                wait.status = wait.child.waitForExitSync();
                wait.done.store(true);
            }));
        for (int attempt = 0; attempt < 5000 and not wait.done.load(); ++attempt)
            Thread::Sleep(1);
        const bool exited = wait.done.load();
        if (not exited)
        {
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(::TerminateProcess(child.handle, 124) != FALSE);
#else
            SC_TEST_EXPECT(::kill(child.processID.pid, SIGKILL) == 0);
#endif
        }
        SC_TEST_EXPECT(waiter.join());
        SC_TEST_EXPECT(exited and wait.status and child.getExitStatus() == 77);
    }

#endif // SC_ASYNC_EXECUTION_SCOPES_AVAILABLE

    void owningClose()
    {
        AsyncExecution app;
        SC_TEST_EXPECT(app.close());
        SC_TEST_EXPECT(app.run(app.sleep({0})).isError(AsyncResultCategory, AsyncError::NotInitialized));
        SC_TEST_EXPECT(app.create(0).isError(ThreadingResultCategory, ThreadingError::InvalidWorkerThreadCount));
        SC_TEST_EXPECT(app.close());
        SC_TEST_EXPECT(app.create());
        SC_TEST_EXPECT(app.create().isError(AsyncResultCategory, AsyncError::AlreadyInitialized));

        Result foreignClose{true};
        Thread foreign;
        SC_TEST_EXPECT(foreign.start([&](Thread&) { foreignClose = app.close(); }));
        SC_TEST_EXPECT(foreign.join());
        SC_TEST_EXPECT(foreignClose.isError(AsyncResultCategory, AsyncError::InvalidState));

        auto&               context = app.getContext();
        auto&               loop    = context.getEventLoop();
        ExecutionStopSource stop;
        ExecutionOutcome    outcome;
        outcome.loop   = &loop;
        auto operation = ex::connect(context.sleep({5000}), ExecutionReceiver{outcome, stop.get_token()});
        ex::start(operation);
        SC_TEST_EXPECT(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
        SC_TEST_EXPECT(loop.runNoWait());
        SC_TEST_EXPECT(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
        stop.request_stop();
        SC_TEST_EXPECT(runBounded(loop));
        SC_TEST_EXPECT(outcome.stopped == 1 and outcome.values == 0 and outcome.errors == 0);

        Result reentrantClose{true};
        SC_TEST_EXPECT(app.run(app.sleep({1}) | ex::then([&]() noexcept { reentrantClose = app.close(); })));
        SC_TEST_EXPECT(reentrantClose.isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));

        // A worker notification is active after its embedded task was released. Rejection must not stop the loop.
        struct PoolGate
        {
            EventObject entered, release, finished;
        } gate;
        ExecutionOutcome poolOutcome;
        poolOutcome.onValue = [&gate]() { gate.finished.signal(); };
        auto graph          = ex::schedule(app.getPoolScheduler()) | ex::then(
                                                                [&gate]() noexcept
                                                                {
                                                                    gate.entered.signal();
                                                                    gate.release.wait();
                                                                });
        auto pooled = ex::connect(move(graph), ExecutionReceiver{poolOutcome, {}});
        ex::start(pooled);
        gate.entered.wait();
        SC_TEST_EXPECT(app.close().isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain));
        SC_TEST_EXPECT(app.run(app.sleep({1})));
        gate.release.signal();
        gate.finished.wait();
        Result closed{false};
        for (int attempt = 0; attempt < 2000; ++attempt)
        {
            closed = app.close();
            if (closed or not closed.isError(AsyncResultCategory, AsyncError::ActiveRequestsRemain))
                break;
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(closed);
        SC_TEST_EXPECT(poolOutcome.values == 1 and poolOutcome.errors == 0 and poolOutcome.stopped == 0);
        SC_TEST_EXPECT(app.close());
        SC_TEST_EXPECT(app.run(app.sleep({0})).isError(AsyncResultCategory, AsyncError::NotInitialized));
        SC_TEST_EXPECT(app.create());
        SC_TEST_EXPECT(app.run(app.sleep({1})));
        SC_TEST_EXPECT(app.close());
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
        Atomic<bool> boundBodyOnWorker     = false;
        bool         boundDeliveredOnOwner = false;
        auto         ownerBound =
            app.onPool(
                [&]() noexcept
                {
                    boundBodyOnWorker.store(Thread::CurrentThreadID() != ownerThread);
                    return 9;
                }) |
            ex::then([&](int value) noexcept
                     { boundDeliveredOnOwner = value == 9 and Thread::CurrentThreadID() == ownerThread; });
        // Even a caller whose environment/starting agent is a pool worker receives onPool's result on the owner.
        SC_TEST_EXPECT(app.run(ex::on(app.getPoolScheduler(), move(ownerBound))));
        SC_TEST_EXPECT(boundBodyOnWorker.load() and boundDeliveredOnOwner);

        ExecutionStopSource poolStop;
        poolStop.request_stop();
        SC_TEST_EXPECT(app.run(ex::schedule(app.getPoolScheduler()), poolStop.get_token())
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        // A codec selects its own error channel before a dependent transport is even constructed.
        const Result encodingFailure = Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge);
        bool         attemptedTCP    = false;
        auto         failedCodec     = ex::just() |
                           ex::on(app.getPoolScheduler(),
                                  tryThen([encodingFailure]() noexcept -> AsyncExecutionValueResult<Span<const char>>
                                          { return {encodingFailure, {}}; })) |
                           ex::let_value(
                               [&](auto& bytes) noexcept
                               {
                                   attemptedTCP = true;
                                   return app.sendTCP("invalid-IP", report.mapPort(19092), bytes);
                               });
        SC_TEST_EXPECT(app.run(move(failedCodec)).isError(encodingFailure.category(), encodingFailure.errorValue()));
        SC_TEST_EXPECT(not attemptedTCP);
        const char* vectors[]  = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
        const char* expected[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
        for (size_t i = 0; i < 7; ++i)
        {
            char output[12] = {};
            auto encoded    = base64Encode({vectors[i], strlen(vectors[i])}, output);
            SC_TEST_EXPECT(encoded.status);
            SC_TEST_EXPECT(encoded.value.sizeInBytes() == strlen(expected[i]));
            SC_TEST_EXPECT(encoded.value.empty() or
                           memcmp(encoded.value.data(), expected[i], encoded.value.sizeInBytes()) == 0);
        }
        char             output[4] = {'?', '?', '?', '?'};
        const char       old[]     = "old";
        Span<const char> written{old, 3};
        SC_TEST_EXPECT(not base64Encode({"foobar", 6}, output, written));
        SC_TEST_EXPECT(written.data() == old and written.sizeInBytes() == 3);
        SC_TEST_EXPECT(memcmp(output, "????", 4) == 0);
        // Impossible input size must fail arithmetic checks before accessing either span.
        SC_TEST_EXPECT(not base64Encode({nullptr, size_t(-1)}, output, written));
        SC_TEST_EXPECT(memcmp(output, "????", 4) == 0);
        // An external synchronous sender must not leave a stale loop interrupt behind.
        SC_TEST_EXPECT(app.run(ex::just()));
        SC_TEST_EXPECT(app.run(app.sleep({1})));
        char input[] = "hello";
        char destination[8];
        memset(destination, '?', sizeof(destination));
        StringPath path;
        SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory.view(), "AsyncExecutionSpanTest.txt"}));
        SC_TEST_EXPECT(app.run(app.writeFile(path.view(), {input, 5})));
        bool checked = false;
        auto job     = app.readFile(path.view(), destination) |
                   ex::let_value(
                       [&](Span<const char>& bytes)
                       {
                           return app.onPool(
                               [&, bytes]() noexcept
                               {
                                   for (size_t i = 0; i < bytes.sizeInBytes(); ++i)
                                       destination[i] = static_cast<char>(destination[i] - 'a' + 'A');
                                   return Span<const char>{destination, bytes.sizeInBytes()};
                               });
                       }) |
                   ex::then([&](Span<const char> bytes) noexcept
                            { checked = bytes.sizeInBytes() == 5 and memcmp(bytes.data(), "HELLO", 5) == 0; });
        SC_TEST_EXPECT(app.run(move(job)));
        SC_TEST_EXPECT(checked and destination[5] == '?');
        Span<const char> result{"unchanged", 9};
        SC_TEST_EXPECT(app.runInto(app.readFile(path.view(), {destination, 2}), result)
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge));
        SC_TEST_EXPECT(result.sizeInBytes() == 9 and memcmp(destination, "he", 2) == 0);
        SC_TEST_EXPECT(app.run(app.readFile(path.view(), {}))
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge));
        SC_TEST_EXPECT(app.runInto(app.readFile(path.view(), {destination, 5}), result));
        SC_TEST_EXPECT(result.sizeInBytes() == 5 and memcmp(result.data(), "hello", 5) == 0);
        SC_TEST_EXPECT(app.run(app.writeFile(path.view(), {})));
        SC_TEST_EXPECT(app.runInto(app.readFile(path.view(), {}), result));
        SC_TEST_EXPECT(result.empty());
        SC_TEST_EXPECT(FileSystem::Operations::removeFile(path.view()));
        SC_TEST_EXPECT(not app.run(app.readFile(path.view(), destination)));
        ExecutionStopSource source;
        source.request_stop();
        SC_TEST_EXPECT(app.run(app.sleep({60000}), source.get_token())
                           .isError(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        SC_TEST_EXPECT(app.run(app.sleep({1})));
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
