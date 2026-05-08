// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Compiler.h"

#if defined(SC_COMPILER_ENABLE_STD_CPP) && SC_LANGUAGE_CPP_AT_LEAST_20

#include "../Foundation/Assert.h"
#include "Await.h"

namespace SC
{
namespace
{
struct AwaitAllocationHeader
{
    AwaitArena* arena      = nullptr;
    void*       allocation = nullptr;
};

static constexpr Result AwaitTaskCancelled() { return Result::Error("AwaitTask cancelled"); }

static constexpr size_t AwaitFrameAlignment =
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
    __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
    alignof(void*);
#endif

static void* alignPointer(void* pointer, size_t alignment)
{
    const size_t address        = reinterpret_cast<size_t>(pointer);
    const size_t alignedAddress = (address + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(alignedAddress);
}

static AwaitAllocationHeader& allocationHeaderFromFrame(void* frame)
{
    return *(reinterpret_cast<AwaitAllocationHeader*>(frame) - 1);
}
} // namespace

AwaitArena::AwaitArena(Span<char> memory) : storage(memory) {}

void* AwaitArena::allocate(size_t size, size_t alignment)
{
    char*  base           = storage.data();
    void*  current        = base + offset;
    void*  aligned        = alignPointer(current, alignment);
    size_t alignedOffset  = static_cast<size_t>(static_cast<char*>(aligned) - base);
    size_t requiredOffset = alignedOffset + size;
    if (requiredOffset > storage.sizeInBytes())
    {
        return nullptr;
    }
    offset = requiredOffset;
    return aligned;
}

void AwaitArena::reset() { offset = 0; }

size_t AwaitArena::used() const { return offset; }

size_t AwaitArena::capacity() const { return storage.sizeInBytes(); }

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

bool AwaitTask::isCancellationRequested() const { return handle and handle.promise().cancellationRequested; }

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

Result AwaitTask::cancel(AwaitEventLoop& await)
{
    if (not handle)
    {
        return Result::Error("AwaitTask is invalid");
    }
    Promise& promise = handle.promise();
    if (promise.completed)
    {
        return Result(true);
    }
    if (not promise.started)
    {
        return Result::Error("AwaitTask is not started");
    }
    if (promise.cancellationRequested)
    {
        return Result(true);
    }
    if (promise.cancellation.cancel == nullptr)
    {
        return Result::Error("AwaitTask cannot be cancelled right now");
    }
    promise.cancellationRequested = true;
    return promise.cancellation.cancel(promise.cancellation.object, await);
}

bool AwaitTask::await_ready() const { return not isActive(); }

bool AwaitTask::await_suspend(Handle parent)
{
    if (not isActive())
    {
        return false;
    }

    Promise& child = handle.promise();
    SC_ASSERT_RELEASE(child.continuation == nullptr);
    child.continuation = parent;

    parent.promise().cancellation = {this, [](void* object, AwaitEventLoop& await)
                                     { return static_cast<AwaitTask*>(object)->cancel(await); }};
    return true;
}

Result AwaitTask::await_resume() const { return result(); }

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

AwaitTask::Promise::Promise()
    : taskResult(Result::Error("AwaitTask not completed")), eventLoop(nullptr), started(false), completed(false)
{
    cancellationRequested = false;
}

AwaitEventLoop* AwaitTask::Promise::findEventLoop() { return nullptr; }

AwaitEventLoop* AwaitTask::Promise::findEventLoop(AwaitEventLoop& await) { return &await; }

void* AwaitTask::Promise::allocateFrame(size_t size, AwaitEventLoop* eventLoop) noexcept
{
    constexpr size_t alignment = AwaitFrameAlignment;
    const size_t     totalSize = size + sizeof(AwaitAllocationHeader) + alignment;

    AwaitArena* arena = eventLoop == nullptr ? nullptr : eventLoop->arena();
    void*       raw   = nullptr;
    if (arena != nullptr)
    {
        raw = arena->allocate(totalSize, alignof(AwaitAllocationHeader));
    }
    else
    {
        raw = ::operator new(totalSize, std::nothrow);
    }

    if (raw == nullptr)
    {
        return nullptr;
    }

    void* frame = alignPointer(static_cast<char*>(raw) + sizeof(AwaitAllocationHeader), alignment);

    AwaitAllocationHeader& header = allocationHeaderFromFrame(frame);
    header.arena                  = arena;
    header.allocation             = raw;
    return frame;
}

void AwaitTask::Promise::deallocateFrame(void* frame) noexcept
{
    if (frame == nullptr)
    {
        return;
    }

    AwaitAllocationHeader& header = allocationHeaderFromFrame(frame);
    if (header.arena == nullptr)
    {
        ::operator delete(header.allocation);
    }
}

void* AwaitTask::Promise::operator new(size_t size) noexcept { return allocateFrame(size, nullptr); }

void AwaitTask::Promise::operator delete(void* frame, size_t) noexcept { deallocateFrame(frame); }

AwaitTask AwaitTask::Promise::get_return_object_on_allocation_failure() { return {}; }

AwaitTask AwaitTask::Promise::get_return_object() { return AwaitTask(AwaitTask::Handle::from_promise(*this)); }

AwaitSuspendAlways AwaitTask::Promise::initial_suspend() noexcept { return {}; }

bool AwaitTask::Promise::FinalSuspend::await_ready() noexcept { return false; }

void AwaitTask::Promise::FinalSuspend::await_suspend(AwaitTask::Handle handle) noexcept
{
    AwaitTask::Promise& promise = handle.promise();
    promise.completed           = true;
    promise.cancellation        = {};
    if (promise.continuation != nullptr)
    {
        AwaitTask::Handle parentContinuation      = promise.continuation;
        promise.continuation                      = {};
        parentContinuation.promise().cancellation = {};
        parentContinuation.resume();
    }
}

void AwaitTask::Promise::FinalSuspend::await_resume() noexcept {}

AwaitTask::Promise::FinalSuspend AwaitTask::Promise::final_suspend() noexcept { return {}; }

void AwaitTask::Promise::return_value(Result newResult) noexcept { taskResult = newResult; }

void AwaitTask::Promise::unhandled_exception() noexcept { taskResult = Result::Error("AwaitTask unhandled exception"); }

AwaitEventLoop::AwaitEventLoop(AsyncEventLoop& asyncEventLoop, AwaitArena* arena)
    : eventLoop(asyncEventLoop), frameArena(arena)
{}

AsyncEventLoop& AwaitEventLoop::asyncEventLoop() { return eventLoop; }

const AsyncEventLoop& AwaitEventLoop::asyncEventLoop() const { return eventLoop; }

AwaitArena* AwaitEventLoop::arena() { return frameArena; }

Result AwaitEventLoop::spawn(AwaitTask& task)
{
    if (not task.isValid())
    {
        return Result::Error("AwaitTask is invalid");
    }
    AwaitEventLoop* taskEventLoop = task.handle.promise().eventLoop;
    if (taskEventLoop != nullptr and taskEventLoop != this)
    {
        return Result::Error("AwaitTask belongs to another AwaitEventLoop");
    }

    SC_TRY(task.start());
    task.resume();
    return Result(true);
}

Result AwaitEventLoop::run() { return eventLoop.run(); }

Result AwaitEventLoop::runOnce() { return eventLoop.runOnce(); }

Result AwaitEventLoop::runNoWait() { return eventLoop.runNoWait(); }

AwaitSleepAwaiter AwaitEventLoop::sleep(TimeMs duration) { return AwaitSleepAwaiter(*this, duration); }

AwaitSocketAcceptAwaiter AwaitEventLoop::accept(const SocketDescriptor& serverSocket, SocketDescriptor& outClient)
{
    return AwaitSocketAcceptAwaiter(*this, serverSocket, outClient);
}

AwaitSocketSendAwaiter AwaitEventLoop::send(const SocketDescriptor& socket, Span<const char> data,
                                            AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendAwaiter(*this, socket, data, outResult);
}

AwaitSocketSendAllAwaiter AwaitEventLoop::sendAll(const SocketDescriptor& socket, Span<const char> data,
                                                  AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendAllAwaiter(*this, socket, data, outResult);
}

AwaitSocketReceiveAwaiter AwaitEventLoop::receive(const SocketDescriptor& socket, Span<char> buffer,
                                                  AwaitSocketReceiveResult& outResult)
{
    return AwaitSocketReceiveAwaiter(*this, socket, buffer, outResult);
}

AwaitSleepAwaiter::AwaitSleepAwaiter(AwaitEventLoop& await, TimeMs duration) : await(await), duration(duration) {}

bool AwaitSleepAwaiter::await_ready() const { return false; }

bool AwaitSleepAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitSleepAwaiter::cancel};

