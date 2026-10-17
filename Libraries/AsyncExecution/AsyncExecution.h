// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "AsyncExecutionExport.h"
#include "Internal/AsyncExecutionProtocol.h"

#if SC_ASYNC_EXECUTION_AVAILABLE
#include "../Async/Async.h"
#include "../Common/PlacementNew.h"
#include "../Common/StringPath.h"
#include "../Threading/ThreadPool.h"
#include "AsyncExecutionError.h"

namespace SC
{
struct AsyncExecutionScheduler;
struct AsyncExecutionConnection;
struct AsyncExecutionReadResult;
struct AsyncExecutionLineResult;
template <typename Setup>
struct AsyncExecutionSender;
namespace detail
{
struct AsyncExecutionTimer;
struct AsyncExecutionAccept;
struct AsyncExecutionConnect;
struct AsyncExecutionSend;
struct AsyncExecutionSendBuffers;
struct AsyncExecutionSendAllBuffers;
struct AsyncExecutionReceiveExact;
struct AsyncExecutionReceiveLine;
struct AsyncExecutionSendAll;
struct AsyncExecutionReceive;
struct AsyncExecutionFileRead;
struct AsyncExecutionFileWrite;
struct AsyncExecutionWork;
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
struct AsyncExecutionPoolTicket;
} // namespace detail

/// Owns an intrusive command dispatcher; borrows its event loop. Create and close on the loop thread.
/// The dispatcher keeps loop.run() alive. A terminal receiver normally calls loop.interrupt().
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionContext
{
    explicit AsyncExecutionContext(AsyncEventLoop& loop);
    ~AsyncExecutionContext();
    AsyncExecutionContext(const AsyncExecutionContext&) = delete;
    AsyncExecutionContext(AsyncExecutionContext&&)      = delete;

    Result create();

    Result close();

    AsyncExecutionScheduler getScheduler() noexcept;

    /// Drive this same loop directly for GUI/frame integration; operation storage must survive completion.
    AsyncEventLoop& getEventLoop() noexcept { return loop; }

    /// Consume one graph on the owner thread, driving the borrowed loop until terminal completion.
    template <typename Sender>
    Result run(Sender&& sender);

    template <typename Sender, typename StopToken>
    Result run(Sender&& sender, StopToken token);

    /// Assign successful values to existing output objects, in completion argument order.
    /// The consumer assigns only on success; assignments must be noexcept.
    template <typename Sender, typename... Outputs>
    Result runInto(Sender&& sender, Outputs&... outputs);

    template <typename Sender, typename StopToken, typename... Outputs>
    Result runIntoWithStop(Sender&& sender, StopToken token, Outputs&... outputs);

    AsyncExecutionSender<detail::AsyncExecutionTimer> sleep(TimeMs duration);

    /// Accept owns the returned connection; connect borrows an already-associated nonblocking socket.
    AsyncExecutionSender<detail::AsyncExecutionAccept>  accept(const SocketDescriptor& listener);
    AsyncExecutionSender<detail::AsyncExecutionConnect> connect(const SocketDescriptor& socket,
                                                                SocketIPAddress         address);
    AsyncExecutionSender<detail::AsyncExecutionConnect> connect(const SocketDescriptor& socket, SocketAddress address);

    AsyncExecutionSender<detail::AsyncExecutionSend>        send(const SocketDescriptor& socket, Span<const char> data);
    AsyncExecutionSender<detail::AsyncExecutionSendBuffers> send(const SocketDescriptor& socket,
                                                                 Span<Span<const char>>  data);

    /// Optional progress reports observed completed chunks, including before an error or stopped completion.
    AsyncExecutionSender<detail::AsyncExecutionSendAll> sendAll(const SocketDescriptor& socket, Span<const char> data,
                                                                size_t* progress = nullptr);
    AsyncExecutionSender<detail::AsyncExecutionSendAllBuffers> sendAll(const SocketDescriptor& socket,
                                                                       Span<Span<const char>>  data,
                                                                       size_t*                 progress = nullptr);

    AsyncExecutionSender<detail::AsyncExecutionReceive>      receive(const SocketDescriptor& socket, Span<char> buffer);
    AsyncExecutionSender<detail::AsyncExecutionReceiveExact> receiveExact(const SocketDescriptor&   socket,
                                                                          Span<char>                buffer,
                                                                          AsyncExecutionReadResult* progress = nullptr);
    AsyncExecutionSender<detail::AsyncExecutionReceiveLine>  receiveLine(const SocketDescriptor&   socket,
                                                                         Span<char>                buffer,
                                                                         AsyncExecutionLineResult* progress = nullptr);

    AsyncExecutionSender<detail::AsyncExecutionFileRead>  fileRead(const FileDescriptor& file, Span<char> buffer,
                                                                   ThreadPool& pool, uint64_t offset = 0);
    AsyncExecutionSender<detail::AsyncExecutionFileWrite> fileWrite(const FileDescriptor& file, Span<const char> data,
                                                                    ThreadPool& pool, uint64_t offset = 0);

    AsyncExecutionSender<detail::AsyncExecutionWork> work(ThreadPool& pool, Function<Result()> procedure);

    AsyncEventLoop& loop;

  private:
    friend struct AsyncExecution;
    friend struct detail::AsyncExecutionPoolTicket;
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

    bool running = false;
    struct RunState
    {
        AsyncExecutionContext&        context;
        Result                        status{true};
        bool                          completed = false;
        bool                          driving   = false;
        detail::AsyncExecutionCommand completion;
        Result                        pending{true};

        void finishOnLoop(Result result) noexcept;
        void finish(Result result) noexcept;
    };
    template <typename Token, typename Output>
    struct RunReceiver;

    Result beginRun();
    Result finishRun(RunState& state);

    Result post(detail::AsyncExecutionCommand& command) noexcept;

    void remove(detail::AsyncExecutionCommand& command) noexcept;

    detail::AsyncExecutionCommand* pop() noexcept;
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionScheduler
{
    using scheduler_concept        = AsyncExecutionProtocol::scheduler_tag;
    AsyncExecutionContext* context = nullptr;

    AsyncExecutionSender<detail::AsyncExecutionTimer> schedule() const noexcept;
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
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionPoolTicket
{
    using Notify = void (*)(void*) noexcept;
    static Result submit(AsyncExecutionContext& context, ThreadPool& pool, void* operation, Notify notify) noexcept;
};
template <typename Receiver>
struct AsyncExecutionPoolOperation
{
    using operation_state_concept = AsyncExecutionProtocol::operation_state_tag;
    AsyncExecutionContext& context;
    ThreadPool&            pool;
    Receiver               receiver;
    AsyncExecutionPoolOperation(AsyncExecutionContext& context, ThreadPool& pool, Receiver receiver)
        : context(context), pool(pool), receiver(move(receiver))
    {}
    AsyncExecutionPoolOperation(const AsyncExecutionPoolOperation&) = delete;
    void start() noexcept
    {
        const Result queued = AsyncExecutionPoolTicket::submit(
            context, pool, this,
            [](void* operation) noexcept
            {
                auto receiver = move(static_cast<AsyncExecutionPoolOperation*>(operation)->receiver);
                auto token    = AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver));
                if (token.stop_requested())
                    AsyncExecutionProtocol::set_stopped(move(receiver));
                else
                    AsyncExecutionProtocol::set_value(move(receiver));
            });
        if (not queued)
            AsyncExecutionProtocol::set_error(move(receiver), queued);
    }
};
} // namespace detail
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionPoolSender
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
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionPoolScheduler
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

