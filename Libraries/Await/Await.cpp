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

static void clearCancellation(AwaitTask::Handle continuation, void* object)
{
    if (continuation == nullptr)
    {
        return;
    }
    AwaitTask::Promise& promise = continuation.promise();
    if (promise.cancellation.object == object)
    {
        promise.cancellation = {};
    }
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
    completionObject      = nullptr;
    completionCallback    = nullptr;
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
    if (promise.completionCallback != nullptr)
    {
        void (*callback)(void*)    = promise.completionCallback;
        void* callbackObject       = promise.completionObject;
        promise.completionObject   = nullptr;
        promise.completionCallback = nullptr;
        callback(callbackObject);
        return;
    }
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

AwaitSocketConnectAwaiter AwaitEventLoop::connect(const SocketDescriptor& socket, SocketIPAddress address)
{
    return AwaitSocketConnectAwaiter(*this, socket, address);
}

AwaitSocketSendAwaiter AwaitEventLoop::send(const SocketDescriptor& socket, Span<const char> data,
                                            AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendAwaiter(*this, socket, data, outResult);
}

AwaitSocketSendToAwaiter AwaitEventLoop::sendTo(const SocketDescriptor& socket, SocketIPAddress address,
                                                Span<const char> data, AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendToAwaiter(*this, socket, address, data, outResult);
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

AwaitSocketReceiveFromAwaiter AwaitEventLoop::receiveFrom(const SocketDescriptor& socket, Span<char> buffer,
                                                          AwaitSocketReceiveFromResult& outResult)
{
    return AwaitSocketReceiveFromAwaiter(*this, socket, buffer, outResult);
}

AwaitFileReadAwaiter AwaitEventLoop::fileRead(const FileDescriptor& file, Span<char> buffer,
                                              AwaitFileReadResult& outResult)
{
    return AwaitFileReadAwaiter(*this, file, buffer, outResult);
}

AwaitFileWriteAwaiter AwaitEventLoop::fileWrite(const FileDescriptor& file, Span<const char> data,
                                                AwaitFileWriteResult* outResult)
{
    return AwaitFileWriteAwaiter(*this, file, data, outResult);
}

AwaitFileSendAwaiter AwaitEventLoop::fileSend(const FileDescriptor& file, const SocketDescriptor& socket,
                                              AwaitFileSendResult& outResult, AwaitFileSendOptions options)
{
    return AwaitFileSendAwaiter(*this, file, socket, outResult, options);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsOpen(ThreadPool& threadPool, StringSpan path, FileOpen mode,
                                                       FileDescriptor& outFile)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::Open, path, StringSpan(),
                                           mode, &outFile);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsClose(ThreadPool& threadPool, FileDescriptor& file)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::Close, file);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsCopyFile(ThreadPool& threadPool, StringSpan path,
                                                           StringSpan destinationPath, FileSystemCopyFlags copyFlags)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::CopyFile, path,
                                           destinationPath, FileOpen(), nullptr, copyFlags);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsCopyDirectory(ThreadPool& threadPool, StringSpan path,
                                                                StringSpan          destinationPath,
                                                                FileSystemCopyFlags copyFlags)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::CopyDirectory, path,
                                           destinationPath, FileOpen(), nullptr, copyFlags);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsRename(ThreadPool& threadPool, StringSpan path, StringSpan newPath)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::Rename, path, newPath);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsRemoveEmptyDirectory(ThreadPool& threadPool, StringSpan path)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::RemoveEmptyDirectory, path);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsRemoveFile(ThreadPool& threadPool, StringSpan path)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::RemoveFile, path);
}

AwaitTaskTimeoutAwaiter AwaitEventLoop::waitFor(AwaitTask& task, TimeMs timeout, AwaitTimeoutResult* outResult)
{
    return AwaitTaskTimeoutAwaiter(*this, task, timeout, outResult);
}

AwaitLoopWorkAwaiter AwaitEventLoop::loopWork(ThreadPool& threadPool, Function<Result()> work)
{
    return AwaitLoopWorkAwaiter(*this, threadPool, move(work));
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
    clearCancellation(continuation, this);
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
    clearCancellation(continuation, this);
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

AwaitSocketConnectAwaiter::AwaitSocketConnectAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                     SocketIPAddress address)
    : await(await), socket(socket), address(address)
{}