    request.callback = [this](AsyncLoopTimeout::Result& result)
    {
        operationResult = result.isValid();
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), duration);
    return operationResult;
}

Result AwaitSleepAwaiter::await_resume()
{
    clearCancellation();
    return operationResult;
}

Result AwaitSleepAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSleepAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSleepAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

void AwaitSleepAwaiter::clearCancellation()
{
    if (continuation == nullptr)
    {
        return;
    }
    AwaitTask::Promise& promise = continuation.promise();
    if (promise.cancellation.object == this)
    {
        promise.cancellation = {};
    }
}

AwaitSocketAcceptAwaiter::AwaitSocketAcceptAwaiter(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                                                   SocketDescriptor& outClient)
    : await(await), serverSocket(serverSocket), outClient(outClient)
{}

bool AwaitSocketAcceptAwaiter::await_ready() const { return false; }

bool AwaitSocketAcceptAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitSocketAcceptAwaiter::cancel};

    request.callback = [this](AsyncSocketAccept::Result& result)
    {
        operationResult = result.moveTo(outClient);
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), serverSocket);
    return operationResult;
}

Result AwaitSocketAcceptAwaiter::await_resume()
{
    clearCancellation();
    return operationResult;
}

Result AwaitSocketAcceptAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketAcceptAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketAcceptAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

