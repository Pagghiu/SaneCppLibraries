// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Internal/AsyncExecutionProtocol.h"

#if SC_ASYNC_EXECUTION_AVAILABLE
#include "../Async/Async.h"
#include "../Common/PlacementNew.h"
#include "../Strings/Path.h"
#include "../Threading/ThreadPool.h"
#include "AsyncExecutionError.h"
#include "Internal/AsyncExecutionAllocation.h"

namespace SC
{
struct AsyncExecutionScheduler;
template <typename Setup>
struct AsyncExecutionSender;
namespace detail
{
struct AsyncExecutionCommand
{
    AsyncExecutionCommand* next                      = nullptr;
    void (*execute)(AsyncExecutionCommand&) noexcept = nullptr;
    void* operation                                  = nullptr;
    bool  queued                                     = false;
};
template <typename Setup, typename Receiver>
struct AsyncExecutionOperation;
template <typename Receiver>
struct AsyncExecutionPoolOperation;
} // namespace detail

/// Owns an intrusive command dispatcher; borrows its event loop. Create and close on the loop thread.
/// The dispatcher keeps loop.run() alive. A terminal receiver normally calls loop.interrupt().
struct AsyncExecutionContext
{
    explicit AsyncExecutionContext(AsyncEventLoop& loop) : loop(loop) {}
    ~AsyncExecutionContext() { SC_ASYNC_ASSERT_DEBUG(not accepting.load()); }
    AsyncExecutionContext(const AsyncExecutionContext&) = delete;
    AsyncExecutionContext(AsyncExecutionContext&&)      = delete;

    Result create()
    {
        if (accepting.load())
            return Result::Error(AsyncResultCategory, AsyncError::AlreadyInitialized);
        ownerThread     = Thread::CurrentThreadID();
        wakeUp.callback = [this](AsyncLoopWakeUp::Result& result)
        {
            result.reactivateRequest(true);
            dispatching = true;
            while (auto* command = pop())
                command->execute(*command); // Commands may destroy themselves. Do not access them afterward.
            dispatching = false;
        };
        SC_TRY(wakeUp.start(loop));
        accepting.store(true);
        return Result(true);
    }

    Result close()
    {
        if (not accepting.load())
            return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
        if (Thread::CurrentThreadID() != ownerThread or dispatching or delivering.load() != 0 or
            outstanding.load() != 0)
            return Result::Error(AsyncResultCategory, AsyncError::ActiveRequestsRemain);
        // Callers must stop submitting and join producer threads before closing the context.
        accepting.store(false);
        SC_TRY(wakeUp.stop(loop));
        SC_TRY(loop.runNoWait());
        SC_ASYNC_ASSERT_DEBUG(wakeUp.isFree());
        return Result(true);
    }

    AsyncExecutionScheduler getScheduler() noexcept;
    auto                    sleep(TimeMs duration);
    auto                    send(const SocketDescriptor& socket, Span<const char> data);
    auto                    receive(const SocketDescriptor& socket, Span<char> buffer);
    auto                    sendAll(const SocketDescriptor& socket, Span<const char> data);
    auto fileRead(const FileDescriptor& file, Span<char> buffer, ThreadPool& pool, uint64_t offset = 0);
    auto fileWrite(const FileDescriptor& file, Span<const char> data, ThreadPool& pool, uint64_t offset = 0);
    auto work(ThreadPool& pool, Function<Result()> procedure);

    AsyncEventLoop& loop;

  private:
    friend struct AsyncExecution;
    template <typename Setup, typename Receiver>
    friend struct detail::AsyncExecutionOperation;
    template <typename Receiver>
    friend struct detail::AsyncExecutionPoolOperation;
    Mutex                          mutex;
    AsyncLoopWakeUp                wakeUp;
    detail::AsyncExecutionCommand* head        = nullptr;
    detail::AsyncExecutionCommand* tail        = nullptr;
    Atomic<int32_t>                outstanding = 0;
    Atomic<int32_t>                delivering  = 0;
    Atomic<bool>                   accepting   = false;
    uint64_t                       ownerThread = 0;
    bool                           dispatching = false;

    Result post(detail::AsyncExecutionCommand& command) noexcept
    {
        mutex.lock();
        if (not accepting.load())
        {
            mutex.unlock();
            return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
        }
        if (command.queued)
        {
            mutex.unlock();
            return Result(true);
        }
        command.next   = nullptr;
        command.queued = true;
        auto* previous = tail;
        if (tail)
            tail->next = &command;
        else
            head = &command;
        tail                = &command;
        const Result status = wakeUp.wakeUp(loop);
        if (not status)
        {
            if (previous)
                previous->next = nullptr;
            else
                head = nullptr;
            tail           = previous;
            command.queued = false;
        }
        mutex.unlock();
        return status;
    }