struct AsyncExecutionLineResult
{
    Span<char> line;
    bool       disconnected = false;
    bool       lineComplete = false;
};

namespace detail
{
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionAccept
{
    using Request = AsyncSocketAccept;
    using Value   = AsyncExecutionConnection;
    const SocketDescriptor& listener;
    SocketDescriptor        client;

    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    Result advance(Request::Result& result);
    bool   hasRemaining() const { return false; }
    Value  value(Request::Result&);
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionConnect
{
    using Request = AsyncSocketConnect;
    using Value   = AsyncCompletionData;
    const SocketDescriptor& socket;
    SocketAddress           address;

    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value value(Request::Result&) { return {}; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionSendBuffers
{
    using Request = AsyncSocketSend;
    using Value   = size_t;
    const SocketDescriptor& socket;
    Span<Span<const char>>  data;

    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value value(Request::Result& result) { return result.completionData.numBytes; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionSendAllBuffers
{
    using Request = AsyncSocketSend;
    using Value   = size_t;
    const SocketDescriptor& socket;
    Span<Span<const char>>  data;
    size_t*                 progress = nullptr;

    size_t index       = 0;
    size_t offset      = 0;
    size_t transferred = 0;
    Result initialization{true};

    void   initialize();
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    Result advance(Request::Result& result);
    Result advanceBytes(size_t count);
    void   findNextBuffer();
    bool   hasRemaining() const { return index < data.sizeInElements(); }
    bool   completesImmediately() const { return initialization and not hasRemaining(); }
    Value  immediateValue() const { return transferred; }
    Value  value(Request::Result&) const { return transferred; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionReceiveExact
{
    using Request = AsyncSocketReceive;
    using Value   = AsyncExecutionReadResult;
    const SocketDescriptor& socket;
    Span<char>              buffer;
    Value*                  progress = nullptr;

    size_t received     = 0;
    bool   disconnected = false;

    void   initialize();
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    Result advance(Request::Result& result);
    Result advanceBytes(size_t count, bool ended);
    bool   hasRemaining() const { return received < buffer.sizeInBytes(); }
    bool   completesImmediately() const { return buffer.empty(); }
    Value  immediateValue() { return {{buffer.data(), 0}, false}; }
    Value  value(Request::Result&) { return {{buffer.data(), received}, disconnected}; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionReceiveLine
{
    using Request = AsyncSocketReceive;
    using Value   = AsyncExecutionLineResult;
    const SocketDescriptor& socket;
    Span<char>              buffer;
    Value*                  progress = nullptr;

    size_t received     = 0;
    char   currentByte  = 0;
    bool   disconnected = false;
    bool   lineComplete = false;

    void   initialize();
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&);
    Result advance(Request::Result& result);
    Result advanceByte(size_t count, bool ended);
    bool   hasRemaining() const { return not disconnected and not lineComplete; }
    Value  value(Request::Result&);
    Value  currentValue();
    void   updateProgress();
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionTimer
{
    using Request = AsyncLoopTimeout;
    using Value   = AsyncCompletionData;
    TimeMs       duration;
    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value value(Request::Result&) { return {}; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionSend
{
    using Request = AsyncSocketSend;
    using Value   = size_t;
    const SocketDescriptor& socket;
    Span<const char>        data;
    Result                  submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value            value(Request::Result& result) { return result.completionData.numBytes; }
};

/// Retries partial successful sends without changing the caller's buffer lifetime.
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionSendAll
{
    using Request = AsyncSocketSend;
    using Value   = size_t;
    const SocketDescriptor& socket;
    Span<const char>        remaining;
    size_t                  transferred = 0;
    size_t*                 progress    = nullptr;

    void initialize()
    {
        if (progress)
            *progress = transferred;
    }
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    Result advance(Request::Result& result);
    Result advanceBytes(size_t count);
    bool   hasRemaining() const { return not remaining.empty(); }
    bool   completesImmediately() const { return remaining.empty(); }
    Value  immediateValue() const { return transferred; }
    Value  value(Request::Result&) const { return transferred; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionReceive
{
    using Request = AsyncSocketReceive;
    using Value   = AsyncExecutionReadResult;
    const SocketDescriptor& socket;
    Span<char>              buffer;
    Result                  submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value            value(Request::Result& result);
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFileRead
{
    using Request = AsyncFileRead;
    using Value   = AsyncExecutionReadResult;
    const FileDescriptor& file;
    Span<char>            buffer;
    ThreadPool&           pool;
    uint64_t              offset;
    Result                submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const;
    static Value          value(Request::Result& result);
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFileWrite
{
    using Request = AsyncFileWrite;
    using Value   = size_t;
    const FileDescriptor& file;
    Span<const char>      data;
    ThreadPool&           pool;
    uint64_t              offset;
    Result                submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const;
    static Value          value(Request::Result& result) { return result.completionData.numBytes; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionWork
{
    using Request = AsyncLoopWork;
    using Value   = AsyncCompletionData;
    ThreadPool&        pool;
    Function<Result()> work;
    Result             submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value       value(Request::Result&) { return {}; }
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
        if constexpr (requires { setup.initialize(); })
            setup.initialize();
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

/// Move-only owned bytes. Allocation failures are Result errors; no C++ exceptions are used.
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionBuffer
{
    AsyncExecutionBuffer()                                       = default;
    AsyncExecutionBuffer(const AsyncExecutionBuffer&)            = delete;
    AsyncExecutionBuffer& operator=(const AsyncExecutionBuffer&) = delete;
    AsyncExecutionBuffer(AsyncExecutionBuffer&& other) noexcept;
    AsyncExecutionBuffer& operator=(AsyncExecutionBuffer&& other) noexcept;
    ~AsyncExecutionBuffer();
    Result           resize(size_t bytes);
    Span<char>       bytes() noexcept { return {memory, length}; }
    Span<const char> bytes() const noexcept { return {memory, length}; }
    size_t           size() const noexcept { return length; }

  private:
    char*  memory   = nullptr;
    size_t length   = 0;
    size_t capacity = 0;
};

/// Owning connection value with a nothrow transfer contract for standard sender composition.
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionConnection
{
    SocketDescriptor socket;
    AsyncExecutionConnection() = default;
    AsyncExecutionConnection& operator=(AsyncExecutionConnection&& other) noexcept;
    explicit AsyncExecutionConnection(SocketDescriptor socket) noexcept : socket(move(socket)) {}
    AsyncExecutionConnection(AsyncExecutionConnection&& other) noexcept : socket(move(other.socket)) {}
    AsyncExecutionConnection(const AsyncExecutionConnection&) = delete;
};

/// Checked output of an SC fallible transform. tryThen sends value only when status succeeds.
template <typename Value>
struct AsyncExecutionValueResult
{
    static_assert(std::is_object_v<Value>, "AsyncExecutionValueResult requires an owned object value");
    Result status{true};
    Value  value{};
};

namespace detail
{
template <typename Returned>
struct AsyncExecutionTryTraits
{
    static_assert(std::is_same_v<Returned, Result>, "tryThen expects Result or AsyncExecutionValueResult<T>");
    using Value          = void;
    using ValueSignature = AsyncExecutionProtocol::set_value_t();
    static Result status(Result result) noexcept { return result; }
};

template <typename Output>
struct AsyncExecutionTryTraits<AsyncExecutionValueResult<Output>>
{
    static_assert(std::is_nothrow_move_constructible_v<Output>, "tryThen output must be nothrow move constructible");
    using Value          = Output;
    using ValueSignature = AsyncExecutionProtocol::set_value_t(Output);
    static Result status(const AsyncExecutionValueResult<Output>& result) noexcept { return result.status; }
};

template <typename Compute>
struct AsyncExecutionTrySender
{
    using sender_concept = AsyncExecutionProtocol::sender_tag;
    using Returned       = std::remove_cvref_t<decltype(std::declval<Compute&>()())>;
    using Traits         = AsyncExecutionTryTraits<Returned>;
    using Value          = typename Traits::Value;
    using ValueSignature = typename Traits::ValueSignature;
    Compute compute;

    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        return AsyncExecutionProtocol::completion_signatures<
            ValueSignature, AsyncExecutionProtocol::set_error_t(Result), AsyncExecutionProtocol::set_stopped_t()>{};
    }

    template <typename Receiver>
    struct Operation
    {
        using operation_state_concept = AsyncExecutionProtocol::operation_state_tag;
        Compute  compute;
        Receiver receiver;

        void start() noexcept
        {
            auto token = AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver));
            if (token.stop_requested())
            {
                AsyncExecutionProtocol::set_stopped(move(receiver));
                return;
            }
            auto         result = compute();
            const Result status = Traits::status(result);
            // Preserve a completed transform's error; a successful CPU function observes stop after returning.
            if (not status)
                AsyncExecutionProtocol::set_error(move(receiver), status);
            else if (token.stop_requested())
                AsyncExecutionProtocol::set_stopped(move(receiver));
            else if constexpr (std::is_void_v<Value>)
                AsyncExecutionProtocol::set_value(move(receiver));
            else
                AsyncExecutionProtocol::set_value(move(receiver), move(result.value));
            // Terminal delivery may destroy this operation; no member accesses follow it.
        }
    };

    template <typename Receiver>
    auto connect(Receiver receiver) && noexcept
    {
        return Operation<Receiver>{move(compute), move(receiver)};
    }
};

template <typename Procedure>
struct AsyncExecutionTryTransform
{
    Procedure procedure;

    template <typename... Values>
    auto operator()(Values&... values) noexcept
    {
        static_assert(std::is_nothrow_invocable_v<Procedure&, Values&&...>, "tryThen callable must be noexcept");
        static_assert(not std::is_reference_v<decltype(procedure(move(values)...))>,
                      "tryThen callable must return its checked result by value");
        auto compute = [procedure = move(procedure), &values...]() mutable noexcept
        { return procedure(move(values)...); };
        return AsyncExecutionTrySender<decltype(compute)>{move(compute)};
    }
};
} // namespace detail

/// SC extension: unwrap Result or AsyncExecutionValueResult<T> into the execution completion channels.
/// Inputs are consumed as rvalues; the callable must be noexcept. Standard let_value retains input lifetimes.
template <typename Procedure>
auto tryThen(Procedure procedure)
{
    return AsyncExecutionProtocol::let_value(detail::AsyncExecutionTryTransform<Procedure>{move(procedure)});
}

template <typename Sender, typename Procedure>
auto tryThen(Sender&& sender, Procedure procedure)
{
    return AsyncExecutionProtocol::let_value(forward<Sender>(sender),
                                             detail::AsyncExecutionTryTransform<Procedure>{move(procedure)});
}

/// Legacy checked-buffer carrier accepted by sendTCP for compatibility.
/// Prefer AsyncExecutionValueResult<AsyncExecutionBuffer> with tryThen for transport-independent error handling.
struct AsyncExecutionBufferResult
{
    Result               status{true};
    AsyncExecutionBuffer data;
};

namespace detail
{
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionOwnedRead
{
    using Request = AsyncLoopWork;
    using Value   = AsyncExecutionBuffer;
    ThreadPool&          pool;
    StringPath           path;
    Result               initialization{true};
    AsyncExecutionBuffer data;
    size_t               maximumBytes;

    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&);
    Value  value(Request::Result&) { return move(data); }
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

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionOwnedWrite
{
    using Request = AsyncLoopWork;
    using Value   = AsyncCompletionData;
    ThreadPool&          pool;
    StringPath           path;
    Result               initialization{true};
    AsyncExecutionBuffer data;
    Result               submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&);
    static Value         value(Request::Result&) { return {}; }
};

SC_ASYNC_EXECUTION_EXPORT Result AsyncExecutionResolvePath(StringSpan path, StringPath& output);

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionOwnedConnect
{
    using Request = AsyncSocketConnect;
    using Value   = AsyncExecutionConnection;
    SocketIPAddress  address;
    Result           initialization{true};
    SocketDescriptor socket;
    Result           submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&);
    Value            value(Request::Result&) { return Value(move(socket)); }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionOwnedSend
{
    using Request = AsyncSocketSend;
    using Value   = size_t;
    AsyncExecutionBuffer  data;
    AsyncExecutionSendAll send;
    AsyncExecutionOwnedSend(const SocketDescriptor& socket, AsyncExecutionBuffer data)
        : data(move(data)), send{socket, this->data.bytes()}
    {}
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task);
    Result advance(Request::Result& result) { return send.advance(result); }
    bool   hasRemaining() const { return send.hasRemaining(); }
    bool   completesImmediately() const { return send.completesImmediately(); }
    Value  immediateValue() const { return send.immediateValue(); }
    Value  value(Request::Result& result) { return send.value(result); }
};
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionSendConnected
{
    AsyncExecutionContext* context;
    AsyncExecutionBuffer   data;

    AsyncExecutionSender<AsyncExecutionOwnedSend> operator()(AsyncExecutionConnection& connection) noexcept;
};
using AsyncExecutionTCPSender = decltype(AsyncExecutionProtocol::let_value(
    std::declval<AsyncExecutionSender<AsyncExecutionOwnedConnect>>(), std::declval<AsyncExecutionSendConnected>()));
} // namespace detail

/// Owning convenience runtime. It hides request, receiver, loop and pool plumbing from application pipelines.
/// readFile and onPool execute blocking work on the owned pool. TCP connect/send use native Async requests.
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecution
{
    AsyncExecution();
    ~AsyncExecution();
    AsyncExecution(const AsyncExecution&) = delete;
    AsyncExecution(AsyncExecution&&)      = delete;

    Result create(size_t workers = 1);

    AsyncExecutionScheduler     getScheduler() noexcept { return context.getScheduler(); }
    AsyncExecutionPoolScheduler getPoolScheduler() noexcept { return {&context, &pool}; }

    AsyncExecutionSender<detail::AsyncExecutionTimer> sleep(TimeMs duration);

    AsyncExecutionSender<detail::AsyncExecutionOwnedRead> readFile(StringSpan path,
                                                                   size_t     maximumBytes = 64 * 1024 * 1024);

    AsyncExecutionSender<detail::AsyncExecutionOwnedWrite> writeFile(StringSpan path, AsyncExecutionBuffer data);

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

    AsyncExecutionSender<detail::AsyncExecutionOwnedConnect> connectTCP(StringSpan host, uint16_t port);

    detail::AsyncExecutionTCPSender sendTCP(StringSpan host, uint16_t port, AsyncExecutionBuffer data,
                                            Result status = Result(true));

    detail::AsyncExecutionTCPSender sendTCP(StringSpan host, uint16_t port, AsyncExecutionBufferResult encoded);

    AsyncExecutionContext& getContext() noexcept { return context; }

    template <typename Sender>
    Result run(Sender&& sender)
    {
        return context.run(forward<Sender>(sender));
    }

    template <typename Sender, typename StopToken>
    Result run(Sender&& sender, StopToken token)
    {
        return context.run(forward<Sender>(sender), token);
    }

    template <typename Sender, typename... Outputs>
    Result runInto(Sender&& sender, Outputs&... outputs)
    {
        return context.runInto(forward<Sender>(sender), outputs...);
    }

    template <typename Sender, typename StopToken, typename... Outputs>
    Result runIntoWithStop(Sender&& sender, StopToken token, Outputs&... outputs)
    {
        return context.runIntoWithStop(forward<Sender>(sender), token, outputs...);
    }

  private:
    AsyncEventLoop        loop;
    ThreadPool            pool;
    AsyncExecutionContext context;
    bool                  initialized = false;
};

namespace detail
{
struct AsyncExecutionDiscard
{
    template <typename... Values>
    void store(Values&&...) noexcept
    {}
};

template <typename... Outputs>
struct AsyncExecutionOutputs;

template <>
struct AsyncExecutionOutputs<>
{
    void store() noexcept {}
};

template <typename Head, typename... Tail>
struct AsyncExecutionOutputs<Head, Tail...>
{
    Head&                          head;
    AsyncExecutionOutputs<Tail...> tail;

    AsyncExecutionOutputs(Head& head, Tail&... tail) noexcept : head(head), tail(tail...) {}

    template <typename Value, typename... Values>
    void store(Value&& value, Values&&... values) noexcept
    {
        static_assert(std::is_nothrow_assignable_v<Head&, Value>, "runInto output assignment must be noexcept");
        head = forward<Value>(value);
        tail.store(forward<Values>(values)...);
    }
};
} // namespace detail

template <typename Token, typename Output>
struct AsyncExecutionContext::RunReceiver
{
    using receiver_concept = AsyncExecutionProtocol::receiver_tag;
    RunState& state;
    Token     token;
    Output    output;
    struct Environment
    {
        Token                   token;
        AsyncExecutionScheduler scheduler;

        auto query(AsyncExecutionProtocol::get_stop_token_t) const noexcept { return token; }
        auto query(AsyncExecutionProtocol::get_scheduler_t) const noexcept { return scheduler; }
        auto query(AsyncExecutionProtocol::get_start_scheduler_t) const noexcept { return scheduler; }
    };
    auto get_env() const noexcept { return Environment{token, state.context.getScheduler()}; }
    template <typename... Values>
    void set_value(Values&&... values) && noexcept
    {
        output.store(forward<Values>(values)...);
        state.finish(Result(true));
    }
    void set_error(Result result) && noexcept { state.finish(result); }
    void set_stopped() && noexcept
    {
        state.finish(Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
    }
};

template <typename Sender>
Result AsyncExecutionContext::run(Sender&& sender)
{
    auto token =
        AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(detail::AsyncExecutionDiscard{}));
    return run(forward<Sender>(sender), token);
}

template <typename Sender, typename StopToken>
Result AsyncExecutionContext::run(Sender&& sender, StopToken token)
{
    SC_TRY(beginRun());
    RunState state{*this};
    auto     operation = AsyncExecutionProtocol::connect(
        forward<Sender>(sender), RunReceiver<StopToken, detail::AsyncExecutionDiscard>{state, token, {}});
    AsyncExecutionProtocol::start(operation);
    return finishRun(state);
}

template <typename Sender, typename... Outputs>
Result AsyncExecutionContext::runInto(Sender&& sender, Outputs&... outputs)
{
    auto token =
        AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(detail::AsyncExecutionDiscard{}));
    return runIntoWithStop(forward<Sender>(sender), token, outputs...);
}

template <typename Sender, typename StopToken, typename... Outputs>
Result AsyncExecutionContext::runIntoWithStop(Sender&& sender, StopToken token, Outputs&... outputs)
{
    SC_TRY(beginRun());
    RunState state{*this};
    using Output   = detail::AsyncExecutionOutputs<Outputs...>;
    auto operation = AsyncExecutionProtocol::connect(forward<Sender>(sender),
                                                     RunReceiver<StopToken, Output>{state, token, Output(outputs...)});
    AsyncExecutionProtocol::start(operation);
    return finishRun(state);
}

} // namespace SC
#endif
