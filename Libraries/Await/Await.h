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

//! @addtogroup group_await Await
//! @{
namespace SC
{
struct AwaitEventLoop;
struct AwaitArena;
struct AwaitCancellationHandler;
struct AwaitSleepAwaiter;
struct AwaitSocketAcceptAwaiter;
struct AwaitSocketSendAwaiter;
struct AwaitSocketSendAllAwaiter;
struct AwaitSocketReceiveAwaiter;

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

struct AwaitSocketReceiveResult
{
    Span<char> data;
    bool       disconnected = false;
};

struct AwaitCancellationHandler
{
    void* object = nullptr;

    Result (*cancel)(void* object, AwaitEventLoop& eventLoop) = nullptr;
};

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

struct SC_AWAIT_EXPORT AwaitTask::Promise
{
    Promise();

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

    bool started;
    bool completed;
    bool cancellationRequested;
};

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

    AwaitSleepAwaiter         sleep(TimeMs duration);
    AwaitSocketAcceptAwaiter  accept(const SocketDescriptor& serverSocket, SocketDescriptor& outClient);
    AwaitSocketSendAwaiter    send(const SocketDescriptor& socket, Span<const char> data,
                                   AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketSendAllAwaiter sendAll(const SocketDescriptor& socket, Span<const char> data,
                                      AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketReceiveAwaiter receive(const SocketDescriptor& socket, Span<char> buffer,
                                      AwaitSocketReceiveResult& outResult);

  private:
    AsyncEventLoop& eventLoop;
    AwaitArena*     frameArena;
};

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
} // namespace SC
//! @}