    void remove(detail::AsyncExecutionCommand& command) noexcept
    {
        mutex.lock();
        if (command.queued)
        {
            detail::AsyncExecutionCommand* previous = nullptr;
            auto*                          current  = head;
            while (current != &command)
            {
                previous = current;
                current  = current->next;
            }
            if (previous)
                previous->next = command.next;
            else
                head = command.next;
            if (tail == &command)
                tail = previous;
            command.next   = nullptr;
            command.queued = false;
        }
        mutex.unlock();
    }

    detail::AsyncExecutionCommand* pop() noexcept
    {
        mutex.lock();
        auto* command = head;
        if (command)
        {
            head = command->next;
            if (not head)
                tail = nullptr;
            command->next   = nullptr;
            command->queued = false;
        }
        mutex.unlock();
        return command;
    }
};

struct AsyncExecutionScheduler
{
    using scheduler_concept        = AsyncExecutionProtocol::scheduler_tag;
    AsyncExecutionContext* context = nullptr;

    auto schedule() const noexcept;
    bool operator==(const AsyncExecutionScheduler& other) const noexcept { return context == other.context; }
    auto query(AsyncExecutionProtocol::get_forward_progress_guarantee_t) const noexcept
    {
        return AsyncExecutionProtocol::forward_progress_guarantee::weakly_parallel;
    }
};

struct AsyncExecutionEnvironment
{
    AsyncExecutionScheduler scheduler;
    auto query(AsyncExecutionProtocol::get_completion_scheduler_t<AsyncExecutionProtocol::set_value_t>) const noexcept
    {
        return scheduler;
    }
};

/// Successful scheduling completes on a worker. A separate ticket survives receiver-driven operation destruction.
struct AsyncExecutionPoolScheduler;
namespace detail
{
template <typename Receiver>
struct AsyncExecutionPoolOperation
{
    using operation_state_concept = AsyncExecutionProtocol::operation_state_tag;
    AsyncExecutionContext& context;
    ThreadPool&            pool;
    Receiver               receiver;
    struct Ticket
    {
        ThreadPoolTask               task;
        AsyncExecutionCommand        cleanup;
        AsyncExecutionPoolOperation* operation;
        AsyncExecutionContext*       context;
        ThreadPool*                  pool;
    };
    AsyncExecutionPoolOperation(AsyncExecutionContext& context, ThreadPool& pool, Receiver receiver)
        : context(context), pool(pool), receiver(move(receiver))
    {}
    AsyncExecutionPoolOperation(const AsyncExecutionPoolOperation&) = delete;
    void start() noexcept
    {
        auto* ticket = static_cast<Ticket*>(AsyncExecutionReallocate(nullptr, sizeof(Ticket)));
        if (not ticket)
        {
            AsyncExecutionProtocol::set_error(
                move(receiver), Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::OutOfMemory));
            return;
        }
        placementNew(*ticket);
        ticket->operation         = this;
        ticket->context           = &context;
        ticket->pool              = &pool;
        ticket->cleanup.operation = ticket;
        ticket->cleanup.execute   = [](AsyncExecutionCommand& command) noexcept
        {
            auto* ticket = static_cast<Ticket*>(command.operation);
            // ThreadPool accesses its Task after the callback returns. Reclaim only after that access finishes.
            SC_ASYNC_TRUST_RESULT(ticket->pool->waitForTask(ticket->task));
            ticket->context->outstanding.fetch_sub(1);
            ticket->~Ticket();
            AsyncExecutionRelease(ticket);
        };
        ticket->task.function = [ticket]()
        {
            auto receiver = move(ticket->operation->receiver);
            auto token    = AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver));
            // Queue reclamation first: run() cannot observe terminal completion before this ticket is joined.
            SC_ASYNC_TRUST_RESULT(ticket->context->post(ticket->cleanup));
            if (token.stop_requested())
                AsyncExecutionProtocol::set_stopped(move(receiver));
            else
                AsyncExecutionProtocol::set_value(move(receiver));
        };
        context.outstanding.fetch_add(1);
        const Result queued = pool.queueTask(ticket->task);
        if (not queued)
        {
            context.outstanding.fetch_sub(1);
            ticket->~Ticket();
            AsyncExecutionRelease(ticket);
            AsyncExecutionProtocol::set_error(move(receiver), queued);
        }
    }
};
} // namespace detail
struct AsyncExecutionPoolSender
{
    using sender_concept = AsyncExecutionProtocol::sender_tag;
    AsyncExecutionContext* context;
    ThreadPool*            pool;
    struct Environment
    {
        AsyncExecutionContext*      context;
        ThreadPool*                 pool;
        AsyncExecutionPoolScheduler query(
            AsyncExecutionProtocol::get_completion_scheduler_t<AsyncExecutionProtocol::set_value_t>) const noexcept;
    };
    auto get_env() const noexcept { return Environment{context, pool}; }
    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        return AsyncExecutionProtocol::completion_signatures<AsyncExecutionProtocol::set_value_t(),
                                                             AsyncExecutionProtocol::set_error_t(Result),
                                                             AsyncExecutionProtocol::set_stopped_t()>{};
    }
    template <typename Receiver>
    auto connect(Receiver receiver) const noexcept
    {
        return detail::AsyncExecutionPoolOperation<Receiver>(*context, *pool, move(receiver));
    }
};
struct AsyncExecutionPoolScheduler
{
    using scheduler_concept        = AsyncExecutionProtocol::scheduler_tag;
    AsyncExecutionContext* context = nullptr;
    ThreadPool*            pool    = nullptr;
    auto                   schedule() const noexcept { return AsyncExecutionPoolSender{context, pool}; }
    bool                   operator==(const AsyncExecutionPoolScheduler& other) const noexcept
    {
        return context == other.context and pool == other.pool;
    }
    auto query(AsyncExecutionProtocol::get_forward_progress_guarantee_t) const noexcept
    {
        return AsyncExecutionProtocol::forward_progress_guarantee::weakly_parallel;
    }
};
inline AsyncExecutionPoolScheduler AsyncExecutionPoolSender::Environment::query(
    AsyncExecutionProtocol::get_completion_scheduler_t<AsyncExecutionProtocol::set_value_t>) const noexcept
{
    return {context, pool};
}

