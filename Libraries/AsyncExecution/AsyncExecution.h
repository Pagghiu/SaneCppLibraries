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
struct AsyncExecutionNotification;
struct AsyncExecutionNotificationSender;
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
struct AsyncExecutionScope;
struct AsyncExecutionScopeJoinSender;
#endif
struct AsyncExecutionConnection;
struct AsyncExecutionReadResult;
struct AsyncExecutionLineResult;
struct AsyncExecutionSocketReceiveFromResult;
struct AsyncExecutionProcessExitResult;
struct AsyncExecutionSignalResult;
struct AsyncExecutionFileSendResult;
template <typename Setup>
struct AsyncExecutionSender;
/// Native request addressing is delegated unless useOffset is true. Use explicit offsets for portable file ranges.
struct AsyncExecutionFileOptions
{
    ThreadPool*         threadPool     = nullptr;
    uint64_t            offset         = 0;
    bool                useOffset      = false;
    AsyncThreadPoolMode threadPoolMode = AsyncThreadPoolMode::NativePreferred;
};
using AsyncExecutionFileReadOptions  = AsyncExecutionFileOptions;
using AsyncExecutionFileWriteOptions = AsyncExecutionFileOptions;

/// A single explicit file range. NativePreferred uses the backend file-send mechanism; force a pool explicitly.
struct AsyncExecutionFileSendOptions
{
    int64_t offset   = 0;
    size_t  length   = 0;
    size_t  pipeSize = 0;

    ThreadPool* threadPool = nullptr;

    AsyncThreadPoolMode threadPoolMode = AsyncThreadPoolMode::NativePreferred;
};

namespace detail
{
template <typename Receiver>
struct AsyncExecutionNotificationOperation;
struct AsyncExecutionNotificationWaiter
{
    void* operation                 = nullptr;
    void (*deliver)(void*) noexcept = nullptr;
};
struct AsyncExecutionTimer;
struct AsyncExecutionFilePoll;
struct AsyncExecutionAccept;
struct AsyncExecutionConnect;
struct AsyncExecutionSend;
struct AsyncExecutionSendTo;
struct AsyncExecutionSendToBuffers;
struct AsyncExecutionReceiveFrom;
struct AsyncExecutionProcessExit;
struct AsyncExecutionSignal;
struct AsyncExecutionSendBuffers;
struct AsyncExecutionSendAllBuffers;
struct AsyncExecutionReceiveExact;
struct AsyncExecutionReceiveLine;
struct AsyncExecutionSendAll;
struct AsyncExecutionReceive;
struct AsyncExecutionFileRead;
struct AsyncExecutionFileWrite;
struct AsyncExecutionFileWriteBuffers;
struct AsyncExecutionFileReadMany;
struct AsyncExecutionFileWriteAll;
struct AsyncExecutionFileSend;
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
struct AsyncExecutionPoolTask;
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

    /// Wait once for a coalescing, possibly cross-thread notification.
    AsyncExecutionNotificationSender wakeUp(AsyncExecutionNotification& notification);

    /// One readable-readiness observation. Borrows the descriptor, consumes no bytes and does not rearm.
    AsyncExecutionSender<detail::AsyncExecutionFilePoll> filePoll(const FileDescriptor& file);

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

    /// Waits for a borrowed child handle. Stopping never terminates it; observed native completion may reap it.
    AsyncExecutionSender<detail::AsyncExecutionProcessExit> processExit(
        FileDescriptor::Handle process, AsyncExecutionProcessExitResult* progress = nullptr);
    /// Completes on the next delivery; native signal support and coalescing limits apply.
    AsyncExecutionSender<detail::AsyncExecutionSignal> signal(int signalNumber, AsyncSignalOptions options = {});

    /// One datagram per operation. Addresses are copied; sockets, payloads and vector metadata are borrowed.
    AsyncExecutionSender<detail::AsyncExecutionSendTo> sendTo(const SocketDescriptor& socket, SocketIPAddress address,
                                                              Span<const char> data);
    AsyncExecutionSender<detail::AsyncExecutionSendTo> sendTo(const SocketDescriptor& socket, SocketAddress address,
                                                              Span<const char> data);
    AsyncExecutionSender<detail::AsyncExecutionSendToBuffers> sendTo(const SocketDescriptor& socket,
                                                                     SocketIPAddress         address,
                                                                     Span<Span<const char>>  data);
    AsyncExecutionSender<detail::AsyncExecutionSendToBuffers> sendTo(const SocketDescriptor& socket,
                                                                     SocketAddress           address,
                                                                     Span<Span<const char>>  data);
    AsyncExecutionSender<detail::AsyncExecutionReceiveFrom>   receiveFrom(const SocketDescriptor& socket,
                                                                          Span<char>              buffer);

    AsyncExecutionSender<detail::AsyncExecutionFileRead>     fileRead(const FileDescriptor& file, Span<char> buffer,
                                                                      AsyncExecutionFileReadOptions options = {});
    AsyncExecutionSender<detail::AsyncExecutionFileRead>     fileReadAt(const FileDescriptor& file, uint64_t offset,
                                                                        Span<char>                    buffer,
                                                                        AsyncExecutionFileReadOptions options = {});
    AsyncExecutionSender<detail::AsyncExecutionFileReadMany> fileReadExact(
        const FileDescriptor& file, Span<char> buffer, AsyncExecutionFileReadOptions options = {},
        AsyncExecutionReadResult* progress = nullptr);
    AsyncExecutionSender<detail::AsyncExecutionFileReadMany> fileReadExactAt(
        const FileDescriptor& file, uint64_t offset, Span<char> buffer, AsyncExecutionFileReadOptions options = {},
        AsyncExecutionReadResult* progress = nullptr);
    AsyncExecutionSender<detail::AsyncExecutionFileReadMany> fileReadUntilFullOrEOF(
        const FileDescriptor& file, Span<char> buffer, AsyncExecutionFileReadOptions options = {},
        AsyncExecutionReadResult* progress = nullptr);