bool AwaitSocketConnectAwaiter::await_ready() const { return false; }

bool AwaitSocketConnectAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitSocketConnectAwaiter::cancel};

    request.callback = [this](AsyncSocketConnect::Result& result)
    {
        operationResult = result.isValid();
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, address);
    return operationResult;
}

Result AwaitSocketConnectAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketConnectAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketConnectAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketConnectAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
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
    clearCancellation(continuation, this);
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

AwaitSocketSendToAwaiter::AwaitSocketSendToAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                   SocketIPAddress address, Span<const char> data,
                                                   AwaitSocketSendResult* outResult)
    : await(await), socket(socket), address(address), data(data), outResult(outResult)
{}

bool AwaitSocketSendToAwaiter::await_ready() const { return false; }

bool AwaitSocketSendToAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitSocketSendToAwaiter::cancel};

    request.callback = [this](AsyncSocketSendTo::Result& result)
    {
        operationResult = result.isValid();
        if (outResult != nullptr)
        {
            outResult->numBytes = result.completionData.numBytes;
        }
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, address, data);
    return operationResult;
}

Result AwaitSocketSendToAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketSendToAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketSendToAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketSendToAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
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
    clearCancellation(continuation, this);
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
    clearCancellation(continuation, this);
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

AwaitSocketReceiveFromAwaiter::AwaitSocketReceiveFromAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                             Span<char> buffer, AwaitSocketReceiveFromResult& outResult)
    : await(await), socket(socket), buffer(buffer), outResult(outResult)
{}

bool AwaitSocketReceiveFromAwaiter::await_ready() const { return false; }

bool AwaitSocketReceiveFromAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitSocketReceiveFromAwaiter::cancel};

    outResult        = {};
    request.callback = [this](AsyncSocketReceiveFrom::Result& result)
    {
        operationResult         = result.get(outResult.data);
        outResult.sourceAddress = result.getSourceAddress();
        outResult.disconnected  = result.completionData.disconnected;
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, buffer);
    return operationResult;
}

Result AwaitSocketReceiveFromAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketReceiveFromAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketReceiveFromAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketReceiveFromAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

AwaitFileReadAwaiter::AwaitFileReadAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<char> buffer,
                                           AwaitFileReadResult& outResult)
    : await(await), file(file), buffer(buffer), outResult(outResult)
{}

bool AwaitFileReadAwaiter::await_ready() const { return false; }

bool AwaitFileReadAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitFileReadAwaiter::cancel};

    outResult        = {};
    request.callback = [this](AsyncFileRead::Result& result)
    {
        operationResult     = result.get(outResult.data);
        outResult.endOfFile = result.completionData.endOfFile;
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), file, buffer);
    return operationResult;
}

Result AwaitFileReadAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFileReadAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFileReadAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFileReadAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

AwaitFileWriteAwaiter::AwaitFileWriteAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<const char> data,
                                             AwaitFileWriteResult* outResult)
    : await(await), file(file), data(data), outResult(outResult)
{}

bool AwaitFileWriteAwaiter::await_ready() const { return false; }

bool AwaitFileWriteAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitFileWriteAwaiter::cancel};

    request.callback = [this](AsyncFileWrite::Result& result)
    {
        operationResult = result.isValid();
        if (outResult != nullptr)
        {
            operationResult = result.get(outResult->numBytes);
        }
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), file, data);
    return operationResult;
}

Result AwaitFileWriteAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFileWriteAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFileWriteAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFileWriteAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

AwaitFileSendAwaiter::AwaitFileSendAwaiter(AwaitEventLoop& await, const FileDescriptor& file,
                                           const SocketDescriptor& socket, AwaitFileSendResult& outResult,
                                           AwaitFileSendOptions options)
    : await(await), file(file), socket(socket), outResult(outResult), options(options)
{}

bool AwaitFileSendAwaiter::await_ready() const { return false; }