struct AsyncExecutionReadResult
{
    Span<char> data;
    bool       endOfFile = false;
};

namespace detail
{
struct AsyncExecutionTimer
{
    using Request = AsyncLoopTimeout;
    using Value   = AsyncCompletionData;
    TimeMs duration;
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
    {
        return request.start(loop, duration);
    }
    static Value value(Request::Result&) { return {}; }
};

struct AsyncExecutionSend
{
    using Request = AsyncSocketSend;
    using Value   = size_t;
    const SocketDescriptor& socket;
    Span<const char>        data;
    Result                  submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
    {
        return request.start(loop, socket, data);
    }
    static Value value(Request::Result& result) { return result.completionData.numBytes; }
};

/// Retries partial successful sends without changing the caller's buffer lifetime.
struct AsyncExecutionSendAll
{
    using Request = AsyncSocketSend;
    using Value   = size_t;
    const SocketDescriptor& socket;
    Span<const char>        remaining;
    size_t                  transferred = 0;

    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
    {
        // Bound each individual send; completion advances over short writes and chunk boundaries alike.
        Span<const char> chunk;
        SC_TRY(remaining.sliceStartLength(0, remaining.sizeInBytes() > 16384 ? 16384 : remaining.sizeInBytes(), chunk));
        return request.start(loop, socket, chunk);
    }
    Result advance(Request::Result& result)
    {
        const size_t count = result.completionData.numBytes;
        if (count == 0 or count > remaining.sizeInBytes())
            return Result::Error(AsyncResultCategory, AsyncError::SocketSendIncomplete);
        transferred += count;
        return Result(remaining.sliceStartLength(count, remaining.sizeInBytes() - count, remaining));
    }
    bool  hasRemaining() const { return not remaining.empty(); }
    bool  completesImmediately() const { return remaining.empty(); }
    Value immediateValue() const { return transferred; }
    Value value(Request::Result&) const { return transferred; }
};

struct AsyncExecutionReceive
{
    using Request = AsyncSocketReceive;
    using Value   = AsyncExecutionReadResult;
    const SocketDescriptor& socket;
    Span<char>              buffer;
    Result                  submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
    {
        return request.start(loop, socket, buffer);
    }
    static Value value(Request::Result& result)
    {
        Span<char> data;
        (void)result.get(data);
        return {data, result.isEnded()};
    }
};

struct AsyncExecutionFileRead
{
    using Request = AsyncFileRead;
    using Value   = AsyncExecutionReadResult;
    const FileDescriptor& file;
    Span<char>            buffer;
    ThreadPool&           pool;
    uint64_t              offset;
    Result                submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
    {
        request.setOffset(offset);
        SC_TRY(request.executeOn(task, pool));
        return request.start(loop, file, buffer);
    }
    static Value value(Request::Result& result)
    {
        Span<char> data;
        (void)result.get(data);
        return {data, result.isEnded()};
    }
};

struct AsyncExecutionFileWrite
{
    using Request = AsyncFileWrite;
    using Value   = size_t;
    const FileDescriptor& file;
    Span<const char>      data;
    ThreadPool&           pool;
    uint64_t              offset;
    Result                submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const
    {
        request.setOffset(offset);
        SC_TRY(request.executeOn(task, pool));
        return request.start(loop, file, data);
    }
    static Value value(Request::Result& result) { return result.completionData.numBytes; }
};

struct AsyncExecutionWork
{
    using Request = AsyncLoopWork;
    using Value   = AsyncCompletionData;
    ThreadPool&        pool;
    Function<Result()> work;
    Result             submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const
    {
        request.work = work;
        SC_TRY(request.setThreadPool(pool));
        return request.start(loop);
    }
    static Value value(Request::Result&) { return {}; }
};

template <typename Setup, typename Receiver>
struct AsyncExecutionOperation
{
    using operation_state_concept = AsyncExecutionProtocol::operation_state_tag;