    AsyncExecutionSender<detail::AsyncExecutionFileWrite> fileWrite(const FileDescriptor& file, Span<const char> data,
                                                                    AsyncExecutionFileWriteOptions options = {});
    AsyncExecutionSender<detail::AsyncExecutionFileWriteBuffers> fileWrite(const FileDescriptor&          file,
                                                                           Span<Span<const char>>         data,
                                                                           AsyncExecutionFileWriteOptions options = {});
    AsyncExecutionSender<detail::AsyncExecutionFileWrite> fileWriteAt(const FileDescriptor& file, uint64_t offset,
                                                                      Span<const char>               data,
                                                                      AsyncExecutionFileWriteOptions options = {});
    AsyncExecutionSender<detail::AsyncExecutionFileWriteBuffers> fileWriteAt(
        const FileDescriptor& file, uint64_t offset, Span<Span<const char>> data,
        AsyncExecutionFileWriteOptions options = {});

    AsyncExecutionSender<detail::AsyncExecutionFileWriteAll> fileWriteAll(const FileDescriptor&          file,
                                                                          Span<const char>               data,
                                                                          AsyncExecutionFileWriteOptions options = {},
                                                                          size_t* progress = nullptr);
    AsyncExecutionSender<detail::AsyncExecutionFileWriteAll> fileWriteAll(const FileDescriptor&          file,
                                                                          Span<Span<const char>>         data,
                                                                          AsyncExecutionFileWriteOptions options = {},
                                                                          size_t* progress = nullptr);
    AsyncExecutionSender<detail::AsyncExecutionFileWriteAll> fileWriteAllAt(const FileDescriptor& file, uint64_t offset,
                                                                            Span<const char>               data,
                                                                            AsyncExecutionFileWriteOptions options = {},
                                                                            size_t* progress = nullptr);
    AsyncExecutionSender<detail::AsyncExecutionFileWriteAll> fileWriteAllAt(const FileDescriptor& file, uint64_t offset,
                                                                            Span<Span<const char>>         data,
                                                                            AsyncExecutionFileWriteOptions options = {},
                                                                            size_t* progress = nullptr);

    /// Compatibility forms retain the prototype's absolute-offset addressing and native-preferred pool.
    AsyncExecutionSender<detail::AsyncExecutionFileRead>  fileRead(const FileDescriptor& file, Span<char> buffer,
                                                                   ThreadPool& pool, uint64_t offset = 0);
    AsyncExecutionSender<detail::AsyncExecutionFileWrite> fileWrite(const FileDescriptor& file, Span<const char> data,
                                                                    ThreadPool& pool, uint64_t offset = 0);

    AsyncExecutionSender<detail::AsyncExecutionFileSend> fileSend(const FileDescriptor&         file,
                                                                  const SocketDescriptor&       socket,
                                                                  AsyncExecutionFileSendOptions options  = {},
                                                                  AsyncExecutionFileSendResult* progress = nullptr);

    AsyncExecutionSender<detail::AsyncExecutionWork> work(ThreadPool& pool, Function<Result()> procedure);

    AsyncEventLoop& loop;

  private:
    friend struct AsyncExecution;
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
    friend struct AsyncExecutionScope;
#endif
    friend struct AsyncExecutionNotification;
    template <typename Receiver>
    friend struct detail::AsyncExecutionNotificationOperation;
    friend struct detail::AsyncExecutionPoolTask;
    template <typename Setup, typename Receiver>
    friend struct detail::AsyncExecutionOperation;
    template <typename Receiver>
    friend struct detail::AsyncExecutionPoolOperation;
    Mutex           mutex;
    AsyncLoopWakeUp dispatcherWakeUp;

    detail::AsyncExecutionCommand* head = nullptr;
    detail::AsyncExecutionCommand* tail = nullptr;

    Atomic<int32_t> outstanding       = 0;
    Atomic<int32_t> delivering        = 0;
    Atomic<int32_t> poolNotifications = 0;

    Atomic<bool> accepting   = false;
    uint64_t     ownerThread = 0;

    bool dispatching = false;
    bool running     = false;
    bool closing     = false;

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
    AsyncExecutionScheduler query(
        AsyncExecutionProtocol::get_completion_scheduler_t<AsyncExecutionProtocol::set_value_t>) const noexcept
    {
        return *this;
    }
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

/// Successful scheduling completes on a worker, after the pool releases the embedded task node.
struct AsyncExecutionPoolScheduler;
namespace detail
{
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
using AsyncExecutionScopeToken       = decltype(std::declval<AsyncExecutionProtocol::counting_scope&>().get_token());
using AsyncExecutionScopeAssociation = decltype(std::declval<AsyncExecutionScopeToken>().try_associate());

struct AsyncExecutionScopeQuery
{
    static constexpr bool query(AsyncExecutionProtocol::forwarding_query_t) noexcept { return true; }
    template <typename Environment>
    auto operator()(const Environment& environment) const noexcept -> decltype(environment.query(*this))
    {
        return environment.query(*this);
    }
};

#endif

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionPoolTask
{
    using Notify = void (*)(void*) noexcept;
    ThreadPoolTask task;

#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
    AsyncExecutionScopeAssociation association;
#endif

    AsyncExecutionContext* context   = nullptr;
    void*                  operation = nullptr;
    Notify                 notify    = nullptr;

    Result submit(AsyncExecutionContext& context, ThreadPool& pool, void* operation, Notify notify) noexcept;
};
template <typename Receiver>
struct AsyncExecutionPoolOperation
{
    using operation_state_concept = AsyncExecutionProtocol::operation_state_tag;
    AsyncExecutionContext& context;
    ThreadPool&            pool;
    Receiver               receiver;
    AsyncExecutionPoolTask task;

