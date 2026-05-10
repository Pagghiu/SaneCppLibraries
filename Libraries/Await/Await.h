// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/Compiler.h"
#ifndef SC_EXPORT_LIBRARY_AWAIT
#define SC_EXPORT_LIBRARY_AWAIT 0
#endif
#define SC_AWAIT_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_AWAIT)

#include "../Async/Async.h"
#include "../Foundation/Result.h"
#include "Internal/AwaitCoroutine.h"

//! @defgroup group_await Await
//! @copybrief library_await (see @ref library_await for more details)
//! Draft C++20 coroutine layer over SC::AsyncEventLoop.
//!
//! Await is an experimental wrapper that lets coroutine bodies express asynchronous operations with `co_await` while
//! still returning plain SC::Result values and using caller-provided output objects.

//! @addtogroup group_await
//! @{
namespace SC
{
struct AwaitEventLoop;
struct AwaitArena;
struct AwaitCancellationHandler;
struct AwaitSleepAwaiter;
struct AwaitSocketAcceptAwaiter;
struct AwaitSocketConnectAwaiter;
struct AwaitSocketSendAwaiter;
struct AwaitSocketSendToAwaiter;
struct AwaitSocketSendAllAwaiter;
struct AwaitSocketReceiveAwaiter;
struct AwaitSocketReceiveFromAwaiter;
struct AwaitFileReadAwaiter;
struct AwaitFileWriteAwaiter;

/// @brief Checks the Result of a coroutine await expression and co_return-s the error to the caller.
#define SC_CO_TRY(expression)                                                                                          \
    {                                                                                                                  \
        if (auto _exprResConv = SC::Result(expression))                                                                \
            SC_LANGUAGE_LIKELY { (void)0; }                                                                            \
        else                                                                                                           \
        {                                                                                                              \
            co_return _exprResConv;                                                                                    \
        }                                                                                                              \
    }

struct AwaitSocketSendResult
{
    size_t numBytes = 0;
};

/// @brief Result object populated by AwaitEventLoop::receive.
struct AwaitSocketReceiveResult
{
    Span<char> data;
    bool       disconnected = false;
};

/// @brief Result object populated by AwaitEventLoop::receiveFrom.
struct AwaitSocketReceiveFromResult
{
    Span<char>      data;
    SocketIPAddress sourceAddress;
    bool            disconnected = false;
};

/// @brief Result object populated by AwaitEventLoop::fileRead.
struct AwaitFileReadResult
{
    Span<char> data;
    bool       endOfFile = false;
};

struct AwaitFileWriteResult
{
    size_t numBytes = 0;
};

/// @brief Cancellation hook installed by the awaiter currently suspending an AwaitTask.
struct AwaitCancellationHandler
{
    void* object = nullptr;

    Result (*cancel)(void* object, AwaitEventLoop& eventLoop) = nullptr;
};

/// @brief Caller-owned monotonic arena for coroutine frame allocation.
struct SC_AWAIT_EXPORT AwaitArena
{
    explicit AwaitArena(Span<char> memory);

    [[nodiscard]] void* allocate(size_t size, size_t alignment);
    void                reset();

    [[nodiscard]] size_t used() const;
    [[nodiscard]] size_t capacity() const;

  private:
    Span<char> storage;
    size_t     offset = 0;
};

/// @brief Caller-owned coroutine task returning a plain SC::Result.
struct SC_AWAIT_EXPORT AwaitTask
{
    struct Promise;
    using promise_type = Promise;
    using Handle       = AwaitCoroutineTypedHandle<Promise>;

    AwaitTask() = default;
    explicit AwaitTask(Handle newHandle);

    AwaitTask(const AwaitTask&)            = delete;
    AwaitTask& operator=(const AwaitTask&) = delete;

    AwaitTask(AwaitTask&& other) noexcept;
    AwaitTask& operator=(AwaitTask&& other) noexcept;
    ~AwaitTask();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isStarted() const;
    [[nodiscard]] bool isCompleted() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] bool isCancellationRequested() const;

    [[nodiscard]] Result result() const;