    struct Request : Setup::Request
    {
        bool usesThreadPool() { return this->getTask() != nullptr; }
    };

    AsyncExecutionContext& context;

    Setup    setup;
    Receiver receiver;

    Request                      request;
    AsyncTaskSequence            task;
    AsyncExecutionCommand        command;
    AsyncExecutionCommand        cancelCommand;
    Function<void(AsyncResult&)> afterStopped;

    struct StopRequest
    {
        AsyncExecutionOperation* operation;
        void                     operator()() const noexcept
        {
            // The receiver and stop-callback storage remain alive until the owner thread unregisters this callback.
            SC_ASYNC_TRUST_RESULT(operation->context.post(operation->cancelCommand));
        }
    };
    using StopToken    = decltype(AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver)));
    using StopCallback = typename StopToken::template callback_type<StopRequest>;
    AlignedStorage<sizeof(StopCallback), alignof(StopCallback)> stopStorage;
    bool                                                        stopRegistered = false;

    StopCallback* stopCallback() noexcept { return reinterpret_cast<StopCallback*>(&stopStorage); }

    void prepareCompletion() noexcept
    {
        // Never hold the queue mutex while unregistering: the callback may be posting from another thread.
        if (stopRegistered)
        {
            stopRegistered = false;
            stopCallback()->~StopCallback();
        }
        context.remove(cancelCommand);
        context.outstanding.fetch_sub(1);
    }

    struct Delivery
    {
        AsyncExecutionContext& context;
        explicit Delivery(AsyncExecutionContext& context) : context(context) { context.delivering.fetch_add(1); }
        ~Delivery() { context.delivering.fetch_sub(1); }
    };

    void finishStopped() noexcept
    {
        prepareCompletion();
        Delivery delivery(context);
        AsyncExecutionProtocol::set_stopped(move(receiver));
    }

    void cancelOnLoop() noexcept
    {
        if (request.isFree())
        {
            finishStopped();
            return;
        }
        if (request.isCancelling())
            return;
        // A worker cannot be preempted. Keep its normal completion path active: its completion may already
        // be queued while this cancellation command is dispatched. complete() observes the stop token only
        // after Async has joined the worker and released its task storage.
        if (request.usesThreadPool())
            return;
        afterStopped = [this](AsyncResult&) { finishStopped(); };
        // Async suppresses normal completion after stop. Notify only after its quiescence callback.
        SC_ASYNC_TRUST_RESULT(request.stop(context.loop, &afterStopped));
    }

    AsyncExecutionOperation(AsyncExecutionContext& context, Setup setup, Receiver receiver)
        : context(context), setup(move(setup)), receiver(move(receiver))
    {}
    AsyncExecutionOperation(const AsyncExecutionOperation&) = delete;
    AsyncExecutionOperation(AsyncExecutionOperation&&)      = delete;

    bool stopRequested() const noexcept
    {
        return AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver)).stop_requested();
    }

    void start() noexcept
    {
        command.operation = this;
        command.execute   = [](AsyncExecutionCommand& command) noexcept
        { static_cast<AsyncExecutionOperation*>(command.operation)->startOnLoop(); };
        context.outstanding.fetch_add(1);
        const Result status = context.post(command);
        if (not status)
        {
            context.outstanding.fetch_sub(1);
            Delivery delivery(context);
            AsyncExecutionProtocol::set_error(move(receiver), status);
        }
    }

    void startOnLoop() noexcept
    {
        cancelCommand.operation = this;
        cancelCommand.execute   = [](AsyncExecutionCommand& command) noexcept
        { static_cast<AsyncExecutionOperation*>(command.operation)->cancelOnLoop(); };
        // Register on the loop thread, after publication through the start command. An already-stopped token may
        // enqueue cancellation synchronously, but cannot dispatch it before this owner-thread call returns.
        auto token = AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver));
        placementNew(*stopCallback(), token, StopRequest{this});
        stopRegistered = true;
        if (stopRequested())
        {
            finishStopped();
            return;
        }
        if constexpr (requires { setup.completesImmediately(); })
        {
            if (setup.completesImmediately())
            {
                auto value = setup.immediateValue();
                prepareCompletion();
                Delivery delivery(context);
                AsyncExecutionProtocol::set_value(move(receiver), move(value));
                return;
            }
        }
        request.callback    = [this](typename Request::Result& result) { complete(result); };
        const Result result = setup.submit(request, context.loop, task);
        if (not result)
        {
            prepareCompletion();
            Delivery delivery(context);
            AsyncExecutionProtocol::set_error(move(receiver), result);
        }
    }

    void complete(typename Request::Result& result) noexcept
    {
        // Completion may destroy this operation. Never access any member after signaling the receiver.
        Result     status  = result.isValid();
        const bool stopped = stopRequested();
        if constexpr (requires { setup.advance(result); })
        {
            if (status and not stopped)
            {
                status = setup.advance(result);
                if (status and setup.hasRemaining())
                {
                    status = setup.submit(request, context.loop, task);
                    if (status)
                        return;
                }
            }
        }
        prepareCompletion();
        Delivery delivery(context);
        if (not status)
            AsyncExecutionProtocol::set_error(move(receiver), status);
        else if (stopped)
            AsyncExecutionProtocol::set_stopped(move(receiver));
        else
        {
            auto value = setup.value(result);
            if constexpr (TypeTraits::IsSame<typename Setup::Value, AsyncCompletionData>::value)
                AsyncExecutionProtocol::set_value(move(receiver));
            else
                AsyncExecutionProtocol::set_value(move(receiver), move(value));
        }
    }
};
} // namespace detail