bool AwaitFileSendAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitFileSendAwaiter::cancel};

    outResult        = {};
    request.callback = [this](AsyncFileSend::Result& result)
    {
        operationResult            = result.isValid();
        outResult.bytesTransferred = result.getBytesTransferred();
        outResult.usedZeroCopy     = result.usedZeroCopy();
        outResult.complete         = result.isComplete();
        continuation.resume();
    };

    if (options.threadPool != nullptr)
    {
        operationResult = request.executeOn(taskSequence, *options.threadPool);
        if (not operationResult)
        {
            return false;
        }
    }

    operationResult =
        request.start(await.asyncEventLoop(), file, socket, options.offset, options.length, options.pipeSize);
    return operationResult;
}

Result AwaitFileSendAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFileSendAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFileSendAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFileSendAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

AwaitFileSystemOperationAwaiter::AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                                                 AwaitFileSystemOperationType operation,
                                                                 StringSpan path, StringSpan otherPath, FileOpen mode,
                                                                 FileDescriptor* outFile, FileSystemCopyFlags copyFlags)
    : await(await), threadPool(threadPool), operation(operation), path(path), otherPath(otherPath), mode(mode),
      outFile(outFile), copyFlags(copyFlags)
{}

AwaitFileSystemOperationAwaiter::AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                                                 AwaitFileSystemOperationType operation,
                                                                 FileDescriptor&              file)
    : await(await), threadPool(threadPool), operation(operation), fileToClose(&file)
{}

bool AwaitFileSystemOperationAwaiter::await_ready() const { return false; }

bool AwaitFileSystemOperationAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;

    request.callback = [this](AsyncFileSystemOperation::Result& result)
    {
        operationResult = result.isValid();
        if (operationResult and operation == AwaitFileSystemOperationType::Open)
        {
            FileDescriptor openedFile(result.completionData.handle);
            operationResult = outFile->assign(move(openedFile));
        }
        continuation.resume();
    };

    operationResult = request.setThreadPool(threadPool);
    if (not operationResult)
    {
        return false;
    }

    switch (operation)
    {
    case AwaitFileSystemOperationType::Open:
        if (outFile == nullptr)
        {
            operationResult = Result::Error("Await fsOpen missing output file");
            return false;
        }
        operationResult = request.open(await.asyncEventLoop(), path, mode);
        return operationResult;
    case AwaitFileSystemOperationType::Close: {
        if (fileToClose == nullptr)
        {
            operationResult = Result::Error("Await fsClose missing file");
            return false;
        }
        FileDescriptor::Handle handle = FileDescriptor::Invalid;
        operationResult               = fileToClose->get(handle, Result::Error("Await fsClose invalid file"));
        if (not operationResult)
        {
            return false;
        }
        operationResult = request.close(await.asyncEventLoop(), handle);
        if (operationResult)
        {
            fileToClose->detach();
        }
        return operationResult;
    }
    case AwaitFileSystemOperationType::CopyFile:
        operationResult = request.copyFile(await.asyncEventLoop(), path, otherPath, copyFlags);
        return operationResult;
    case AwaitFileSystemOperationType::CopyDirectory:
        operationResult = request.copyDirectory(await.asyncEventLoop(), path, otherPath, copyFlags);
        return operationResult;
    case AwaitFileSystemOperationType::Rename:
        operationResult = request.rename(await.asyncEventLoop(), path, otherPath);
        return operationResult;
    case AwaitFileSystemOperationType::RemoveEmptyDirectory:
        operationResult = request.removeEmptyDirectory(await.asyncEventLoop(), path);
        return operationResult;
    case AwaitFileSystemOperationType::RemoveFile:
        operationResult = request.removeFile(await.asyncEventLoop(), path);
        return operationResult;
    }

    operationResult = Result::Error("Await file system operation is invalid");
    return false;
}

Result AwaitFileSystemOperationAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

AwaitTaskTimeoutAwaiter::AwaitTaskTimeoutAwaiter(AwaitEventLoop& await, AwaitTask& task, TimeMs timeout,
                                                 AwaitTimeoutResult* outResult)
    : await(await), task(task), timeout(timeout), outResult(outResult)
{}

bool AwaitTaskTimeoutAwaiter::await_ready() const { return not task.isActive(); }

bool AwaitTaskTimeoutAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    if (outResult != nullptr)
    {
        outResult->timedOut = false;
    }

    if (not task.isActive())
    {
        operationResult = task.result();
        return false;
    }

    AwaitTask::Promise& promise = task.handle.promise();
    if (promise.completionCallback != nullptr or promise.continuation != nullptr)
    {
        operationResult = Result::Error("AwaitTask is already being awaited");
        return false;
    }

    promise.completionObject   = this;
    promise.completionCallback = AwaitTaskTimeoutAwaiter::onTaskCompleted;

    timeoutRequest.callback = [this](AsyncLoopTimeout::Result& result)
    {
        operationResult = result.isValid();
        if (not operationResult)
        {
            finish(operationResult);
            return;
        }

        timeoutFired = true;
        if (outResult != nullptr)
        {
            outResult->timedOut = true;
        }
        operationResult     = Result::Error("AwaitTask timed out");
        Result cancelResult = task.cancel(await);
        if (not cancelResult)
        {
            finish(cancelResult);
        }
    };

    operationResult = timeoutRequest.start(await.asyncEventLoop(), timeout);
    if (not operationResult)
    {
        promise.completionObject   = nullptr;
        promise.completionCallback = nullptr;
    }
    else
    {
        continuation.promise().cancellation = {this, AwaitTaskTimeoutAwaiter::cancel};
    }
    return operationResult;
}

Result AwaitTaskTimeoutAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

void AwaitTaskTimeoutAwaiter::onTaskCompleted(void* object)
{
    static_cast<AwaitTaskTimeoutAwaiter*>(object)->onTaskCompleted();
}

Result AwaitTaskTimeoutAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitTaskTimeoutAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitTaskTimeoutAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    cancelling      = true;
    if (outResult != nullptr)
    {
        outResult->timedOut = false;
    }

    Result cancelResult = task.cancel(eventLoop);
    if (not cancelResult)
    {
        return cancelResult;
    }

    if (timeoutFired)
    {
        return Result(true);
    }

    stopCallback = [this](AsyncResult&)
    {
        timeoutStopped = true;
        if (childCompleted)
        {
            finish(operationResult);
        }
    };
    return timeoutRequest.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

void AwaitTaskTimeoutAwaiter::onTaskCompleted()
{
    if (finished)
    {
        return;
    }

    childCompleted = true;
    if (timeoutFired)
    {
        finish(operationResult);
        return;
    }
    if (cancelling)
    {
        if (timeoutStopped)
        {
            finish(operationResult);
        }
        return;
    }

    operationResult = task.result();
    stopCallback    = [this](AsyncResult&)
    {
        timeoutStopped = true;
        finish(operationResult);
    };
    Result stopResult = timeoutRequest.stop(await.asyncEventLoop(), &stopCallback);
    if (not stopResult)
    {
        finish(stopResult);
    }
}

void AwaitTaskTimeoutAwaiter::finish(Result result)
{
    if (finished)
    {
        return;
    }
    finished        = true;
    operationResult = result;

    AwaitTask::Promise& promise = task.handle.promise();
    if (promise.completionObject == this)
    {
        promise.completionObject   = nullptr;
        promise.completionCallback = nullptr;
    }

    continuation.resume();
}

AwaitLoopWorkAwaiter::AwaitLoopWorkAwaiter(AwaitEventLoop& await, ThreadPool& threadPool, Function<Result()> work)
    : await(await), threadPool(threadPool), work(move(work))
{}

bool AwaitLoopWorkAwaiter::await_ready() const { return false; }

bool AwaitLoopWorkAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    stopCallback = [this](AsyncResult&) { continuation.resume(); };

    continuation.promise().cancellation = {this, AwaitLoopWorkAwaiter::cancel};

    if (not work.isValid())
    {
        operationResult = Result::Error("AwaitLoopWork callback is invalid");
        return false;
    }

    request.work     = [this] { return work(); };
    request.callback = [this](AsyncLoopWork::Result& result)
    {
        operationResult = result.isValid();
        continuation.resume();
    };

    operationResult = request.setThreadPool(threadPool);
    if (not operationResult)
    {
        return false;
    }
    operationResult = request.start(await.asyncEventLoop());
    return operationResult;
}

Result AwaitLoopWorkAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitLoopWorkAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitLoopWorkAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitLoopWorkAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitTaskCancelled();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

} // namespace SC
#endif