    Result cancel(AwaitEventLoop& await);

    bool   await_ready() const;
    bool   await_suspend(Handle continuation);
    Result await_resume() const;

  private:
    friend struct AwaitEventLoop;

    Result start();
    void   resume();
    void   destroy();

    Handle handle = {};
};

/// @brief Coroutine promise implementation used by AwaitTask.
struct SC_AWAIT_EXPORT AwaitTask::Promise
{
    Promise();

    template <typename First, typename... Rest>
    Promise(First& first, Rest&... rest) : Promise()
    {
        eventLoop = findEventLoop(first, rest...);
    }

    static AwaitEventLoop* findEventLoop();
    static AwaitEventLoop* findEventLoop(AwaitEventLoop& await);

    template <typename First, typename... Rest>
    static AwaitEventLoop* findEventLoop(First&, Rest&... rest)
    {
        return findEventLoop(rest...);
    }

    static void* allocateFrame(size_t size, AwaitEventLoop* eventLoop) noexcept;
    static void  deallocateFrame(void* frame) noexcept;

    static void* operator new(size_t size) noexcept;

    template <typename First, typename... Rest>
    static void* operator new(size_t size, First& first, Rest&... rest) noexcept
    {
        return allocateFrame(size, findEventLoop(first, rest...));
    }

    static void operator delete(void* frame, size_t) noexcept;

    static AwaitTask get_return_object_on_allocation_failure();

    AwaitTask get_return_object();

    AwaitSuspendAlways initial_suspend() noexcept;

    struct FinalSuspend
    {
        bool await_ready() noexcept;
        void await_suspend(AwaitTask::Handle handle) noexcept;
        void await_resume() noexcept;
    };

    FinalSuspend final_suspend() noexcept;

    void return_value(Result newResult) noexcept;

    void unhandled_exception() noexcept;

    Result taskResult;

    AwaitTask::Handle        continuation;
    AwaitCancellationHandler cancellation;
    AwaitEventLoop*          eventLoop;

    bool started;
    bool completed;
    bool cancellationRequested;
};

/// @brief Coroutine-friendly wrapper around an existing AsyncEventLoop.
struct SC_AWAIT_EXPORT AwaitEventLoop
{
    explicit AwaitEventLoop(AsyncEventLoop& asyncEventLoop, AwaitArena* arena = nullptr);

    [[nodiscard]] AsyncEventLoop&       asyncEventLoop();
    [[nodiscard]] const AsyncEventLoop& asyncEventLoop() const;
    [[nodiscard]] AwaitArena*           arena();

    Result spawn(AwaitTask& task);

    Result run();
    Result runOnce();
    Result runNoWait();