template <typename Setup>
struct AsyncExecutionSender
{
    using sender_concept = AsyncExecutionProtocol::sender_tag;
    using Value          = typename Setup::Value;
    using ValueSignature = typename TypeTraits::Conditional<TypeTraits::IsSame<Value, AsyncCompletionData>::value,
                                                            AsyncExecutionProtocol::set_value_t(),
                                                            AsyncExecutionProtocol::set_value_t(Value)>::type;
    using CompletionSignatures =
        AsyncExecutionProtocol::completion_signatures<ValueSignature, AsyncExecutionProtocol::set_error_t(Result),
                                                      AsyncExecutionProtocol::set_stopped_t()>;

    // N5046 [exec.getcomplsigs] uses a static member function, rather than the older type-alias protocol.
    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        return CompletionSignatures{};
    }

    AsyncExecutionContext& context;
    Setup                  setup;

    AsyncExecutionSender(AsyncExecutionContext& context, Setup setup) noexcept : context(context), setup(move(setup)) {}
    AsyncExecutionSender(AsyncExecutionSender&& other) noexcept : context(other.context), setup(move(other.setup)) {}
    AsyncExecutionSender(const AsyncExecutionSender&) = default;

    auto get_env() const noexcept { return AsyncExecutionEnvironment{context.getScheduler()}; }

    template <typename Receiver>
    auto connect(Receiver receiver) && noexcept
    {
        return detail::AsyncExecutionOperation<Setup, Receiver>(context, move(setup), move(receiver));
    }

    template <typename Receiver>
    auto connect(Receiver receiver) const& noexcept
        requires(std::is_copy_constructible_v<Setup>)
    {
        return detail::AsyncExecutionOperation<Setup, Receiver>(context, setup, move(receiver));
    }
};

inline AsyncExecutionScheduler AsyncExecutionContext::getScheduler() noexcept { return {this}; }
inline auto                    AsyncExecutionScheduler::schedule() const noexcept
{
    return AsyncExecutionSender<detail::AsyncExecutionTimer>{*context, {{0}}};
}
inline auto AsyncExecutionContext::sleep(TimeMs duration)
{
    return AsyncExecutionSender<detail::AsyncExecutionTimer>{*this, {duration}};
}
inline auto AsyncExecutionContext::send(const SocketDescriptor& socket, Span<const char> data)
{
    return AsyncExecutionSender<detail::AsyncExecutionSend>{*this, {socket, data}};
}
inline auto AsyncExecutionContext::sendAll(const SocketDescriptor& socket, Span<const char> data)
{
    return AsyncExecutionSender<detail::AsyncExecutionSendAll>{*this, {socket, data}};
}
inline auto AsyncExecutionContext::receive(const SocketDescriptor& socket, Span<char> buffer)
{
    return AsyncExecutionSender<detail::AsyncExecutionReceive>{*this, {socket, buffer}};
}
inline auto AsyncExecutionContext::fileRead(const FileDescriptor& file, Span<char> buffer, ThreadPool& pool,
                                            uint64_t offset)
{
    return AsyncExecutionSender<detail::AsyncExecutionFileRead>{*this, {file, buffer, pool, offset}};
}
inline auto AsyncExecutionContext::fileWrite(const FileDescriptor& file, Span<const char> data, ThreadPool& pool,
                                             uint64_t offset)
{
    return AsyncExecutionSender<detail::AsyncExecutionFileWrite>{*this, {file, data, pool, offset}};
}
inline auto AsyncExecutionContext::work(ThreadPool& pool, Function<Result()> procedure)
{
    return AsyncExecutionSender<detail::AsyncExecutionWork>{*this, {pool, move(procedure)}};
}
/// Move-only owned bytes. Allocation failures are Result errors; no C++ exceptions are used.
struct AsyncExecutionBuffer
{
    AsyncExecutionBuffer()                                       = default;
    AsyncExecutionBuffer(const AsyncExecutionBuffer&)            = delete;
    AsyncExecutionBuffer& operator=(const AsyncExecutionBuffer&) = delete;
    AsyncExecutionBuffer(AsyncExecutionBuffer&& other) noexcept { *this = move(other); }
    AsyncExecutionBuffer& operator=(AsyncExecutionBuffer&& other) noexcept
    {
        if (this != &other)
        {
            detail::AsyncExecutionRelease(memory);
            memory       = other.memory;
            length       = other.length;
            capacity     = other.capacity;
            other.memory = nullptr;
            other.length = other.capacity = 0;
        }
        return *this;
    }
    ~AsyncExecutionBuffer() { detail::AsyncExecutionRelease(memory); }
    Result resize(size_t bytes)
    {
        if (bytes > capacity)
        {
            auto* replacement = static_cast<char*>(detail::AsyncExecutionReallocate(memory, bytes));
            if (not replacement)
                return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::OutOfMemory);
            memory   = replacement;
            capacity = bytes;
        }
        length = bytes;
        return Result(true);
    }
    Span<char>       bytes() noexcept { return {memory, length}; }
    Span<const char> bytes() const noexcept { return {memory, length}; }
    size_t           size() const noexcept { return length; }

  private:
    char*  memory   = nullptr;
    size_t length   = 0;
    size_t capacity = 0;
};