    AsyncExecutionPoolOperation(AsyncExecutionContext& context, ThreadPool& pool, Receiver receiver)
        : context(context), pool(pool), receiver(move(receiver))
    {}
    AsyncExecutionPoolOperation(const AsyncExecutionPoolOperation&) = delete;
    void start() noexcept
    {
#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
        if constexpr (requires { AsyncExecutionScopeQuery{}(AsyncExecutionProtocol::get_env(receiver)); })
        {
            task.association = AsyncExecutionScopeQuery{}(AsyncExecutionProtocol::get_env(receiver)).try_associate();
            if (not task.association)
            {
                AsyncExecutionProtocol::set_error(
                    move(receiver), Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::ScopeClosed));
                return;
            }
        }
#endif
        const Result queued =
            task.submit(context, pool, this,
                        [](void* operation) noexcept
                        {
                            auto receiver = move(static_cast<AsyncExecutionPoolOperation*>(operation)->receiver);
                            auto token =
                                AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver));
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
    AsyncExecutionPoolScheduler query(
        AsyncExecutionProtocol::get_completion_scheduler_t<AsyncExecutionProtocol::set_value_t>) const noexcept
    {
        return *this;
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

struct AsyncExecutionProcessExitResult
{
    int  exitStatus = -1;
    bool complete   = false;
};

struct AsyncExecutionSignalResult
{
    int      signalNumber  = 0;
    uint32_t deliveryCount = 0;
};

/// One received datagram. data borrows the receive buffer; both source-address values are copied.
struct AsyncExecutionSocketReceiveFromResult
{
    Span<char>      data;
    SocketIPAddress sourceAddress;
    SocketAddress   sourceSocketAddress;
    bool            truncated = false;
};

struct AsyncExecutionFileSendResult
{
    size_t bytesTransferred = 0;
    bool   usedZeroCopy     = false;
    bool   complete         = false;
};

namespace detail
{
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFilePoll
{
    using Request = AsyncFileReadiness;
    using Value   = AsyncCompletionData;
    const FileDescriptor& file;

    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value value(Request::Result&) { return {}; }
};

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

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionProcessExit
{
    using Request = AsyncProcessExit;
    using Value   = AsyncExecutionProcessExitResult;
    FileDescriptor::Handle           process;
    AsyncExecutionProcessExitResult* progress = nullptr;

    void         initialize();
    void         observe(Request::Result& result);
    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value value(Request::Result& result);
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionSignal
{
    using Request = AsyncSignal;
    using Value   = AsyncExecutionSignalResult;
    int                signalNumber;
    AsyncSignalOptions options;
    Result             submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value       value(Request::Result& result);
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionSendTo
{
    using Request = AsyncSocketSendTo;
    using Value   = size_t;
    const SocketDescriptor& socket;
    SocketAddress           address;
    Span<const char>        data;
    Result                  submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value            value(Request::Result& result) { return result.completionData.numBytes; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionSendToBuffers
{
    using Request = AsyncSocketSendTo;
    using Value   = size_t;
    const SocketDescriptor& socket;
    SocketAddress           address;
    Span<Span<const char>>  data;
    Result                  submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value            value(Request::Result& result) { return result.completionData.numBytes; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionReceiveFrom
{
    using Request = AsyncSocketReceiveFrom;
    using Value   = AsyncExecutionSocketReceiveFromResult;
    const SocketDescriptor& socket;
    Span<char>              buffer;
    Result                  submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence&) const;
    static Value            value(Request::Result& result);
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFileRead
{
    using Request = AsyncFileRead;
    using Value   = AsyncExecutionReadResult;
    const FileDescriptor&         file;
    Span<char>                    buffer;
    AsyncExecutionFileReadOptions options;

    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const;
    static Value value(Request::Result& result);
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFileWrite
{
    using Request = AsyncFileWrite;
    using Value   = size_t;
    const FileDescriptor&          file;
    Span<const char>               data;
    AsyncExecutionFileWriteOptions options;

    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const;
    static Value value(Request::Result& result) { return result.completionData.numBytes; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFileWriteBuffers
{
    using Request = AsyncFileWrite;
    using Value   = size_t;
    const FileDescriptor&          file;
    Span<Span<const char>>         data;
    AsyncExecutionFileWriteOptions options;

    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const;
    static Value value(Request::Result& result) { return result.completionData.numBytes; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFileReadMany
{
    using Request = AsyncFileRead;
    using Value   = AsyncExecutionReadResult;
    const FileDescriptor&         file;
    Span<char>                    buffer;
    AsyncExecutionFileReadOptions options;
    Value*                        progress    = nullptr;
    bool                          requireFull = false;

    size_t received  = 0;
    bool   endOfFile = false;
    Result initialization{true};

    void   initialize();
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const;
    Result advance(Request::Result& result);
    Result advanceBytes(size_t count, bool ended);
    bool   hasRemaining() const { return not endOfFile and received < buffer.sizeInBytes(); }
    bool   completesImmediately() const { return initialization and buffer.empty(); }
    Value  immediateValue() { return {{buffer.data(), 0}, false}; }
    Value  value(Request::Result&) { return {{buffer.data(), received}, endOfFile}; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFileWriteAll
{
    using Request = AsyncFileWrite;
    using Value   = size_t;
    const FileDescriptor&          file;
    Span<const char>               data;
    Span<Span<const char>>         buffers;
    AsyncExecutionFileWriteOptions options;
    size_t*                        progress     = nullptr;
    bool                           singleBuffer = true;

    size_t index       = 0;
    size_t offset      = 0;
    size_t transferred = 0;
    Result initialization{true};

    void   initialize();
    Result remaining(Span<const char>& output) const;
    Result submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const;
    Result advance(Request::Result& result);
    Result advanceBytes(size_t count);
    void   findNextBuffer();
    bool  hasRemaining() const { return singleBuffer ? offset < data.sizeInBytes() : index < buffers.sizeInElements(); }
    bool  completesImmediately() const { return initialization and not hasRemaining(); }
    Value immediateValue() const { return transferred; }
    Value value(Request::Result&) const { return transferred; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionFileSend
{
    using Request = AsyncFileSend;
    using Value   = AsyncExecutionFileSendResult;

    const FileDescriptor&   file;
    const SocketDescriptor& socket;

    AsyncExecutionFileSendOptions options;

    Value* progress = nullptr;

    void         initialize();
    Result       submit(Request& request, AsyncEventLoop& loop, AsyncTaskSequence& task) const;
    void         observe(Request::Result& result);
    static Value value(Request::Result& result);
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
        if constexpr (requires { setup.observe(result); })
            setup.observe(result); // Preserve native progress even on error or a concurrent stop.
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

/// A stable producer-facing notification. Join producers and complete its wait before destruction.
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionNotification
{
    explicit AsyncExecutionNotification(AsyncExecutionContext& context) noexcept;
    ~AsyncExecutionNotification();
    AsyncExecutionNotification(const AsyncExecutionNotification&) = delete;
    AsyncExecutionNotification(AsyncExecutionNotification&&)      = delete;

    /// Coalesces notifications, including before a wait. Success means published, not delivered.
    Result notify() noexcept;

  private:
    template <typename Receiver>
    friend struct detail::AsyncExecutionNotificationOperation;
    Result attach(detail::AsyncExecutionNotificationWaiter& waiter) noexcept;
    void   detach(detail::AsyncExecutionNotificationWaiter& waiter) noexcept;
    void   dispatch() noexcept;

    AsyncExecutionContext& context;
    Mutex                  mutex;

    detail::AsyncExecutionCommand command;

    detail::AsyncExecutionNotificationWaiter* waiter = nullptr;

    bool pending = false;
};

namespace detail
{
template <typename Receiver>
struct AsyncExecutionNotificationOperation
{
    using operation_state_concept = AsyncExecutionProtocol::operation_state_tag;
    AsyncExecutionContext&      context;
    AsyncExecutionNotification& notification;

    Receiver receiver;

    AsyncExecutionNotificationWaiter waiter;

    AsyncExecutionCommand command;
    AsyncExecutionCommand cancelCommand;

    struct StopRequest
    {
        AsyncExecutionNotificationOperation* operation;
        void operator()() const noexcept { SC_ASYNC_TRUST_RESULT(operation->context.post(operation->cancelCommand)); }
    };
    using StopToken    = decltype(AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver)));
    using StopCallback = typename StopToken::template callback_type<StopRequest>;
    AlignedStorage<sizeof(StopCallback), alignof(StopCallback)> stopStorage;

    bool stopRegistered = false;

    AsyncExecutionNotificationOperation(AsyncExecutionContext& context, AsyncExecutionNotification& notification,
                                        Receiver receiver)
        : context(context), notification(notification), receiver(move(receiver))
    {}
    AsyncExecutionNotificationOperation(const AsyncExecutionNotificationOperation&) = delete;
    AsyncExecutionNotificationOperation(AsyncExecutionNotificationOperation&&)      = delete;

    bool stopRequested() const noexcept
    {
        return AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver)).stop_requested();
    }
    void finish(Result status, bool stopped) noexcept
    {
        if (stopRegistered)
        {
            stopRegistered = false;
            reinterpret_cast<StopCallback*>(&stopStorage)->~StopCallback();
        }
        notification.detach(waiter);
        context.remove(cancelCommand);
        context.outstanding.fetch_sub(1);
        typename AsyncExecutionOperation<AsyncExecutionTimer, Receiver>::Delivery delivery(context);
        if (not status)
            AsyncExecutionProtocol::set_error(move(receiver), status);
        else if (stopped)
            AsyncExecutionProtocol::set_stopped(move(receiver));
        else
            AsyncExecutionProtocol::set_value(move(receiver));
    }
    void startOnLoop() noexcept
    {
        cancelCommand.operation = this;
        cancelCommand.execute   = [](AsyncExecutionCommand& command) noexcept
        { static_cast<AsyncExecutionNotificationOperation*>(command.operation)->finish(Result(true), true); };
        auto token = AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver));
        placementNew(*reinterpret_cast<StopCallback*>(&stopStorage), token, StopRequest{this});
        stopRegistered = true;
        if (stopRequested())
        {
            finish(Result(true), true);
            return;
        }
        if (&context != &notification.context)
        {
            finish(Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::NotificationContextMismatch),
                   false);
            return;
        }
        waiter.operation = this;
        waiter.deliver   = [](void* pointer) noexcept
        {
            auto* operation = static_cast<AsyncExecutionNotificationOperation*>(pointer);
            operation->finish(Result(true), operation->stopRequested());
        };
        const Result status = notification.attach(waiter);
        if (not status)
            finish(status, false);
    }
    void start() noexcept
    {
        command.operation = this;
        command.execute   = [](AsyncExecutionCommand& command) noexcept
        { static_cast<AsyncExecutionNotificationOperation*>(command.operation)->startOnLoop(); };
        context.outstanding.fetch_add(1);
        const Result status = context.post(command);
        if (not status)
            finish(status, false);
    }
};
} // namespace detail

struct AsyncExecutionNotificationSender
{
    using sender_concept = AsyncExecutionProtocol::sender_tag;
    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        return AsyncExecutionProtocol::completion_signatures<AsyncExecutionProtocol::set_value_t(),
                                                             AsyncExecutionProtocol::set_error_t(Result),
                                                             AsyncExecutionProtocol::set_stopped_t()>{};
    }
    AsyncExecutionContext&      context;
    AsyncExecutionNotification& notification;
    auto                        get_env() const noexcept { return AsyncExecutionEnvironment{context.getScheduler()}; }
    template <typename Receiver>
    auto connect(Receiver receiver) const noexcept
    {
        return detail::AsyncExecutionNotificationOperation<Receiver>{context, notification, move(receiver)};
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

    /// Owner-thread shutdown. Rejects active adapter work; joins no running graph.
    /// Idempotent after successful close; a failed cleanup can be retried after resolving its cause.
    Result close();

    AsyncExecutionScheduler     getScheduler() noexcept { return context.getScheduler(); }
    AsyncExecutionPoolScheduler getPoolScheduler() noexcept { return {&context, &pool}; }

    AsyncExecutionSender<detail::AsyncExecutionTimer> sleep(TimeMs duration);

    AsyncExecutionSender<detail::AsyncExecutionOwnedRead> readFile(StringSpan path,
                                                                   size_t     maximumBytes = 64 * 1024 * 1024);

    AsyncExecutionSender<detail::AsyncExecutionOwnedWrite> writeFile(StringSpan path, AsyncExecutionBuffer data);

    /// SC extension: run the body on the owned pool and complete on this runtime's owner loop.
    /// Standard on restores the caller's scheduler; schedule(pool) continues on a worker.
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

    detail::AsyncExecutionTCPSender sendTCP(StringSpan host, uint16_t port, AsyncExecutionBuffer data);

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

    uint64_t ownerThread = 0;

    bool loopCreated    = false;
    bool poolCreated    = false;
    bool contextCreated = false;
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

#if SC_ASYNC_EXECUTION_SCOPES_AVAILABLE
/// Allocation callbacks must return correctly aligned storage or nullptr. Both null selects the default allocator.
struct AsyncExecutionScopeAllocator
{
    void* userData = nullptr;

    void* (*allocate)(void*, size_t, size_t) noexcept         = nullptr;
    void (*deallocate)(void*, void*, size_t, size_t) noexcept = nullptr;
};

struct AsyncExecutionScopeSummary
{
    size_t accepted  = 0;
    size_t succeeded = 0;
    size_t failed    = 0;
    size_t stopped   = 0;
    Result firstError{true};
};

namespace detail
{
struct AsyncExecutionScopeNode;
template <typename Sender>
struct AsyncExecutionScopeChild;
template <typename Receiver>
struct AsyncExecutionScopeJoinOperation;
} // namespace detail

/// Own connected child storage while borrowing the context. A used scope must be joined before destruction.
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionScope
{
    explicit AsyncExecutionScope(AsyncExecutionContext& context, AsyncExecutionScopeAllocator allocator = {});
    ~AsyncExecutionScope();
    AsyncExecutionScope(const AsyncExecutionScope&) = delete;
    AsyncExecutionScope(AsyncExecutionScope&&)      = delete;

    template <typename Sender>
    Result spawn(Sender&& sender);

    void requestStop() noexcept;

    AsyncExecutionScopeJoinSender join() noexcept;
    AsyncExecutionScopeSummary    getSummary();

  private:
    friend struct detail::AsyncExecutionScopeNode;
    template <typename Sender>
    friend struct detail::AsyncExecutionScopeChild;
    template <typename Receiver>
    friend struct detail::AsyncExecutionScopeJoinOperation;

    AsyncExecutionContext&       context;
    AsyncExecutionScopeAllocator allocator;

    AsyncExecutionProtocol::counting_scope provider;

    AsyncExecutionScopeSummary summary;

    Mutex mutex;
    bool  sealed       = false;
    bool  joining      = false;
    bool  used         = false;
    bool  joined       = false;
    bool  contextBound = false;

    Result checkSpawn();
    Result beginChild(detail::AsyncExecutionScopeAssociation& association);
    Result beginJoin();

    void acceptedChild();
    void abandonChild();
    void completedChild(Result status, bool stopped);

    AsyncExecutionScopeSummary finishJoin();
};

namespace detail
{
struct AsyncExecutionScopeEnvironment
{
    AsyncExecutionScheduler  scheduler;
    AsyncExecutionScopeToken token;

    auto query(AsyncExecutionProtocol::get_scheduler_t) const noexcept { return scheduler; }
    auto query(AsyncExecutionProtocol::get_start_scheduler_t) const noexcept { return scheduler; }
    auto query(AsyncExecutionScopeQuery) const noexcept { return token; }
};

struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionScopeNode
{
    AsyncExecutionScope&           scope;
    AsyncExecutionScopeAssociation association;
    void (*destroy)(AsyncExecutionScopeNode*) noexcept;

    AsyncExecutionScopeNode(AsyncExecutionScope& scope, AsyncExecutionScopeAssociation association,
                            void (*destroy)(AsyncExecutionScopeNode*) noexcept)
        : scope(scope), association(move(association)), destroy(destroy)
    {}
    AsyncExecutionScopeEnvironment getEnvironment() const noexcept;

    void complete(Result status, bool stopped) noexcept;
};

struct AsyncExecutionScopeReceiver
{
    using receiver_concept = AsyncExecutionProtocol::receiver_tag;
    AsyncExecutionScopeNode* node;
    auto                     get_env() const noexcept { return node->getEnvironment(); }
    template <typename... Values>
    void set_value(Values&&...) && noexcept
    {
        node->complete(Result(true), false);
    }
    void set_error(Result result) && noexcept { node->complete(result ? Result(false) : result, false); }
    void set_stopped() && noexcept { node->complete(Result(true), true); }
};

template <typename Sender>
struct AsyncExecutionScopeChild : AsyncExecutionScopeNode
{
    using Wrapped   = decltype(std::declval<AsyncExecutionScopeToken>().wrap(std::declval<Sender>()));
    using Operation = decltype(AsyncExecutionProtocol::connect(std::declval<Wrapped>(), AsyncExecutionScopeReceiver{}));
    Operation operation;

    AsyncExecutionScopeChild(AsyncExecutionScope& scope, AsyncExecutionScopeAssociation association, Sender&& sender)
        : AsyncExecutionScopeNode(scope, move(association),
                                  [](AsyncExecutionScopeNode* node) noexcept
                                  {
                                      auto*      self      = static_cast<AsyncExecutionScopeChild*>(node);
                                      const auto allocator = self->scope.allocator;
                                      self->~AsyncExecutionScopeChild();
                                      allocator.deallocate(allocator.userData, self, sizeof(AsyncExecutionScopeChild),
                                                           alignof(AsyncExecutionScopeChild));
                                  }),
          operation(AsyncExecutionProtocol::connect(scope.provider.get_token().wrap(forward<Sender>(sender)),
                                                    AsyncExecutionScopeReceiver{this}))
    {}
};

template <typename Receiver>
struct AsyncExecutionScopeJoinOperation
{
    using operation_state_concept = AsyncExecutionProtocol::operation_state_tag;
    AsyncExecutionScope& scope;
    Receiver             receiver;
    struct Environment
    {
        AsyncExecutionScheduler scheduler;
        auto                    query(AsyncExecutionProtocol::get_scheduler_t) const noexcept { return scheduler; }
        auto query(AsyncExecutionProtocol::get_start_scheduler_t) const noexcept { return scheduler; }
        // Joining cannot be abandoned. Caller stop is handled by the outer operation instead.
    };
    struct Bridge
    {
        using receiver_concept = AsyncExecutionProtocol::receiver_tag;
        AsyncExecutionScopeJoinOperation* operation;
        AsyncExecutionScope*              owner;

        Environment get_env() const noexcept { return {owner->context.getScheduler()}; }
        void        set_value() && noexcept { operation->complete(Result(true)); }
        void        set_error(Result result) && noexcept { operation->complete(result); }
        void        set_stopped() && noexcept
        {
            operation->complete(Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::Stopped));
        }
    };
    using Inner = decltype(AsyncExecutionProtocol::connect(
        std::declval<AsyncExecutionProtocol::counting_scope&>().join(), Bridge{}));
    Inner inner;
    struct Stop
    {
        AsyncExecutionScope* scope;
        void                 operator()() const noexcept { scope->requestStop(); }
    };
    using Token    = decltype(AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver)));
    using Callback = typename Token::template callback_type<Stop>;
    AlignedStorage<sizeof(Callback), alignof(Callback)> storage;

    bool registered = false;

    Callback* callback() noexcept { return reinterpret_cast<Callback*>(&storage); }

    AsyncExecutionScopeJoinOperation(AsyncExecutionScope& scope, Receiver receiver)
        : scope(scope), receiver(move(receiver)),
          inner(AsyncExecutionProtocol::connect(scope.provider.join(), Bridge{this, &scope}))
    {}
    AsyncExecutionScopeJoinOperation(const AsyncExecutionScopeJoinOperation&) = delete;
    ~AsyncExecutionScopeJoinOperation() { SC_ASYNC_ASSERT_RELEASE(not registered); }

    void start() noexcept
    {
        const Result status = scope.beginJoin();
        if (not status)
        {
            AsyncExecutionProtocol::set_error(move(receiver), status);
            return;
        }
        auto token = AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver));
        placementNew(*callback(), token, Stop{&scope});
        registered = true;
        AsyncExecutionProtocol::start(inner);
    }
    void complete(Result status) noexcept
    {
        registered = false;
        callback()->~Callback();
        auto summary = scope.finishJoin();
        auto target  = move(receiver);
        if (not status)
            AsyncExecutionProtocol::set_error(move(target), status);
        else if (not summary.firstError)
            AsyncExecutionProtocol::set_error(move(target), summary.firstError);
        else
            AsyncExecutionProtocol::set_value(move(target), summary);
    }
};
} // namespace detail

struct AsyncExecutionScopeJoinSender
{
    using sender_concept = AsyncExecutionProtocol::sender_tag;
    AsyncExecutionScope* scope;
    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        return AsyncExecutionProtocol::completion_signatures<AsyncExecutionProtocol::set_value_t(
                                                                 AsyncExecutionScopeSummary),
                                                             AsyncExecutionProtocol::set_error_t(Result)>{};
    }
    template <typename Receiver>
    auto connect(Receiver receiver) const noexcept
    {
        return detail::AsyncExecutionScopeJoinOperation<Receiver>(*scope, move(receiver));
    }
};

template <typename Sender>
Result AsyncExecutionScope::spawn(Sender&& sender)
{
    SC_TRY(checkSpawn());
    using Child  = detail::AsyncExecutionScopeChild<Sender>;
    void* memory = allocator.allocate(allocator.userData, sizeof(Child), alignof(Child));
    if (memory == nullptr)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::OutOfMemory);
    detail::AsyncExecutionScopeAssociation association;
    const Result                           associated = beginChild(association);
    if (not associated)
    {
        allocator.deallocate(allocator.userData, memory, sizeof(Child), alignof(Child));
        return associated;
    }
    auto* child = static_cast<Child*>(memory);
    placementNew(*child, *this, move(association), forward<Sender>(sender));
    acceptedChild();
    AsyncExecutionProtocol::start(child->operation);
    return Result(true); // start may have synchronously destroyed the child and completed join.
}

#endif // SC_ASYNC_EXECUTION_SCOPES_AVAILABLE

namespace detail
{
struct AsyncExecutionTimeoutStopEnvironment
{
    decltype(std::declval<AsyncExecutionStopSource&>().get_token()) token;
    auto query(AsyncExecutionProtocol::get_stop_token_t) const noexcept { return token; }
};

/// Arbitration is separate from provider-owned connected storage and value retention.
struct SC_ASYNC_EXECUTION_EXPORT AsyncExecutionTimeoutState
{
    AsyncExecutionStopSource workStop;
    AsyncExecutionStopSource timerStop;

    void   workCompleted(Result status, bool stopped) noexcept;
    void   timerCompleted(Result status, bool stopped) noexcept;
    bool   beginCallerStop() noexcept;
    bool   endCallerStop() noexcept;
    bool   joined() noexcept;
    Result outcome(bool& stopped) noexcept;

  private:
    Mutex mutex;
    enum class Winner
    {
        Pending,
        Work,
        Timer
    };
    Winner winner = Winner::Pending;
    Result workError{true};
    Result timerError{true};
    bool   workStopped    = false;
    bool   callerStopped  = false;
    bool   stoppingCaller = false;
    bool   bothJoined     = false;
    bool   delivering     = false;
};

template <typename Signature>
struct AsyncExecutionTimeoutSignature;
template <typename... Values>
struct AsyncExecutionTimeoutSignature<AsyncExecutionProtocol::set_value_t(Values...)>
{
    static_assert((std::is_nothrow_constructible_v<std::decay_t<Values>, Values> and ...),
                  "timeout values require nonthrowing owned storage construction");
    using Type = AsyncExecutionProtocol::set_value_t(std::decay_t<Values>...);
};
template <typename Error>
struct AsyncExecutionTimeoutSignature<AsyncExecutionProtocol::set_error_t(Error)>
{
    static_assert(std::is_same_v<std::decay_t<Error>, Result>, "timeout errors must carry SC::Result");
    using Type = AsyncExecutionProtocol::set_error_t(Result);
};
template <>
struct AsyncExecutionTimeoutSignature<AsyncExecutionProtocol::set_stopped_t()>
{
    using Type = AsyncExecutionProtocol::set_stopped_t();
};
template <typename Signatures>
struct AsyncExecutionTimeoutSignatures
{
    // Preserve the provider's dependent/diagnostic result until a concrete environment is supplied.
    using Type = Signatures;
};
template <typename... Signatures>
struct AsyncExecutionTimeoutSignatures<AsyncExecutionProtocol::completion_signatures<Signatures...>>
{
    using Type =
        AsyncExecutionProtocol::completion_signatures<typename AsyncExecutionTimeoutSignature<Signatures>::Type...,
                                                      AsyncExecutionProtocol::set_error_t(Result),
                                                      AsyncExecutionProtocol::set_stopped_t()>;
};

// Owned snapshots live in the outer operation, independently of completed child operation states.
template <typename... Values>
struct AsyncExecutionTimeoutValues;
template <>
struct AsyncExecutionTimeoutValues<>
{
    template <typename Receiver, typename... Outputs>
    void emit(Receiver&& receiver, Outputs&&... outputs) noexcept
    {
        AsyncExecutionProtocol::set_value(forward<Receiver>(receiver), forward<Outputs>(outputs)...);
    }
};
template <typename Head, typename... Tail>
struct AsyncExecutionTimeoutValues<Head, Tail...>
{
    Head head;

    AsyncExecutionTimeoutValues<Tail...> tail;
    AsyncExecutionTimeoutValues(Head&& head, Tail&&... tail) noexcept : head(move(head)), tail(move(tail)...) {}
    template <typename Receiver, typename... Outputs>
    void emit(Receiver&& receiver, Outputs&&... outputs) noexcept
    {
        tail.emit(forward<Receiver>(receiver), forward<Outputs>(outputs)..., move(head));
    }
};

template <typename Signature>
struct AsyncExecutionTimeoutLayout
{
    static constexpr size_t Size      = 1;
    static constexpr size_t Alignment = 1;
};
template <typename... Values>
struct AsyncExecutionTimeoutLayout<AsyncExecutionProtocol::set_value_t(Values...)>
{
    using Pack = AsyncExecutionTimeoutValues<std::decay_t<Values>...>;

    static constexpr size_t Size      = sizeof(Pack);
    static constexpr size_t Alignment = alignof(Pack);
};
template <typename Signatures>
struct AsyncExecutionTimeoutStorage;
template <typename... Signatures>
struct AsyncExecutionTimeoutStorage<AsyncExecutionProtocol::completion_signatures<Signatures...>>
{
    static constexpr size_t Size = []() consteval
    {
        size_t maximum = 1;
        ((maximum = maximum < AsyncExecutionTimeoutLayout<Signatures>::Size
                        ? AsyncExecutionTimeoutLayout<Signatures>::Size
                        : maximum),
         ...);
        return maximum;
    }();
    static constexpr size_t Alignment = []() consteval
    {
        size_t maximum = 1;
        ((maximum = maximum < AsyncExecutionTimeoutLayout<Signatures>::Alignment
                        ? AsyncExecutionTimeoutLayout<Signatures>::Alignment
                        : maximum),
         ...);
        return maximum;
    }();
};

template <typename Sender, typename Timer, typename Receiver>
struct AsyncExecutionTimeoutOperation
{
    using operation_state_concept = AsyncExecutionProtocol::operation_state_tag;

    using Self = AsyncExecutionTimeoutOperation;
    AsyncExecutionTimeoutState state;

    Receiver receiver;

    using Work =
        decltype(AsyncExecutionProtocol::write_env(std::declval<Sender>(), AsyncExecutionTimeoutStopEnvironment{}));
    using Environment = decltype(AsyncExecutionProtocol::get_env(std::declval<Receiver&>()));
    using Layout =
        AsyncExecutionTimeoutStorage<decltype(AsyncExecutionProtocol::get_completion_signatures<Work, Environment>())>;
    AlignedStorage<Layout::Size, Layout::Alignment> output;

    void (*emit)(void*, Receiver&) noexcept = nullptr;
    void (*destroy)(void*) noexcept         = nullptr;

    template <typename... Values>
    void storeValues(Values&&... values) noexcept
    {
        using Pack = AsyncExecutionTimeoutValues<std::decay_t<Values>...>;
        static_assert(sizeof(Pack) <= Layout::Size and alignof(Pack) <= Layout::Alignment);
        auto* stored = reinterpret_cast<Pack*>(&output);
        placementNew(*stored, forward<Values>(values)...);
        emit    = [](void* storage, Receiver& target) noexcept { static_cast<Pack*>(storage)->emit(move(target)); };
        destroy = [](void* storage) noexcept { static_cast<Pack*>(storage)->~Pack(); };
    }
    struct Capture
    {
        Self* owner;

        AsyncExecutionTimeoutState* state;
        template <typename... Values>
        auto operator()(Values&... values) const noexcept -> decltype(AsyncExecutionProtocol::just())
        {
            owner->storeValues(move(values)...);
            state->workCompleted(Result(true), false);
            return AsyncExecutionProtocol::just();
        }
    };
    struct Error
    {
        AsyncExecutionTimeoutState* state;

        bool timer;
        auto operator()(Result status) const noexcept
        {
            if (timer)
                state->timerCompleted(status ? Result(false) : status, false);
            else
                state->workCompleted(status ? Result(false) : status, false);
            return AsyncExecutionProtocol::just();
        }
    };
    struct Stopped
    {
        AsyncExecutionTimeoutState* state;

        bool timer;
        auto operator()() const noexcept
        {
            if (timer)
                state->timerCompleted(Result(true), true);
            else
                state->workCompleted(Result(true), true);
            return AsyncExecutionProtocol::just();
        }
    };
    struct TimerValue
    {
        AsyncExecutionTimeoutState* state;
        void                        operator()() const noexcept { state->timerCompleted(Result(true), false); }
    };
    static auto children(Sender sender, Timer timer, Self* owner, AsyncExecutionTimeoutState& state)
    {
        namespace ex = AsyncExecutionProtocol;
        auto work =
            ex::let_stopped(ex::let_error(ex::let_value(move(sender), Capture{owner, &state}), Error{&state, false}),
                            Stopped{&state, false});
        auto deadline = ex::let_stopped(ex::let_error(ex::then(move(timer), TimerValue{&state}), Error{&state, true}),
                                        Stopped{&state, true});
        return ex::unstoppable(ex::when_all(
            ex::write_env(move(work), AsyncExecutionTimeoutStopEnvironment{state.workStop.get_token()}),
            ex::write_env(move(deadline), AsyncExecutionTimeoutStopEnvironment{state.timerStop.get_token()})));
    }
    struct Joined
    {
        using receiver_concept = AsyncExecutionProtocol::receiver_tag;
        Self*     owner;
        Receiver* target;
        auto      get_env() const noexcept { return AsyncExecutionProtocol::get_env(*target); }
        void      set_value() && noexcept
        {
            if (owner->state.joined())
                owner->complete();
        }
    };
    using Inner = decltype(AsyncExecutionProtocol::connect(children(std::declval<Sender>(), std::declval<Timer>(),
                                                                    static_cast<Self*>(nullptr),
                                                                    std::declval<AsyncExecutionTimeoutState&>()),
                                                           Joined{}));
    Inner inner;

    struct Stop
    {
        Self* owner;
        void  operator()() const noexcept
        {
            if (owner->state.beginCallerStop())
            {
                owner->state.workStop.request_stop();
                owner->state.timerStop.request_stop();
                if (owner->state.endCallerStop())
                    owner->complete();
            }
        }
    };
    using Token    = decltype(AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver)));
    using Callback = typename Token::template callback_type<Stop>;
    AlignedStorage<sizeof(Callback), alignof(Callback)> callbackStorage;

    bool      registered = false;
    Callback* callback() noexcept { return reinterpret_cast<Callback*>(&callbackStorage); }

    AsyncExecutionTimeoutOperation(Sender sender, Timer timer, Receiver receiver)
        : receiver(move(receiver)),
          inner(AsyncExecutionProtocol::connect(children(move(sender), move(timer), this, state),
                                                Joined{this, &this->receiver}))
    {}
    AsyncExecutionTimeoutOperation(const AsyncExecutionTimeoutOperation&) = delete;
    ~AsyncExecutionTimeoutOperation()
    {
        SC_ASYNC_ASSERT_RELEASE(not registered);
        if (destroy)
            destroy(&output);
    }
    void start() noexcept
    {
        auto token = AsyncExecutionProtocol::get_stop_token(AsyncExecutionProtocol::get_env(receiver));
        placementNew(*callback(), token, Stop{this});
        registered = true;
        AsyncExecutionProtocol::start(inner);
    }
    void complete() noexcept
    {
        registered = false;
        callback()->~Callback();
        bool         stopped = false;
        const Result status  = state.outcome(stopped);
        if (stopped)
            AsyncExecutionProtocol::set_stopped(move(receiver));
        else if (not status)
            AsyncExecutionProtocol::set_error(move(receiver), status);
        else
            emit(&output, receiver);
        // Terminal delivery may destroy this operation and its owned value storage.
    }
};

template <typename Sender, typename Timer>
struct AsyncExecutionTimeoutSender
{
    using sender_concept = AsyncExecutionProtocol::sender_tag;
    Sender sender;
    Timer  timer;
    template <typename Self, typename... Environment>
    static consteval auto get_completion_signatures()
    {
        using Work =
            decltype(AsyncExecutionProtocol::write_env(std::declval<Sender>(), AsyncExecutionTimeoutStopEnvironment{}));
        using Signatures = decltype(AsyncExecutionProtocol::get_completion_signatures<Work, Environment...>());
        return typename AsyncExecutionTimeoutSignatures<Signatures>::Type{};
    }
    template <typename Receiver>
    auto connect(Receiver receiver) && noexcept
    {
        return AsyncExecutionTimeoutOperation<Sender, Timer, Receiver>(move(sender), move(timer), move(receiver));
    }
};
} // namespace detail

/// SC extension: request stop at a relative deadline and join both branches before delivering work values/error/stop.
/// Values use owned decayed storage. A reported work error takes precedence over deadline/caller cancellation.
template <typename Sender>
auto timeout(AsyncExecutionContext& context, Sender&& sender, TimeMs duration)
{
    auto timer = context.sleep(duration);
    return detail::AsyncExecutionTimeoutSender<std::decay_t<Sender>, decltype(timer)>{forward<Sender>(sender),
                                                                                      move(timer)};
}

} // namespace SC
#endif
