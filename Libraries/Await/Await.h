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
struct AwaitSleepAwaiter;
struct AwaitSocketSendAwaiter;
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

    [[nodiscard]] Result result() const;

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
    bool   started;
    bool   completed;
};

struct SC_AWAIT_EXPORT AwaitEventLoop
{
    explicit AwaitEventLoop(AsyncEventLoop& asyncEventLoop);

    [[nodiscard]] AsyncEventLoop&       asyncEventLoop();
    [[nodiscard]] const AsyncEventLoop& asyncEventLoop() const;

    Result spawn(AwaitTask& task);

    Result run();
    Result runOnce();
    Result runNoWait();

    AwaitSleepAwaiter         sleep(TimeMs duration);
    AwaitSocketSendAwaiter    send(const SocketDescriptor& socket, Span<const char> data,
                                   AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketReceiveAwaiter receive(const SocketDescriptor& socket, Span<char> buffer,
                                      AwaitSocketReceiveResult& outResult);

  private:
    AsyncEventLoop& eventLoop;
};

struct SC_AWAIT_EXPORT AwaitSleepAwaiter
{
    AwaitEventLoop&  await;
    TimeMs           duration;
    AsyncLoopTimeout request;
    Result           operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitCoroutineHandle continuation);
    Result await_resume();
};

struct SC_AWAIT_EXPORT AwaitSocketSendAwaiter
{
    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    Span<const char>        data;
    AwaitSocketSendResult*  outResult = nullptr;
    AsyncSocketSend         request;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitCoroutineHandle continuation);
    Result await_resume();
};

struct SC_AWAIT_EXPORT AwaitSocketReceiveAwaiter
{
    AwaitEventLoop&           await;
    const SocketDescriptor&   socket;
    Span<char>                buffer;
    AwaitSocketReceiveResult& outResult;
    AsyncSocketReceive        request;
    Result                    operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitCoroutineHandle continuation);
    Result await_resume();
};
} // namespace SC
//! @}