/// Owning connection value with a nothrow transfer contract for standard sender composition.
struct AsyncExecutionConnection
{
    SocketDescriptor socket;
    explicit AsyncExecutionConnection(SocketDescriptor socket) noexcept : socket(move(socket)) {}
    AsyncExecutionConnection(AsyncExecutionConnection&& other) noexcept : socket(move(other.socket)) {}
    AsyncExecutionConnection(const AsyncExecutionConnection&) = delete;
};

/// Checked output of a fallible synchronous buffer transform. then passes this as a value;
/// the next owning I/O sender explicitly converts a failed status into set_error(Result).
struct AsyncExecutionBufferResult
{
    Result               status{true};
    AsyncExecutionBuffer data;
};

namespace detail
{
struct AsyncExecutionOwnedRead
{
    using Request = AsyncLoopWork;
    using Value   = AsyncExecutionBuffer;
    ThreadPool&          pool;
    StringPath           path;
    Result               initialization{true};
    AsyncExecutionBuffer data;
    size_t               maximumBytes;

    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
    {
        SC_TRY(initialization);
        SC_TRY(request.setThreadPool(pool));
        request.work = [this]() -> Result
        {
            FileDescriptor file;
            FileOpen       options;
            options.mode = FileOpen::Read;
            SC_TRY(file.open(path.view(), options));
            size_t used = 0;
            while (true)
            {
                if (used == maximumBytes)
                {
                    char       probe;
                    Span<char> read;
                    SC_TRY(file.read({&probe, 1}, read));
                    if (not read.empty())
                        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge);
                    break;
                }
                const size_t available = maximumBytes - used;
                const size_t count     = available > 32768 ? 32768 : available;
                SC_TRY(data.resize(used + count));
                Span<char> read;
                SC_TRY(file.read({data.bytes().data() + used, count}, read));
                used += read.sizeInBytes();
                if (read.empty())
                    break;
            }
            return data.resize(used);
        };
        return request.start(loop);
    }
    Value value(Request::Result&) { return move(data); }
};

template <typename Procedure>
struct AsyncExecutionOwnedTransform
{
    using Request = AsyncLoopWork;
    using Value   = AsyncExecutionBuffer;
    ThreadPool&          pool;
    AsyncExecutionBuffer data;
    Procedure            procedure;
    Result               submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
    {
        SC_TRY(request.setThreadPool(pool));
        request.work = [this]() -> Result
        {
            if constexpr (std::is_void_v<decltype(procedure(data))>)
            {
                procedure(data);
                return Result(true);
            }
            else
                return procedure(data);
        };
        return request.start(loop);
    }
    Value value(Request::Result&) { return move(data); }
};

template <typename Procedure>
struct AsyncExecutionOwnedCompute
{
    using Request  = AsyncLoopWork;
    using Returned = decltype(std::declval<Procedure&>()());
    using Value = std::conditional_t<std::is_void_v<Returned> or std::is_same_v<Returned, Result>, AsyncCompletionData,
                                     std::remove_cvref_t<Returned>>;
    ThreadPool&                                   pool;
    Procedure                                     procedure;
    AlignedStorage<sizeof(Value), alignof(Value)> output;
    bool                                          produced = false;

    AsyncExecutionOwnedCompute(ThreadPool& pool, Procedure procedure) : pool(pool), procedure(move(procedure)) {}
    AsyncExecutionOwnedCompute(AsyncExecutionOwnedCompute&& other) noexcept
        : pool(other.pool), procedure(move(other.procedure))
    {
        SC_ASYNC_ASSERT_DEBUG(not other.produced);
    }
    AsyncExecutionOwnedCompute(const AsyncExecutionOwnedCompute&) = delete;
    ~AsyncExecutionOwnedCompute()
    {
        if (produced)
            get()->~Value();
    }
    Value* get() noexcept { return reinterpret_cast<Value*>(&output); }
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
    {
        SC_TRY(request.setThreadPool(pool));
        request.work = [this]() -> Result
        {
            if constexpr (std::is_same_v<Returned, Result>)
                return procedure();
            else if constexpr (std::is_void_v<Returned>)
            {
                procedure();
                return Result(true);
            }
            else
            {
                placementNew(*get(), procedure());
                produced = true;
                return Result(true);
            }
        };
        return request.start(loop);
    }
    Value value(Request::Result&)
    {
        if constexpr (std::is_void_v<Returned> or std::is_same_v<Returned, Result>)
            return {};
        else
            return move(*get());
    }
};