void AwaitSocketAcceptAwaiter::clearCancellation()
{
    if (continuation == nullptr)
    {
        return;
    }
    AwaitTask::Promise& promise = continuation.promise();
    if (promise.cancellation.object == this)
    {
        promise.cancellation = {};
    }
}

AwaitSocketSendAwaiter::AwaitSocketSendAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                               Span<const char> data, AwaitSocketSendResult* outResult)
    : await(await), socket(socket), data(data), outResult(outResult)
{}

bool AwaitSocketSendAwaiter::await_ready() const { return false; }

bool AwaitSocketSendAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitSocketSendAwaiter::cancel};

    request.callback = [this](AsyncSocketSend::Result& result)
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

Result AwaitSocketSendAwaiter::await_resume()
{
    clearCancellation();
    return operationResult;
}

Result AwaitSocketSendAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketSendAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketSendAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

void AwaitSocketSendAwaiter::clearCancellation()
{
    if (continuation == nullptr)
    {
        return;
    }
    AwaitTask::Promise& promise = continuation.promise();
    if (promise.cancellation.object == this)
    {
        promise.cancellation = {};
    }
}

AwaitSocketSendAllAwaiter::AwaitSocketSendAllAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                     Span<const char> data, AwaitSocketSendResult* outResult)
    : await(await), socket(socket), data(data), outResult(outResult)
{}

bool AwaitSocketSendAllAwaiter::await_ready() const { return data.empty(); }

bool AwaitSocketSendAllAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitSocketSendAllAwaiter::cancel};

    request.callback = [this](AsyncSocketSend::Result& result)
    {
        operationResult = result.isValid();
        if (operationResult)
        {
            const size_t bytesSent = result.completionData.numBytes;
            if (bytesSent == 0)
            {
                operationResult = Result::Error("AwaitSocketSendAll made no progress");
                continuation.resume();
                return;
            }

            numBytesSent += bytesSent;
            if (outResult != nullptr)
            {
                outResult->numBytes = numBytesSent;
            }

            if (numBytesSent < data.sizeInBytes())
            {
                Span<const char> remaining;
                operationResult = Result(data.sliceStart(numBytesSent, remaining));
                if (operationResult)
                {
                    request.buffer = remaining;
                    result.reactivateRequest(true);
                    return;
                }
            }
        }
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, data);
    return operationResult;
}

Result AwaitSocketSendAllAwaiter::await_resume()
{
    clearCancellation();
    if (outResult != nullptr and operationResult)
    {
        outResult->numBytes = numBytesSent;
    }
    return operationResult;
}

Result AwaitSocketSendAllAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketSendAllAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketSendAllAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

void AwaitSocketSendAllAwaiter::clearCancellation()
{
    if (continuation == nullptr)
    {
        return;
    }
    AwaitTask::Promise& promise = continuation.promise();
    if (promise.cancellation.object == this)
    {
        promise.cancellation = {};
    }
}

AwaitSocketReceiveAwaiter::AwaitSocketReceiveAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                     Span<char> buffer, AwaitSocketReceiveResult& outResult)
    : await(await), socket(socket), buffer(buffer), outResult(outResult)
{}

bool AwaitSocketReceiveAwaiter::await_ready() const { return false; }

bool AwaitSocketReceiveAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitSocketReceiveAwaiter::cancel};

    outResult        = {};
    request.callback = [this](AsyncSocketReceive::Result& result)
    {
        operationResult        = result.get(outResult.data);
        outResult.disconnected = result.completionData.disconnected;
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, buffer);
    return operationResult;
}

Result AwaitSocketReceiveAwaiter::await_resume()
{
    clearCancellation();
    return operationResult;
}

Result AwaitSocketReceiveAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketReceiveAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketReceiveAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

void AwaitSocketReceiveAwaiter::clearCancellation()
{
    if (continuation == nullptr)
    {
        return;
    }
    AwaitTask::Promise& promise = continuation.promise();
    if (promise.cancellation.object == this)
    {
        promise.cancellation = {};
    }
}
} // namespace SC
#endif
