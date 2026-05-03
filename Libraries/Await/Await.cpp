// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Compiler.h"

#if defined(SC_COMPILER_ENABLE_STD_CPP) && SC_LANGUAGE_CPP_AT_LEAST_20

#include "../Foundation/Assert.h"
#include "Await.h"

namespace SC
{
AwaitTask::AwaitTask(Handle newHandle) : handle(newHandle) {}

AwaitTask::AwaitTask(AwaitTask&& other) noexcept : handle(other.handle) { other.handle = {}; }

AwaitTask& AwaitTask::operator=(AwaitTask&& other) noexcept
{
    if (this != &other)
    {
        SC_ASSERT_RELEASE(not isActive());
        destroy();
        handle       = other.handle;
        other.handle = {};
    }
    return *this;
}

AwaitTask::~AwaitTask()
{
    SC_ASSERT_RELEASE(not isActive());
    destroy();
}

bool AwaitTask::isValid() const { return handle != nullptr; }

bool AwaitTask::isStarted() const { return handle and handle.promise().started; }

bool AwaitTask::isCompleted() const { return handle and handle.promise().completed; }

bool AwaitTask::isActive() const { return isStarted() and not isCompleted(); }

Result AwaitTask::result() const
{
    if (not handle)
    {
        return Result::Error("AwaitTask is invalid");
    }
    if (not isCompleted())
    {
        return Result::Error("AwaitTask is not completed");
    }
    return handle.promise().taskResult;
}

Result AwaitTask::start()
{
    if (not handle)
    {
        return Result::Error("AwaitTask is invalid");
    }
    if (handle.promise().started)
    {
        return Result::Error("AwaitTask already started");
    }
    handle.promise().started = true;
    return Result(true);
}

void AwaitTask::resume()
{
    SC_ASSERT_RELEASE(handle != nullptr);
    handle.resume();
}

void AwaitTask::destroy()
{
    if (handle)
    {
        handle.destroy();
        handle = {};
    }
}

AwaitTask::Promise::Promise() : taskResult(Result::Error("AwaitTask not completed")), started(false), completed(false)
{}

AwaitTask AwaitTask::Promise::get_return_object() { return AwaitTask(AwaitTask::Handle::from_promise(*this)); }

AwaitSuspendAlways AwaitTask::Promise::initial_suspend() noexcept { return {}; }

bool AwaitTask::Promise::FinalSuspend::await_ready() noexcept { return false; }

void AwaitTask::Promise::FinalSuspend::await_suspend(AwaitTask::Handle handle) noexcept
{
    handle.promise().completed = true;
}

void AwaitTask::Promise::FinalSuspend::await_resume() noexcept {}

AwaitTask::Promise::FinalSuspend AwaitTask::Promise::final_suspend() noexcept { return {}; }

void AwaitTask::Promise::return_value(Result newResult) noexcept { taskResult = newResult; }

void AwaitTask::Promise::unhandled_exception() noexcept { taskResult = Result::Error("AwaitTask unhandled exception"); }

AwaitEventLoop::AwaitEventLoop(AsyncEventLoop& asyncEventLoop) : eventLoop(asyncEventLoop) {}

AsyncEventLoop& AwaitEventLoop::asyncEventLoop() { return eventLoop; }

const AsyncEventLoop& AwaitEventLoop::asyncEventLoop() const { return eventLoop; }

Result AwaitEventLoop::spawn(AwaitTask& task)
{
    SC_TRY(task.start());
    task.resume();
    return Result(true);
}

Result AwaitEventLoop::run() { return eventLoop.run(); }

Result AwaitEventLoop::runOnce() { return eventLoop.runOnce(); }

Result AwaitEventLoop::runNoWait() { return eventLoop.runNoWait(); }

AwaitSleepAwaiter AwaitEventLoop::sleep(TimeMs duration) { return {*this, duration}; }

AwaitSocketSendAwaiter AwaitEventLoop::send(const SocketDescriptor& socket, Span<const char> data,
                                            AwaitSocketSendResult* outResult)
{
    return {*this, socket, data, outResult};
}

AwaitSocketReceiveAwaiter AwaitEventLoop::receive(const SocketDescriptor& socket, Span<char> buffer,
                                                  AwaitSocketReceiveResult& outResult)
{
    return {*this, socket, buffer, outResult};
}

bool AwaitSleepAwaiter::await_ready() const { return false; }

bool AwaitSleepAwaiter::await_suspend(AwaitCoroutineHandle continuation)
{
    request.callback = [this, continuation](AsyncLoopTimeout::Result& result)
    {
        operationResult = result.isValid();
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), duration);
    return operationResult;
}

Result AwaitSleepAwaiter::await_resume() { return operationResult; }

bool AwaitSocketSendAwaiter::await_ready() const { return false; }

bool AwaitSocketSendAwaiter::await_suspend(AwaitCoroutineHandle continuation)
{
    request.callback = [this, continuation](AsyncSocketSend::Result& result)
    {
        operationResult = result.isValid();
        if (outResult != nullptr)
        {
            outResult->numBytes = result.completionData.numBytes;
        }
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, data);
    return operationResult;
}

Result AwaitSocketSendAwaiter::await_resume() { return operationResult; }

bool AwaitSocketReceiveAwaiter::await_ready() const { return false; }

bool AwaitSocketReceiveAwaiter::await_suspend(AwaitCoroutineHandle continuation)
{
    outResult        = {};
    request.callback = [this, continuation](AsyncSocketReceive::Result& result)
    {
        operationResult        = result.get(outResult.data);
        outResult.disconnected = result.completionData.disconnected;
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, buffer);
    return operationResult;
}

Result AwaitSocketReceiveAwaiter::await_resume() { return operationResult; }
} // namespace SC
#endif