struct AsyncExecutionOwnedWrite
{
    using Request = AsyncLoopWork;
    using Value   = AsyncCompletionData;
    ThreadPool&          pool;
    StringPath           path;
    Result               initialization{true};
    AsyncExecutionBuffer data;
    Result               submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
    {
        SC_TRY(initialization);
        SC_TRY(request.setThreadPool(pool));
        request.work = [this]() -> Result
        {
            FileDescriptor file;
            FileOpen       options;
            options.mode = FileOpen::Write;
            SC_TRY(file.open(path.view(), options));
            return file.write(data.bytes());
        };
        return request.start(loop);
    }
    static Value value(Request::Result&) { return {}; }
};

inline Result AsyncExecutionResolvePath(StringSpan path, StringPath& output)
{
    StringPath directory;
    const bool copied = Path::isAbsolute(path, Path::AsNative)
                            ? output.assign(path)
                            : Path::join(output, {FileSystem::Operations::getCurrentWorkingDirectory(directory), path});
    return copied ? Result(true) : Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::InvalidPath);
}

struct AsyncExecutionOwnedConnect
{
    using Request = AsyncSocketConnect;
    using Value   = AsyncExecutionConnection;
    SocketIPAddress  address;
    Result           initialization{true};
    SocketDescriptor socket;
    Result           submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&)
    {
        SC_TRY(initialization);
        SC_TRY(socket.create(address.getAddressFamily()));
        SC_TRY(socket.setBlocking(false));
        SC_TRY(loop.associateExternallyCreatedSocket(socket));
        return request.start(loop, socket, address);
    }
    Value value(Request::Result&) { return Value(move(socket)); }
};

struct AsyncExecutionOwnedSend
{
    using Request = AsyncSocketSend;
    using Value   = size_t;
    AsyncExecutionBuffer  data;
    AsyncExecutionSendAll send;
    AsyncExecutionOwnedSend(const SocketDescriptor& socket, AsyncExecutionBuffer data)
        : data(move(data)), send{socket, this->data.bytes()}
    {}
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task)
    {
        return send.submit(request, loop, task);
    }
    Result advance(Request::Result& result) { return send.advance(result); }
    bool   hasRemaining() const { return send.hasRemaining(); }
    bool   completesImmediately() const { return send.completesImmediately(); }
    Value  immediateValue() const { return send.immediateValue(); }
    Value  value(Request::Result& result) { return send.value(result); }
};
} // namespace detail

/// Owning convenience runtime. It hides request, receiver, loop and pool plumbing from application pipelines.
/// readFile and onPool execute blocking work on the owned pool. TCP connect/send use native Async requests.
struct AsyncExecution
{
    AsyncExecution() : context(loop) {}
    ~AsyncExecution()
    {
        if (initialized)
        {
            SC_ASYNC_ASSERT_RELEASE(not running);
            SC_ASYNC_TRUST_RESULT(context.close());
            SC_ASYNC_TRUST_RESULT(loop.close());
            SC_ASYNC_TRUST_RESULT(pool.destroy());
        }
    }
    AsyncExecution(const AsyncExecution&) = delete;
    AsyncExecution(AsyncExecution&&)      = delete;

    Result create(size_t workers = 1)
    {
        if (initialized)
            return Result::Error(AsyncResultCategory, AsyncError::AlreadyInitialized);
        SocketNetworking::initNetworking();
        SC_TRY(loop.create());
        const Result createdPool = pool.create(workers);
        if (not createdPool)
        {
            (void)loop.close();
            return createdPool;
        }
        const Result createdContext = context.create();
        if (not createdContext)
        {
            (void)pool.destroy();
            (void)loop.close();
            return createdContext;
        }
        initialized = true;
        return Result(true);
    }

    AsyncExecutionScheduler     getScheduler() noexcept { return context.getScheduler(); }
    AsyncExecutionPoolScheduler getPoolScheduler() noexcept { return {&context, &pool}; }
    auto                        sleep(TimeMs duration) { return context.sleep(duration); }

    auto readFile(StringSpan path, size_t maximumBytes = 64 * 1024 * 1024)
    {
        detail::AsyncExecutionOwnedRead setup{pool, {}, Result(true), {}, maximumBytes};
        setup.initialization = detail::AsyncExecutionResolvePath(path, setup.path);
        return AsyncExecutionSender<detail::AsyncExecutionOwnedRead>{context, move(setup)};
    }