    AwaitSleepAwaiter             sleep(TimeMs duration);
    AwaitSocketAcceptAwaiter      accept(const SocketDescriptor& serverSocket, SocketDescriptor& outClient);
    AwaitSocketConnectAwaiter     connect(const SocketDescriptor& socket, SocketIPAddress address);
    AwaitSocketSendAwaiter        send(const SocketDescriptor& socket, Span<const char> data,
                                       AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketSendToAwaiter      sendTo(const SocketDescriptor& socket, SocketIPAddress address, Span<const char> data,
                                         AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketSendAllAwaiter     sendAll(const SocketDescriptor& socket, Span<const char> data,
                                          AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketReceiveAwaiter     receive(const SocketDescriptor& socket, Span<char> buffer,
                                          AwaitSocketReceiveResult& outResult);
    AwaitSocketReceiveFromAwaiter receiveFrom(const SocketDescriptor& socket, Span<char> buffer,
                                              AwaitSocketReceiveFromResult& outResult);
    AwaitFileReadAwaiter  fileRead(const FileDescriptor& file, Span<char> buffer, AwaitFileReadResult& outResult);
    AwaitFileWriteAwaiter fileWrite(const FileDescriptor& file, Span<const char> data,
                                    AwaitFileWriteResult* outResult = nullptr);

  private:
    AsyncEventLoop& eventLoop;
    AwaitArena*     frameArena;
};

/// @brief Awaiter for a single AsyncLoopTimeout operation.
struct SC_AWAIT_EXPORT AwaitSleepAwaiter
{
    AwaitSleepAwaiter(AwaitEventLoop& await, TimeMs duration);

    AwaitEventLoop&  await;
    TimeMs           duration;
    AsyncLoopTimeout request;
    Result           operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketAccept operation.
struct SC_AWAIT_EXPORT AwaitSocketAcceptAwaiter
{
    AwaitSocketAcceptAwaiter(AwaitEventLoop& await, const SocketDescriptor& serverSocket, SocketDescriptor& outClient);

    AwaitEventLoop&         await;
    const SocketDescriptor& serverSocket;
    SocketDescriptor&       outClient;
    AsyncSocketAccept       request;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketConnect operation.
struct SC_AWAIT_EXPORT AwaitSocketConnectAwaiter
{
    AwaitSocketConnectAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, SocketIPAddress address);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    SocketIPAddress         address;
    AsyncSocketConnect      request;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketSend operation.
struct SC_AWAIT_EXPORT AwaitSocketSendAwaiter
{
    AwaitSocketSendAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<const char> data,
                           AwaitSocketSendResult* outResult);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    Span<const char>        data;
    AwaitSocketSendResult*  outResult = nullptr;
    AsyncSocketSend         request;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketSendTo operation.
struct SC_AWAIT_EXPORT AwaitSocketSendToAwaiter
{
    AwaitSocketSendToAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, SocketIPAddress address,
                             Span<const char> data, AwaitSocketSendResult* outResult);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    SocketIPAddress         address;
    Span<const char>        data;
    AwaitSocketSendResult*  outResult = nullptr;
    AsyncSocketSendTo       request;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter that reactivates AsyncSocketSend until the whole buffer is sent.
struct SC_AWAIT_EXPORT AwaitSocketSendAllAwaiter
{
    AwaitSocketSendAllAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<const char> data,
                              AwaitSocketSendResult* outResult);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    Span<const char>        data;
    AwaitSocketSendResult*  outResult = nullptr;
    AsyncSocketSend         request;
    Result                  operationResult = Result(true);
    size_t                  numBytesSent    = 0;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketReceive operation.
struct SC_AWAIT_EXPORT AwaitSocketReceiveAwaiter
{
    AwaitSocketReceiveAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<char> buffer,
                              AwaitSocketReceiveResult& outResult);

    AwaitEventLoop&           await;
    const SocketDescriptor&   socket;
    Span<char>                buffer;
    AwaitSocketReceiveResult& outResult;
    AsyncSocketReceive        request;
    Result                    operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketReceiveFrom operation.
struct SC_AWAIT_EXPORT AwaitSocketReceiveFromAwaiter
{
    AwaitSocketReceiveFromAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<char> buffer,
                                  AwaitSocketReceiveFromResult& outResult);

    AwaitEventLoop&               await;
    const SocketDescriptor&       socket;
    Span<char>                    buffer;
    AwaitSocketReceiveFromResult& outResult;
    AsyncSocketReceiveFrom        request;
    Result                        operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncFileRead operation.
struct SC_AWAIT_EXPORT AwaitFileReadAwaiter
{
    AwaitFileReadAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<char> buffer,
                         AwaitFileReadResult& outResult);

    AwaitEventLoop&       await;
    const FileDescriptor& file;
    Span<char>            buffer;
    AwaitFileReadResult&  outResult;
    AsyncFileRead         request;
    Result                operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncFileWrite operation.
struct SC_AWAIT_EXPORT AwaitFileWriteAwaiter
{
    AwaitFileWriteAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<const char> data,
                          AwaitFileWriteResult* outResult);

    AwaitEventLoop&       await;
    const FileDescriptor& file;
    Span<const char>      data;
    AwaitFileWriteResult* outResult = nullptr;
    AsyncFileWrite        request;
    Result                operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    void   clearCancellation();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};
} // namespace SC
//! @}