    auto writeFile(StringSpan path, AsyncExecutionBuffer data)
    {
        detail::AsyncExecutionOwnedWrite setup{pool, {}, Result(true), move(data)};
        setup.initialization = detail::AsyncExecutionResolvePath(path, setup.path);
        return AsyncExecutionSender<detail::AsyncExecutionOwnedWrite>{context, move(setup)};
    }

    template <typename Procedure>
    auto onPool(AsyncExecutionBuffer data, Procedure procedure)
    {
        using Setup = detail::AsyncExecutionOwnedTransform<Procedure>;
        return AsyncExecutionSender<Setup>{context, {pool, move(data), move(procedure)}};
    }

    template <typename Procedure>
    auto onPool(Procedure procedure)
    {
        using Setup = detail::AsyncExecutionOwnedCompute<Procedure>;
        return AsyncExecutionSender<Setup>{context, Setup(pool, move(procedure))};
    }

    auto connectTCP(StringSpan host, uint16_t port)
    {
        detail::AsyncExecutionOwnedConnect setup;
        setup.initialization = setup.address.fromAddressPort(host, port);
        return AsyncExecutionSender<detail::AsyncExecutionOwnedConnect>{context, move(setup)};
    }

    auto sendTCP(StringSpan host, uint16_t port, AsyncExecutionBuffer data, Result status = Result(true))
    {
        detail::AsyncExecutionOwnedConnect setup;
        setup.initialization = status;
        if (status)
            setup.initialization = setup.address.fromAddressPort(host, port);
        return AsyncExecutionProtocol::let_value(
            AsyncExecutionSender<detail::AsyncExecutionOwnedConnect>{context, move(setup)},
            [this, data = move(data)](AsyncExecutionConnection& connection) mutable noexcept
            {
                return AsyncExecutionSender<detail::AsyncExecutionOwnedSend>{context, {connection.socket, move(data)}};
            });
    }

    auto sendTCP(StringSpan host, uint16_t port, AsyncExecutionBufferResult encoded)
    {
        return sendTCP(host, port, move(encoded.data), encoded.status);
    }

    template <typename Sender>
    Result run(Sender&& sender)
    {
        struct EmptyReceiver
        {
        };
        auto token = AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(EmptyReceiver{}));
        return run(forward<Sender>(sender), token);
    }

    template <typename Sender, typename StopToken>
    Result run(Sender&& sender, StopToken token)
    {
        if (not initialized)
            return Result::Error(AsyncResultCategory, AsyncError::NotInitialized);
        if (running)
            return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::AlreadyRunning);
        running = true;
        RunState state{context};
        auto operation = AsyncExecutionProtocol::connect(forward<Sender>(sender), RunReceiver<StopToken>{state, token});
        AsyncExecutionProtocol::start(operation);
        // SC sender completions occur on the owner thread. Synchronous external senders can finish in start().
        if (not state.completed)
        {
            state.driving = true;
            // A failed dispatcher cannot safely release arbitrary connected operation storage.
            SC_ASYNC_TRUST_RESULT(loop.run());
            state.driving = false;
        }
        running = false;
        SC_ASYNC_ASSERT_RELEASE(state.completed);
        return state.status;
    }

  private:
    AsyncEventLoop        loop;
    ThreadPool            pool;
    AsyncExecutionContext context;
    bool                  initialized = false;
    bool                  running     = false;
    struct RunState
    {
        AsyncExecutionContext&        context;
        Result                        status{true};
        bool                          completed = false;
        bool                          driving   = false;
        detail::AsyncExecutionCommand completion;
        Result                        pending{true};

        void finishOnLoop(Result result) noexcept
        {
            status    = result;
            completed = true;
            if (driving)
                context.loop.interrupt();
        }
        void finish(Result result) noexcept
        {
            if (Thread::CurrentThreadID() == context.ownerThread)
                finishOnLoop(result);
            else
            {
                pending              = result;
                completion.operation = this;
                completion.execute   = [](detail::AsyncExecutionCommand& command) noexcept
                {
                    auto& state = *static_cast<RunState*>(command.operation);
                    state.finishOnLoop(state.pending);
                };
                SC_ASYNC_TRUST_RESULT(context.post(completion));
            }
        }
    };
    template <typename Token>
    struct RunReceiver
    {
        using receiver_concept = AsyncExecutionProtocol::receiver_tag;
        RunState& state;
        Token     token;
        struct Environment
        {
            Token token;
            auto  query(AsyncExecutionProtocol::get_stop_token_t) const noexcept { return token; }
        };
        auto get_env() const noexcept { return Environment{token}; }
        template <typename... Values>
        void set_value(Values&&...) && noexcept
        {
            state.finish(Result(true));
        }
        void set_error(Result result) && noexcept { state.finish(result); }
        void set_stopped() && noexcept
        {
            state.finish(Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        }
    };
};

} // namespace SC
#endif
