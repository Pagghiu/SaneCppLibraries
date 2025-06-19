// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Containers/Internal/IntrusiveDoubleLinkedList.inl" // IWYU pragma: keep
#include "../Foundation/Platform.h"
#include "Libraries/File/FileDescriptor.h"

#include <string.h> // strncpy

#if SC_PLATFORM_WINDOWS
#include "Internal/AsyncWindows.inl"
#elif SC_PLATFORM_APPLE
#include "Internal/AsyncPosix.inl"
#elif SC_PLATFORM_LINUX
#include "Internal/AsyncLinux.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "Internal/AsyncEmscripten.inl"
#endif

#include "../Threading/ThreadPool.h"
#include "../Threading/Threading.h" // EventObject

#if SC_ASYNC_ENABLE_LOG
#include "../Strings/Console.h"
#else
#if defined(SC_LOG_MESSAGE)
#undef SC_LOG_MESSAGE
#endif
#define SC_LOG_MESSAGE(a, ...)
#endif

//-------------------------------------------------------------------------------------------------------
// AsyncRequest
//-------------------------------------------------------------------------------------------------------

#if SC_ASYNC_ENABLE_LOG
const char* SC::AsyncRequest::TypeToString(Type type)
{
    switch (type)
    {
    case Type::LoopTimeout: return "LoopTimeout";
    case Type::LoopWakeUp: return "LoopWakeUp";
    case Type::LoopWork: return "LoopWork";
    case Type::ProcessExit: return "ProcessExit";
    case Type::SocketAccept: return "SocketAccept";
    case Type::SocketConnect: return "SocketConnect";
    case Type::SocketSend: return "SocketSend";
    case Type::SocketSendTo: return "SocketSendTo";
    case Type::SocketReceive: return "SocketReceive";
    case Type::SocketReceiveFrom: return "SocketReceiveFrom";
    case Type::SocketClose: return "SocketClose";
    case Type::FileRead: return "FileRead";
    case Type::FileWrite: return "FileWrite";
    case Type::FilePoll: return "FilePoll";
    case Type::FileSystemOperation: return "FileSystemOperation";
    }
    Assert::unreachable();
}
#endif

void SC::AsyncRequest::setDebugName(const char* newDebugName)
{
    SC_COMPILER_UNUSED(newDebugName);
#if SC_ASYNC_ENABLE_LOG
    debugName = newDebugName;
#endif
}

void SC::AsyncRequest::executeOn(AsyncSequence& task) { sequence = &task; }

SC::Result SC::AsyncRequest::executeOn(AsyncTaskSequence& task, ThreadPool& pool)
{
    if (flags & AsyncEventLoop::Internal::Flag_AsyncTaskSequenceInUse)
    {
        return Result::Error("AsyncTaskSequence is bound to a different async being started");
    }
    task.threadPool = &pool;
    sequence        = &task;
    flags |= AsyncEventLoop::Internal::Flag_AsyncTaskSequence;
    flags |= AsyncEventLoop::Internal::Flag_AsyncTaskSequenceInUse;
    return Result(true);
}

void SC::AsyncRequest::disableThreadPool()
{
    AsyncTaskSequence* asyncTask = getTask();
    if (asyncTask)
    {
        // Preserve AsyncSequence but disable AsyncTaskSequence
        asyncTask->threadPool = nullptr;
        flags &= ~AsyncEventLoop::Internal::Flag_AsyncTaskSequenceInUse;
        flags &= ~AsyncEventLoop::Internal::Flag_AsyncTaskSequence;
    }
}

SC::Result SC::AsyncRequest::checkState()
{
    const bool asyncStateIsFree = state == AsyncRequest::State::Free;
    SC_LOG_MESSAGE("{} {} QUEUE\n", debugName, AsyncRequest::TypeToString(type));
    SC_TRY_MSG(asyncStateIsFree, "Trying to stage AsyncRequest that is in use");
    return Result(true);
}

void SC::AsyncRequest::markAsFree()
{
    state = AsyncRequest::State::Free;
    flags = 0;
}

void SC::AsyncRequest::queueSubmission(AsyncEventLoop& eventLoop) { eventLoop.internal.queueSubmission(*this); }

SC::AsyncTaskSequence* SC::AsyncRequest::getTask()
{
    if (flags & AsyncEventLoop::Internal::Flag_AsyncTaskSequence)
    {
        return static_cast<AsyncTaskSequence*>(sequence);
    }
    return nullptr;
}

SC::Result SC::AsyncRequest::stop(AsyncEventLoop& eventLoop, Function<void(AsyncResult&)>* onClose)
{
    return eventLoop.internal.stop(eventLoop, *this, onClose);
}

bool SC::AsyncRequest::isFree() const { return state == State::Free; }

bool SC::AsyncRequest::isCancelling() const { return state == State::Cancelling; }

bool SC::AsyncRequest::isActive() const { return state == State::Active or state == State::Reactivate; }

SC::Result SC::AsyncRequest::start(AsyncEventLoop& eventLoop) { return eventLoop.start(*this); }

//-------------------------------------------------------------------------------------------------------
// AsyncResult
//-------------------------------------------------------------------------------------------------------

void SC::AsyncResult::reactivateRequest(bool shouldBeReactivated)
{
    if (hasBeenReactivated)
        *hasBeenReactivated = shouldBeReactivated;
    if (shouldBeReactivated)
    {
        switch (async.state)
        {
        case AsyncRequest::State::Free: {
            if (AsyncEventLoop::Internal::KernelEvents::needsSubmissionWhenReactivating(async))
            {
                async.state = AsyncRequest::State::Reactivate;
                eventLoop.internal.submissions.queueBack(async);
                eventLoop.internal.numberOfSubmissions += 1;
            }
            else
            {
                async.state = AsyncRequest::State::Submitting;
                eventLoop.internal.addActiveHandle(async);
            }
        }
        break;
        case AsyncRequest::State::Reactivate: {
            // Nothing to do
        }
        break;
        case AsyncRequest::State::Cancelling:
        case AsyncRequest::State::Active:
        case AsyncRequest::State::Setup: {
            // Should not happen
            SC_ASSERT_RELEASE(false);
        }
        break;
        case AsyncRequest::State::Submitting:
            SC_ASSERT_RELEASE(AsyncEventLoop::Internal::KernelEvents::needsSubmissionWhenReactivating(async));
            break;
        }
    }
    else
    {
        switch (async.state)
        {
        case AsyncRequest::State::Free: {
            // Nothing to do
        }
        break;
        case AsyncRequest::State::Reactivate: {
            // TODO: Is a teardown needed here?
            async.state = AsyncRequest::State::Free;
            eventLoop.internal.submissions.remove(async);
            eventLoop.internal.numberOfSubmissions -= 1;
        }
        break;
        case AsyncRequest::State::Cancelling:
        case AsyncRequest::State::Active:
        case AsyncRequest::State::Setup: {
            // Should not happen
            SC_ASSERT_RELEASE(false);
        }
        break;
        case AsyncRequest::State::Submitting:
            SC_ASSERT_RELEASE(AsyncEventLoop::Internal::KernelEvents::needsSubmissionWhenReactivating(async));
            break;
        }
    }
}

//-------------------------------------------------------------------------------------------------------
// Async***
//-------------------------------------------------------------------------------------------------------
SC::Result SC::AsyncLoopTimeout::validate(AsyncEventLoop&) { return SC::Result(true); }

SC::Result SC::AsyncLoopTimeout::start(AsyncEventLoop& eventLoop, Time::Milliseconds timeout)
{
    relativeTimeout = timeout;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncLoopWakeUp::start(AsyncEventLoop& eventLoop, EventObject& eo)
{
    SC_TRY(checkState());
    eventObject = &eo;
    queueSubmission(eventLoop);
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWakeUp::validate(AsyncEventLoop&) { return SC::Result(true); }

SC::Result SC::AsyncLoopWakeUp::wakeUp(AsyncEventLoop& eventLoop) { return eventLoop.wakeUpFromExternalThread(*this); }

SC::Result SC::AsyncLoopWork::validate(AsyncEventLoop&)
{
    SC_TRY_MSG(work.isValid(), "AsyncLoopWork::start - Invalid work callback");
    SC_TRY_MSG(sequence != nullptr, "AsyncLoopWork::start - setThreadPool not called");
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWork::setThreadPool(ThreadPool& threadPool) { return executeOn(task, threadPool); }

SC::Result SC::AsyncProcessExit::start(AsyncEventLoop& loop, ProcessDescriptor::Handle process)
{
    handle = process;
    return loop.start(*this);
}

SC::Result SC::AsyncProcessExit::validate(AsyncEventLoop&)
{
    SC_TRY_MSG(handle != ProcessDescriptor::Invalid, "AsyncProcessExit - Invalid handle");
    return SC::Result(true);
}

SC::Result SC::detail::AsyncSocketAcceptBase::start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor,
                                                    detail::AsyncSocketAcceptData& data)
{
    acceptData = &data;
    SC_TRY(checkState());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(socketDescriptor.getAddressFamily(addressFamily));
    return eventLoop.start(*this);
}

SC::Result SC::AsyncSocketAccept::start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor)
{
    return AsyncSocketAcceptBase::start(eventLoop, socketDescriptor, data);
}

SC::Result SC::detail::AsyncSocketAcceptBase::validate(AsyncEventLoop&)
{
    SC_TRY_MSG(handle != SocketDescriptor::Invalid, "AsyncSocketAccept - Invalid handle");
    SC_TRY_MSG(acceptData != nullptr, "AsyncSocketAccept - Invalid acceptData");
    return SC::Result(true);
}

SC::Result SC::AsyncSocketConnect::start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor,
                                         SocketIPAddress address)
{
    SC_TRY(checkState());
    SC_TRY(descriptor.get(handle, SC::Result::Error("Invalid handle")));
    ipAddress = address;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncSocketConnect::validate(AsyncEventLoop&)
{
    SC_TRY_MSG(handle != SocketDescriptor::Invalid, "AsyncSocketConnect - Invalid handle");
    SC_TRY_MSG(ipAddress.isValid(), "AsyncSocketConnect - Invalid ipaddress");
    return SC::Result(true);
}

SC::Result SC::AsyncSocketSend::start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor,
                                      Span<const char> data)
{
    SC_TRY(descriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer       = data;
    singleBuffer = true;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncSocketSend::start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor,
                                      Span<Span<const char>> data)
{
    SC_TRY(descriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffers      = data;
    singleBuffer = false;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncSocketSend::validate(AsyncEventLoop&)
{
    SC_TRY_MSG(handle != SocketDescriptor::Invalid, "AsyncSocketSend - Invalid handle");
    if (singleBuffer)
    {
        SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncSocketSend - Zero sized write buffer");
    }
    else
    {
        SC_TRY_MSG(buffers.sizeInBytes() > 0 and not buffers[0].empty(), "AsyncSocketSend - Zero sized write buffer");
    }
    totalBytesWritten = 0;
    return SC::Result(true);
}

SC::Result SC::AsyncSocketSendTo::start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor,
                                        SocketIPAddress ipAddress, Span<const char> data)
{
    SC_TRY(descriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer       = data;
    singleBuffer = true;
    address      = ipAddress;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncSocketSendTo::start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor,
                                        SocketIPAddress ipAddress, Span<Span<const char>> data)
{
    SC_TRY(descriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffers      = data;
    singleBuffer = false;
    address      = ipAddress;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncSocketSendTo::validate(AsyncEventLoop& eventLoop)
{
    SC_TRY(AsyncSocketSend::validate(eventLoop))
    return SC::Result(true);
}

SC::Result SC::AsyncSocketReceive::start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor, Span<char> data)
{
    SC_TRY(descriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer = data;
    return eventLoop.start(*this);
}

SC::SocketIPAddress SC::AsyncSocketReceive::Result::getSourceAddress() const
{
    if (getAsync().getType() == Type::SocketReceiveFrom)
    {
        return static_cast<const AsyncSocketReceiveFrom&>(getAsync()).address;
    }
    return SocketIPAddress();
}

SC::Result SC::AsyncSocketReceive::validate(AsyncEventLoop&)
{
    SC_TRY_MSG(handle != SocketDescriptor::Invalid, "AsyncSocketReceive - Invalid handle");
    return SC::Result(true);
}

SC::Result SC::AsyncFileRead::validate(AsyncEventLoop& eventLoop)
{
    SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncFileRead - Zero sized read buffer");
    SC_TRY_MSG(handle != FileDescriptor::Invalid, "AsyncFileRead - Invalid file descriptor");
    // Only use the async tasks for operations and backends that are not io_uring
    if (not eventLoop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        disableThreadPool();
    }
    return SC::Result(true);
}

SC::Result SC::AsyncFileWrite::start(AsyncEventLoop& eventLoop, Span<Span<const char>> data)
{
    buffers      = data;
    singleBuffer = false;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncFileWrite::start(AsyncEventLoop& eventLoop, Span<const char> data)
{
    buffer       = data;
    singleBuffer = true;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncFileWrite::validate(AsyncEventLoop& eventLoop)
{
    if (singleBuffer)
    {
        SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncFileWrite - Zero sized write buffer");
    }
    else
    {
        SC_TRY_MSG(not buffers.empty() and not buffers[0].empty(), "AsyncFileWrite - Zero sized write buffer");
    }
    SC_TRY_MSG(handle != FileDescriptor::Invalid, "AsyncFileWrite - Invalid file descriptor");
    totalBytesWritten = 0;

    // Only use the async tasks for operations and backends that are not io_uring
    if (not eventLoop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        disableThreadPool();
    }

    return SC::Result(true);
}

SC::Result SC::AsyncFilePoll::start(AsyncEventLoop& eventLoop, FileDescriptor::Handle fd)
{
    SC_TRY(checkState());
    handle = fd;
    return eventLoop.start(*this);
}

SC::Result SC::AsyncFilePoll::validate(AsyncEventLoop&)
{
    SC_TRY_MSG(handle != FileDescriptor::Invalid, "AsyncFilePoll - Invalid file descriptor");
    return SC::Result(true);
}

//-------------------------------------------------------------------------------------------------------
// AsyncFileSystemOperation
//-------------------------------------------------------------------------------------------------------
SC::Result SC::AsyncFileSystemOperation::validate(AsyncEventLoop&)
{
    SC_TRY_MSG(operation != Operation::None, "AsyncFileSystemOperation - No operation set");
    switch (operation)
    {
    case Operation::Open: SC_TRY_MSG(not openData.path.isEmpty(), "AsyncFileSystemOperation - Invalid path"); break;
    case Operation::Close:
        SC_TRY_MSG(closeData.handle != FileDescriptor::Invalid, "AsyncFileSystemOperation - Invalid file descriptor");
        break;
    case Operation::Read:
        SC_TRY_MSG(readData.handle != FileDescriptor::Invalid, "AsyncFileSystemOperation - Invalid file descriptor");
        SC_TRY_MSG(readData.buffer.sizeInBytes() > 0, "AsyncFileSystemOperation - Zero sized read buffer");
        break;
    case Operation::Write:
        SC_TRY_MSG(writeData.handle != FileDescriptor::Invalid, "AsyncFileSystemOperation - Invalid file descriptor");
        SC_TRY_MSG(writeData.buffer.sizeInBytes() > 0, "AsyncFileSystemOperation - Zero sized write buffer");
        break;
    case Operation::None: break;
    }
    return SC::Result(true);
}

void SC::AsyncFileSystemOperation::destroy()
{
    switch (operation)
    {
    case Operation::Open: dtor(openData); break;
    case Operation::Close: dtor(closeData); break;
    case Operation::Read: dtor(readData); break;
    case Operation::Write: dtor(writeData); break;
    case Operation::None: break;
    }
    operation = Operation::None;
}

void SC::AsyncFileSystemOperation::onOperationCompleted(AsyncLoopWork::Result& res)
{
    SC::Result                       result = res.isValid();
    AsyncFileSystemOperation::Result fsRes(res.eventLoop, *this, result, nullptr);
    fsRes.completionData = completionData;
    callback(fsRes);
    // TODO: should we call reactivateRequest here?
}

SC::Result SC::AsyncFileSystemOperation::setThreadPool(ThreadPool& threadPool)
{
    return loopWork.setThreadPool(threadPool);
}

SC::Result SC::AsyncFileSystemOperation::open(AsyncEventLoop& eventLoop, StringViewData path, FileOpen mode)
{
    SC_TRY(checkState());
    operation = Operation::Open;
    new (&openData, PlacementNew()) OpenData({path, mode});
    SC_TRY(validate(eventLoop));
    if (not eventLoop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        return eventLoop.start(*this);
    }

    loopWork.work = [&]()
    {
        FileDescriptor fd;
        SC_TRY(fd.openNativeEncoding(openData.path, openData.mode));
        auto res = fd.get(completionData.handle, SC::Result::Error("Open returned invalid handle"));
        fd.detach(); // Detach the file descriptor from the loop work so that it is not closed when the callback ends
        return res;
    };
    loopWork.callback.bind<AsyncFileSystemOperation, &AsyncFileSystemOperation::onOperationCompleted>(*this);
    return eventLoop.start(loopWork);
}

SC::Result SC::AsyncFileSystemOperation::close(AsyncEventLoop& eventLoop, FileDescriptor::Handle handle)
{
    SC_TRY(checkState());
    operation = Operation::Close;
    new (&closeData, PlacementNew()) CloseData({handle});
    if (not eventLoop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        return eventLoop.start(*this);
    }

    loopWork.work = [&]()
    {
        FileDescriptor fd(closeData.handle);
        return fd.close();
    };
    loopWork.callback.bind<AsyncFileSystemOperation, &AsyncFileSystemOperation::onOperationCompleted>(*this);
    return eventLoop.start(loopWork);
}

SC::Result SC::AsyncFileSystemOperation::read(AsyncEventLoop& eventLoop, FileDescriptor::Handle handle,
                                              Span<char> buffer, uint64_t offset)
{
    SC_TRY(checkState());
    operation = Operation::Read;
    new (&readData, PlacementNew()) ReadData({handle, buffer, offset});
    if (not eventLoop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        return eventLoop.start(*this);
    }

    loopWork.work = [&]()
    {
        FileDescriptor fd(readData.handle);
        Span<char>     actuallyRead;
        SC_TRY(fd.read(readData.buffer, actuallyRead, readData.offset));
        completionData.numBytes = actuallyRead.sizeInBytes();
        return SC::Result(true);
    };
    loopWork.callback.bind<AsyncFileSystemOperation, &AsyncFileSystemOperation::onOperationCompleted>(*this);
    return eventLoop.start(loopWork);
}

SC::Result SC::AsyncFileSystemOperation::write(AsyncEventLoop& eventLoop, FileDescriptor::Handle handle,
                                               Span<const char> buffer, uint64_t offset)
{
    SC_TRY(checkState());
    operation = Operation::Write;
    new (&writeData, PlacementNew()) WriteData({handle, buffer, offset});
    if (not eventLoop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        return eventLoop.start(*this);
    }

    loopWork.work = [&]()
    {
        FileDescriptor fd(writeData.handle);
        SC_TRY(fd.write(writeData.buffer, writeData.offset));
        completionData.numBytes = writeData.buffer.sizeInBytes();
        return SC::Result(true);
    };
    loopWork.callback.bind<AsyncFileSystemOperation, &AsyncFileSystemOperation::onOperationCompleted>(*this);
    return eventLoop.start(loopWork);
}

//-------------------------------------------------------------------------------------------------------
// AsyncEventLoop
//-------------------------------------------------------------------------------------------------------

SC::AsyncEventLoop::AsyncEventLoop() : internal(internalOpaque.get()) {}

SC::Result SC::AsyncEventLoop::create(Options options)
{
    SC_TRY_MSG(not internal.initialized, "already created");
    SC_TRY(internal.kernelQueue.get().createEventLoop(options));
    SC_TRY(internal.kernelQueue.get().createSharedWatchers(*this));
    internal.initialized   = true;
    internal.createOptions = options;
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::close()
{
    SC_TRY_MSG(internal.initialized, "already closed");
    SC_TRY(internal.close(*this));
    internal.initialized = false;
    return SC::Result(true);
}

void SC::AsyncEventLoop::interrupt() { internal.interrupted = true; }

bool SC::AsyncEventLoop::isInitialized() const { return internal.initialized; }

SC::Result SC::AsyncEventLoop::runOnce() { return internal.runStep(*this, Internal::SyncMode::ForcedForwardProgress); }

SC::Result SC::AsyncEventLoop::runNoWait() { return internal.runStep(*this, Internal::SyncMode::NoWait); }

SC::Result SC::AsyncEventLoop::run()
{
    // It may happen that getTotalNumberOfActiveHandle() < 0 when re-activating an async that has been calling
    // excludeFromActiveCount() during initial setup. Now that async would be in the submissions.
    // One example that matches this case is re-activation of the FilePoll used for shared wakeups.
    while (internal.getTotalNumberOfActiveHandle() != 0 or not internal.submissions.isEmpty() or
           internal.hasPendingKernelCancellations or not internal.cancellations.isEmpty())
    {
        SC_TRY(runOnce());
        if (internal.interrupted)
        {
            internal.interrupted = false;
            break;
        }
    };
    // We may still have pending cancellation callbacks
    internal.executeCancellationCallbacks(*this);
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::submitRequests(AsyncKernelEvents& kernelEvents)
{
    return internal.submitRequests(*this, kernelEvents);
}

SC::Result SC::AsyncEventLoop::blockingPoll(AsyncKernelEvents& kernelEvents)
{
    return internal.blockingPoll(*this, Internal::SyncMode::ForcedForwardProgress, kernelEvents);
}

SC::Result SC::AsyncEventLoop::dispatchCompletions(AsyncKernelEvents& kernelEvents)
{
    return internal.dispatchCompletions(*this, Internal::SyncMode::ForcedForwardProgress, kernelEvents);
}

template <>
void SC::AsyncEventLoop::InternalOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::AsyncEventLoop::InternalOpaque::destruct(Object& obj)
{
    obj.~Object();
}

SC::Result SC::AsyncEventLoop::createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor)
{
    auto res = outDescriptor.create(family, SocketFlags::SocketStream, SocketFlags::ProtocolTcp,
                                    SocketFlags::NonBlocking, SocketFlags::NonInheritable);
    SC_TRY(res);
    return associateExternallyCreatedSocket(outDescriptor);
}

SC::Result SC::AsyncEventLoop::createAsyncUDPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor)
{
    auto res = outDescriptor.create(family, SocketFlags::SocketDgram, SocketFlags::ProtocolUdp,
                                    SocketFlags::NonBlocking, SocketFlags::NonInheritable);
    SC_TRY(res);
    return associateExternallyCreatedSocket(outDescriptor);
}

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread()
{
    if (not internal.wakeUpPending.exchange(true))
    {
        // This executes if current thread is lucky enough to atomically exchange pending from false to true.
        // This effectively allows coalescing calls from different threads into a single notification.
        return internal.kernelQueue.get().wakeUpFromExternalThread();
    }
    return Result(true);
}

SC::Result SC::AsyncEventLoop::associateExternallyCreatedSocket(SocketDescriptor& outDescriptor)
{
    return internal.kernelQueue.get().associateExternallyCreatedSocket(outDescriptor);
}

SC::Result SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor)
{
    return internal.kernelQueue.get().associateExternallyCreatedFileDescriptor(outDescriptor);
}

SC::Result SC::AsyncEventLoop::removeAllAssociationsFor(SocketDescriptor& outDescriptor)
{
    return Internal::KernelQueue::removeAllAssociationsFor(outDescriptor);
}

SC::Result SC::AsyncEventLoop::removeAllAssociationsFor(FileDescriptor& outDescriptor)
{
    return Internal::KernelQueue::removeAllAssociationsFor(outDescriptor);
}

void SC::AsyncEventLoop::updateTime() { return internal.updateTime(); }

SC::Time::Monotonic SC::AsyncEventLoop::getLoopTime() const { return internal.loopTime; }

int SC::AsyncEventLoop::getNumberOfActiveRequests() const { return internal.getTotalNumberOfActiveHandle(); }

int SC::AsyncEventLoop::getNumberOfSubmittedRequests() const { return internal.numberOfSubmissions; }

SC::AsyncLoopTimeout* SC::AsyncEventLoop::findEarliestLoopTimeout() const
{
    // activeLoopTimeouts are ordered by expiration time
    AsyncLoopTimeout* earliestTime = internal.activeLoopTimeouts.front;
    // Unfortunately we have to still manually find any potential submissions that could be earlier...
    for (AsyncRequest* async = internal.submissions.front; async != nullptr; async = async->next)
    {
        if (async->type == AsyncRequest::Type::LoopTimeout)
        {
            AsyncLoopTimeout& timeout = *static_cast<AsyncLoopTimeout*>(async);
            // We need to store computed expiration time, even if it will be recomputed later
            // in order to compare the ones that are still on the submission queue.
            timeout.expirationTime = internal.loopTime.offsetBy(timeout.relativeTimeout);
            if (earliestTime == nullptr or earliestTime->expirationTime.isLaterThanOrEqualTo(timeout.expirationTime))
            {
                earliestTime = &timeout;
            }
        }
    }

    return earliestTime;
}

void SC::AsyncEventLoop::excludeFromActiveCount(AsyncRequest& async)
{
    if (not async.isFree() and not async.isCancelling() and not isExcludedFromActiveCount(async))
    {
        async.flags |= Internal::Flag_ExcludeFromActiveCount;
        internal.numberOfExternals -= 1;
    }
}

void SC::AsyncEventLoop::includeInActiveCount(AsyncRequest& async)
{
    if (not async.isFree() and isExcludedFromActiveCount(async))
    {
        async.flags &= ~Internal::Flag_ExcludeFromActiveCount;
        internal.numberOfExternals += 1;
    }
}

bool SC::AsyncEventLoop::isExcludedFromActiveCount(const AsyncRequest& async)
{
    return (async.flags & Internal::Flag_ExcludeFromActiveCount) != 0;
}

/// @brief Enumerates all requests objects associated with this loop
void SC::AsyncEventLoop::enumerateRequests(Function<void(AsyncRequest&)> enumerationCallback)
{
    // TODO: Consolidate this list with stopAsync
    // TODO: Should cancellations be enumerated as well?
    internal.enumerateRequests(internal.submissions, enumerationCallback);
    internal.enumerateRequests(internal.activeLoopTimeouts, enumerationCallback);
    internal.enumerateRequests(internal.activeLoopWakeUps, enumerationCallback);
    internal.enumerateRequests(internal.activeProcessExits, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketAccepts, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketConnects, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketSends, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketSendsTo, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketReceives, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketReceivesFrom, enumerationCallback);
    internal.enumerateRequests(internal.activeFileReads, enumerationCallback);
    internal.enumerateRequests(internal.activeFileWrites, enumerationCallback);
    internal.enumerateRequests(internal.activeFilePolls, enumerationCallback);
    internal.enumerateRequests(internal.manualCompletions, enumerationCallback);
}

void SC::AsyncEventLoop::setListeners(AsyncEventLoopListeners* listeners) { internal.listeners = listeners; }

#if SC_PLATFORM_LINUX
#else
bool SC::AsyncEventLoop::tryLoadingLiburing() { return false; }
#endif

SC::Result SC::AsyncEventLoop::start(AsyncRequest& async)
{
    return Internal::applyOnAsync(async,
                                  [this](auto& async)
                                  {
                                      SC_TRY(async.checkState());
                                      SC_TRY(async.validate(*this));
                                      async.queueSubmission(*this);
                                      return SC::Result(true);
                                  });
}

void SC::AsyncEventLoop::clearSequence(AsyncSequence& sequence) { internal.clearSequence(sequence); }

//-------------------------------------------------------------------------------------------------------
// AsyncEventLoopMonitor
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncEventLoopMonitor::create(AsyncEventLoop& loop)
{
    if (eventLoop)
    {
        return Result::Error("Already initialized");
    }
    eventLoop = &loop;

    asyncKernelEvents.eventsMemory = eventsMemory;
    SC_TRY(eventLoopWakeUp.start(*eventLoop));
    eventLoopWakeUp.callback = [this](AsyncLoopWakeUp::Result& result)
    {
        result.reactivateRequest(true);
        wakeUpHasBeenCalled = true;
    };
    SC_TRY(eventLoopThread.start([this](Thread& thread) { SC_TRUST_RESULT(monitoringLoopThread(thread)); }));
    return Result(true);
}

SC::Result SC::AsyncEventLoopMonitor::startMonitoring()
{
    // Submit all requests made so far before entering polling mode
    SC_TRY(eventLoop->submitRequests(asyncKernelEvents));
    eventObjectEnterBlockingMode.signal();
    return Result(true);
}

SC::Result SC::AsyncEventLoopMonitor::monitoringLoopThread(Thread& thread)
{
    thread.setThreadName(SC_NATIVE_STR("Monitoring Loop thread"));
    do
    {
        eventObjectEnterBlockingMode.wait();
        // Block to poll for events and store them into asyncKernelEvents
        Result res = eventLoop->blockingPoll(asyncKernelEvents);
        needsWakeUp.exchange(false);
        onNewEventsAvailable();
        eventObjectExitBlockingMode.signal();
        if (not res)
        {
            return res;
        }
    } while (not finished.load());
    return Result(true);
}

SC::Result SC::AsyncEventLoopMonitor::stopMonitoringAndDispatchCompletions()
{
    SC_TRY_MSG(eventLoop != nullptr, "Not initialized");
    SC_TRY_MSG(not finished.load(), "Finished == true");
    // Unblock the blocking poll on the other thread, even if it could be already unblocked
    const bool wakeUpMustBeSent = needsWakeUp.load();
    if (wakeUpMustBeSent)
    {
        wakeUpHasBeenCalled = false;
        SC_TRY(eventLoopWakeUp.wakeUp(*eventLoop));
    }
    eventObjectExitBlockingMode.wait();
    needsWakeUp.exchange(true);
    // Dispatch the callbacks associated with the IO events signaled by AsyncEventLoop::blockingPoll
    SC_TRY(eventLoop->dispatchCompletions(asyncKernelEvents));
    if (wakeUpMustBeSent and not wakeUpHasBeenCalled)
    {
        // We need one more event loop step to consume the earlier wakeUpFromExternalThread().
        // Note: runOnce will also submit new async requests potentially queued by the callbacks.
        return eventLoop->runOnce();
    }
    return Result(true);
}

SC::Result SC::AsyncEventLoopMonitor::close()
{
    if (eventLoop == nullptr)
    {
        return Result::Error("Not initialized");
    }
    finished.exchange(true);
    eventObjectEnterBlockingMode.signal();
    eventObjectExitBlockingMode.signal();
    SC_TRY(eventLoop->wakeUpFromExternalThread());
    SC_TRY(eventLoopThread.join());
    SC_TRY(eventLoopWakeUp.stop(*eventLoop));
    eventLoop = nullptr;
    return Result(true);
}

void SC::AsyncEventLoop::Internal::popNextInSequence(AsyncSequence& sequence)
{
    sequence.runningAsync = true;
    submissions.queueBack(*sequence.submissions.dequeueFront());
    numberOfSubmissions += 1;
    if (sequence.submissions.isEmpty())
    {
        clearSequence(sequence);
    }
}

//-------------------------------------------------------------------------------------------------------
// AsyncEventLoop::Internal
//-------------------------------------------------------------------------------------------------------

void SC::AsyncEventLoop::Internal::queueSubmission(AsyncRequest& async)
{
    async.state = AsyncRequest::State::Setup;
    if (async.sequence)
    {
        AsyncSequence& sequence = *async.sequence;
        sequence.submissions.queueBack(async);
        if (sequence.runningAsync)
        {
            if (not sequence.tracked)
            {
                sequences.queueBack(sequence);
                sequence.tracked = true;
            }
        }
        else
        {
            popNextInSequence(sequence);
        }
    }
    else
    {
        submissions.queueBack(async);
        numberOfSubmissions += 1;
    }
}

void SC::AsyncEventLoop::Internal::resumeSequence(AsyncSequence& sequence)
{
    if (not sequence.runningAsync)
    {
        if (not sequence.submissions.isEmpty())
        {
            popNextInSequence(sequence);
        }
    }
}

void SC::AsyncEventLoop::Internal::clearSequence(AsyncSequence& sequence)
{
    sequence.submissions.clear();
    if (sequence.tracked)
    {
        sequences.remove(sequence);
        sequence.tracked = false;
    }
}

SC::AsyncLoopTimeout* SC::AsyncEventLoop::Internal::findEarliestLoopTimeout() const { return activeLoopTimeouts.front; }

void SC::AsyncEventLoop::Internal::invokeExpiredTimers(AsyncEventLoop& eventLoop, Time::Absolute currentTime)
{
    AsyncLoopTimeout* async = activeLoopTimeouts.front;
    while (async != nullptr)
    {
        AsyncLoopTimeout* current = async;
        async                     = static_cast<AsyncLoopTimeout*>(async->next);
        if (currentTime.isLaterThanOrEqualTo(current->expirationTime))
        {
            removeActiveHandle(*current);
            Result                   res(true);
            AsyncLoopTimeout::Result result(eventLoop, *current, res);
            if (current->callback.isValid())
            {
                current->callback(result);
            }

            if (async != nullptr and not async->isActive())
            {
                // Our "next" timeout to check could have been Cancelled during the callback
                // and it could be in the submission queue now.
                // It's possible detecting this case by checking the active state.
                // In this case it makes sense to re-check the entire active timers list.
                async = activeLoopTimeouts.front;
                SC_ASSERT_DEBUG(async == nullptr or async->isActive()); // Should not be possible
            }
        }
        else
        {
            // Timers are ordered by expirationTime so we can safely break out of this loop
            break;
        }
    }
}

template <typename T>
void SC::AsyncEventLoop::Internal::stopRequests(AsyncEventLoop& eventLoop, IntrusiveDoubleLinkedList<T>& linkedList)
{
    auto async = linkedList.front;
    while (async != nullptr)
    {
        auto asyncNext = static_cast<T*>(async->next);
        if (not async->isCancelling() and not async->isFree())
        {
            Result res = async->stop(eventLoop);
            (void)res;
            SC_ASSERT_DEBUG(res);
        }
        async = asyncNext;
    }
}

template <typename T>
void SC::AsyncEventLoop::Internal::enumerateRequests(IntrusiveDoubleLinkedList<T>&  linkedList,
                                                     Function<void(AsyncRequest&)>& callback)
{
    auto async = linkedList.front;
    while (async != nullptr)
    {
        T* asyncNext = static_cast<T*>(async->next);
        if ((async->flags & Flag_Internal) == 0) // Exclude internal requests
        {
            callback(*async);
        }
        async = asyncNext;
    }
}

template <typename T>
SC::Result SC::AsyncEventLoop::Internal::waitForThreadPoolTasks(IntrusiveDoubleLinkedList<T>& linkedList)
{
    Result res = Result(true);
    // Wait for all thread pool tasks
    for (T* it = linkedList.front; it != nullptr; it = static_cast<T*>(it->next))
    {
        AsyncTaskSequence* asyncTask = it->getTask();
        if (asyncTask != nullptr)
        {
            if (not asyncTask->threadPool->waitForTask(asyncTask->task))
            {
                res = Result::Error("Threadpool was already stopped");
            }
            it->flags &= ~AsyncEventLoop::Internal::Flag_AsyncTaskSequenceInUse;
        }
    }
    return res;
}

SC::Result SC::AsyncEventLoop::Internal::close(AsyncEventLoop& eventLoop)
{
    Result res = Result(true);

    // Wait for all thread pool tasks
    Result threadPoolRes1 = waitForThreadPoolTasks(activeFileReads);
    Result threadPoolRes2 = waitForThreadPoolTasks(activeFileWrites);

    if (not threadPoolRes1)
        res = threadPoolRes1;

    if (not threadPoolRes2)
        res = threadPoolRes2;

    // Clear the never submitted requests of all sequences
    for (AsyncSequence* sequence = sequences.front; sequence != nullptr; sequence = sequence->next)
    {
        clearSequence(*sequence);
    }
    sequences.clear();

    // TODO: Consolidate this list with enumerateRequests
    stopRequests(eventLoop, submissions);

    while (AsyncRequest* async = manualThreadPoolCompletions.pop())
    {
        Result stopRes = async->stop(eventLoop);
        (void)stopRes;
        SC_ASSERT_DEBUG(stopRes);
    }

    stopRequests(eventLoop, activeLoopTimeouts);
    stopRequests(eventLoop, activeLoopWakeUps);
    stopRequests(eventLoop, activeProcessExits);
    stopRequests(eventLoop, activeSocketAccepts);
    stopRequests(eventLoop, activeSocketConnects);
    stopRequests(eventLoop, activeSocketSends);
    stopRequests(eventLoop, activeSocketSendsTo);
    stopRequests(eventLoop, activeSocketReceives);
    stopRequests(eventLoop, activeSocketReceivesFrom);
    stopRequests(eventLoop, activeFileReads);
    stopRequests(eventLoop, activeFileWrites);
    stopRequests(eventLoop, activeFilePolls);

    stopRequests(eventLoop, manualCompletions);

    SC_TRY(eventLoop.run());
    executeCancellationCallbacks(eventLoop);
    SC_TRY(eventLoop.internal.kernelQueue.get().close());
    if (numberOfExternals != 0 or numberOfActiveHandles != 0 or numberOfManualCompletions != 0)
    {
        return Result::Error("Non-Zero active count after close");
    }
    return res;
}

SC::Result SC::AsyncEventLoop::Internal::stageSubmission(AsyncEventLoop& eventLoop, KernelEvents& kernelEvents,
                                                         AsyncRequest& async)
{
    switch (async.state)
    {
    case AsyncRequest::State::Setup: {
        SC_TRY(setupAsync(eventLoop, kernelEvents, async));
        async.state = AsyncRequest::State::Submitting;
        SC_TRY(activateAsync(eventLoop, kernelEvents, async));
    }
    break;
    case AsyncRequest::State::Submitting: {
        SC_TRY(activateAsync(eventLoop, kernelEvents, async));
    }
    break;
    case AsyncRequest::State::Reactivate: {
        async.state = AsyncRequest::State::Submitting;
        SC_TRY(activateAsync(eventLoop, kernelEvents, async));
    }
    break;
    case AsyncRequest::State::Free: {
        // TODO: Stop the completion, it has been cancelled before being submitted
        SC_ASSERT_RELEASE(false);
    }
    break;
    case AsyncRequest::State::Cancelling: {
        SC_TRY(cancelAsync(eventLoop, kernelEvents, async));
        AsyncTeardown teardown;
        prepareTeardown(eventLoop, async, teardown);
        SC_TRY(teardownAsync(teardown));
        pushToCancellationQueue(async);
    }
    break;
    case AsyncRequest::State::Active: {
        SC_ASSERT_DEBUG(false);
        return SC::Result::Error("AsyncEventLoop::processSubmissions() got Active handle");
    }
    break;
    }
    return SC::Result(true);
}

int SC::AsyncEventLoop::Internal::getTotalNumberOfActiveHandle() const
{
    return numberOfActiveHandles + numberOfExternals;
}

SC::Result SC::AsyncEventLoop::Internal::completeAndReactivateOrTeardown(AsyncEventLoop& eventLoop,
                                                                         KernelEvents&   kernelEvents,
                                                                         AsyncRequest& async, int32_t eventIndex,
                                                                         Result& returnCode)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Active);
    removeActiveHandle(async);
    AsyncTeardown teardown;
    prepareTeardown(eventLoop, async, teardown);
    bool hasBeenReactivated = false;
    SC_TRY(completeAsync(eventLoop, kernelEvents, async, eventIndex, returnCode, &hasBeenReactivated));
    // hasBeenReactivated is required to avoid accessing async when it has been not reactivated (and maybe deallocated)
    if (hasBeenReactivated and async.state == AsyncRequest::State::Reactivate)
    {
        if (teardown.sequence)
        {
            teardown.sequence->runningAsync = true;
        }
    }
    else
    {
        SC_TRY(teardownAsync(teardown));
        if (teardown.sequence)
        {
            resumeSequence(*teardown.sequence);
        }
    }
    if (not returnCode)
    {
        // TODO: We shouldn't probably access async if it has not been reactivated...
        reportError(eventLoop, kernelEvents, async, returnCode, eventIndex);
    }
    return Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::runStep(AsyncEventLoop& eventLoop, SyncMode syncMode)
{
    alignas(uint64_t) uint8_t buffer[8 * 1024]; // 8 Kb of kernel events
    AsyncKernelEvents         kernelEvents;
    kernelEvents.eventsMemory = buffer;
    SC_TRY(submitRequests(eventLoop, kernelEvents));
    SC_TRY(blockingPoll(eventLoop, syncMode, kernelEvents));
    return dispatchCompletions(eventLoop, syncMode, kernelEvents);
}

void SC::AsyncEventLoop::Internal::pushToCancellationQueue(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.isCancelling());
    if (async.sequence and async.sequence->clearSequenceOnCancel)
    {
        clearSequence(*async.sequence);
    }
    cancellations.queueBack(async);
}

SC::Result SC::AsyncEventLoop::Internal::submitRequests(AsyncEventLoop& eventLoop, AsyncKernelEvents& asyncKernelEvents)
{
    KernelEvents kernelEvents(eventLoop.internal.kernelQueue.get(), asyncKernelEvents);
    asyncKernelEvents.numberOfEvents = 0;
    // TODO: Check if it's possible to avoid zeroing kernel events memory
    memset(asyncKernelEvents.eventsMemory.data(), 0, asyncKernelEvents.eventsMemory.sizeInBytes());
    SC_LOG_MESSAGE("---------------\n");

    updateTime();
    while (AsyncRequest* async = submissions.dequeueFront())
    {
        numberOfSubmissions -= 1;
        auto res = stageSubmission(eventLoop, kernelEvents, *async);
        if (not res)
        {
            reportError(eventLoop, kernelEvents, *async, res, -1);
        }
    }

    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::blockingPoll(AsyncEventLoop& eventLoop, SyncMode syncMode,
                                                      AsyncKernelEvents& asyncKernelEvents)
{
    if (listeners and listeners->beforeBlockingPoll.isValid())
    {
        listeners->beforeBlockingPoll(eventLoop);
    }
    KernelEvents kernelEvents(eventLoop.internal.kernelQueue.get(), asyncKernelEvents);
    const auto   numActiveHandles = getTotalNumberOfActiveHandle();
    SC_ASSERT_RELEASE(numActiveHandles >= 0);
    if (numActiveHandles > 0 or numberOfManualCompletions != 0 or hasPendingKernelCancellations)
    {
        hasPendingKernelCancellations = false;
        // We may have some manualCompletions queued (for SocketClose for example) but no active handles
        SC_LOG_MESSAGE("Active Requests Before Poll = {}\n", getTotalNumberOfActiveHandle());

        // If there are manual completions the loop can't block waiting for I/O, to dispatch them immediately
        const bool canBlockForIO = numberOfManualCompletions == 0;
        SC_TRY(kernelEvents.syncWithKernel(eventLoop, canBlockForIO ? syncMode : SyncMode::NoWait));
        SC_LOG_MESSAGE("Active Requests After Poll = {}\n", getTotalNumberOfActiveHandle());
    }

    if (listeners and listeners->afterBlockingPoll.isValid())
    {
        listeners->afterBlockingPoll(eventLoop);
    }
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::dispatchCompletions(AsyncEventLoop& eventLoop, SyncMode syncMode,
                                                             AsyncKernelEvents& asyncKernelEvents)
{
    if (interrupted)
    {
        return SC::Result(true);
    }
    KernelEvents kernelEvents(eventLoop.internal.kernelQueue.get(), asyncKernelEvents);
    switch (syncMode)
    {
    case SyncMode::NoWait: {
        // No need to update time as it was already updated in submitRequests and syncing
        // with kernel has not been blocking (as we are in NoWait mode)
        invokeExpiredTimers(eventLoop, loopTime);
    }
    break;
    case SyncMode::ForcedForwardProgress: {
        // Update loop time unconditionally after a (potentially blocking) sync kernel operation
        updateTime();
        if (runTimers)
        {
            runTimers = false;
            invokeExpiredTimers(eventLoop, loopTime);
        }
    }
    break;
    }
    runStepExecuteCompletions(eventLoop, kernelEvents);
    runStepExecuteManualCompletions(eventLoop, kernelEvents);
    runStepExecuteManualThreadPoolCompletions(eventLoop, kernelEvents);
    executeCancellationCallbacks(eventLoop);

    SC_LOG_MESSAGE("Active Requests After Completion = {} ( + {} manual)\n", getTotalNumberOfActiveHandle(),
                   numberOfManualCompletions);
    return SC::Result(true);
}

void SC::AsyncEventLoop::Internal::executeCancellationCallbacks(AsyncEventLoop& eventLoop)
{
    AsyncRequest* async = cancellations.front;
    while (async)
    {
        AsyncRequest* next = async->next;
        SC_ASSERT_RELEASE(async->state == AsyncRequest::State::Cancelling);
        async->markAsFree();
        cancellations.remove(*async);
        if (async->closeCallback)
        {
            Function<void(AsyncResult&)>& closeCallback = *async->closeCallback;

            Result      result(true);
            AsyncResult res(eventLoop, *async, result);
            closeCallback(res);
        }
        async = next;
    }
}

void SC::AsyncEventLoop::Internal::runStepExecuteManualCompletions(AsyncEventLoop& eventLoop,
                                                                   KernelEvents&   kernelEvents)
{
    while (AsyncRequest* async = manualCompletions.dequeueFront())
    {
        Result res(true);
        if (not completeAndReactivateOrTeardown(eventLoop, kernelEvents, *async, -1, res))
        {
            SC_LOG_MESSAGE("Error completing {}", async->debugName);
        }
    }
}

void SC::AsyncEventLoop::Internal::runStepExecuteManualThreadPoolCompletions(AsyncEventLoop& eventLoop,
                                                                             KernelEvents&   kernelEvents)
{
    while (AsyncRequest* async = manualThreadPoolCompletions.pop())
    {
        Result res(true);
        if (not completeAndReactivateOrTeardown(eventLoop, kernelEvents, *async, -1, res))
        {
            SC_LOG_MESSAGE("Error completing {}", async->debugName);
        }
    }
}

void SC::AsyncEventLoop::Internal::runStepExecuteCompletions(AsyncEventLoop& eventLoop, KernelEvents& kernelEvents)
{
    for (uint32_t idx = 0; idx < kernelEvents.getNumEvents(); ++idx)
    {
        SC_LOG_MESSAGE(" Iteration = {}\n", idx);
        SC_LOG_MESSAGE(" Active Requests = {}\n", getTotalNumberOfActiveHandle());
        bool continueProcessing = true;

        AsyncRequest* request = kernelEvents.getAsyncRequest(idx);
        if (request == nullptr)
        {
            continue;
        }
        const int32_t eventIndex = static_cast<int32_t>(idx);

        AsyncRequest& async  = *request;
        Result        result = Result(kernelEvents.validateEvent(idx, continueProcessing));
        if (not result)
        {
            reportError(eventLoop, kernelEvents, async, result, eventIndex);
            continue;
        }

        if (not continueProcessing)
        {
            continue;
        }
        if (async.state == AsyncRequest::State::Active)
        {
            const bool isManualCompletion = (async.flags & Internal::Flag_ManualCompletion) != 0;
            if (isManualCompletion)
            {
                // Posix AsyncSocketSend sets Flag_ManualCompletion while also setting an active
                // write watcher that must be removed to avoid executing completion two times.
                manualCompletions.remove(async);
            }
            if (not completeAndReactivateOrTeardown(eventLoop, kernelEvents, async, eventIndex, result))
            {
                SC_LOG_MESSAGE("Error completing {}", async.debugName);
            }
        }
        else
        {
            // Cancellations are not delivered on epoll / kqueue backends and they're filtered by
            // KernelEvents::validateEvent on Windows IOCP and Linux io_uring.
            SC_ASSERT_RELEASE(false);
        }
    }
}

struct SC::AsyncEventLoop::Internal::SetupAsyncPhase
{
    AsyncEventLoop& eventLoop;
    KernelEvents&   kernelEvents;

    template <typename T>
    SC::Result operator()(T& async)
    {
        if (async.getTask())
        {
            return Result(true);
        }
        else
        {
            return Result(kernelEvents.setupAsync(eventLoop, async));
        }
    }
};

struct SC::AsyncEventLoop::Internal::ActivateAsyncPhase
{
    AsyncEventLoop& eventLoop;
    KernelEvents&   kernelEvents;

    template <typename T>
    SC::Result operator()(T& async)
    {
        AsyncTaskSequence* asyncTask = async.getTask();
        if (asyncTask)
        {
            AsyncEventLoop* loop     = &eventLoop;
            asyncTask->task.function = [&async, loop] { executeThreadPoolOperation(*loop, async); };
            return asyncTask->threadPool->queueTask(asyncTask->task);
        }

        SC_TRY(kernelEvents.activateAsync(eventLoop, async));
        if (async.flags & Internal::Flag_ManualCompletion)
        {
            eventLoop.internal.scheduleManualCompletion(async);
        }
        return Result(true);
    }

    template <typename T>
    static void executeThreadPoolOperation(AsyncEventLoop& eventLoop, T& async)
    {
        AsyncTaskSequence& task = *async.getTask();
        task.returnCode         = KernelEvents::executeOperation(async, task.completion.construct(async));
        eventLoop.internal.manualThreadPoolCompletions.push(async);
        SC_ASSERT_RELEASE(eventLoop.wakeUpFromExternalThread());
    }
};

struct SC::AsyncEventLoop::Internal::CancelAsyncPhase
{
    AsyncEventLoop& eventLoop;
    KernelEvents&   kernelEvents;
    CancelAsyncPhase(AsyncEventLoop& eventLoop, KernelEvents& kernelEvents)
        : eventLoop(eventLoop), kernelEvents(kernelEvents)
    {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        AsyncTaskSequence* asyncTask = async.getTask();
        if (asyncTask)
        {
            // Waiting here is not ideal but we need it to be able to reliably know that
            // the task can be reused soon after cancelling an async that uses it.
            SC_TRY(asyncTask->threadPool->waitForTask(asyncTask->task));

            // Prevent this async from going in the CompleteAsyncPhase and mark task as free
            eventLoop.internal.manualThreadPoolCompletions.remove(async);
            async.flags &= ~AsyncEventLoop::Internal::Flag_AsyncTaskSequenceInUse;
            return Result(true);
        }

        if (async.flags & Internal::Flag_ManualCompletion)
        {
            eventLoop.internal.manualCompletions.remove(async);
            return Result(true);
        }
        else
        {
            return Result(kernelEvents.cancelAsync(eventLoop, async));
        }
    }
};

struct SC::AsyncEventLoop::Internal::CompleteAsyncPhase
{
    int32_t         eventIndex;
    AsyncEventLoop& eventLoop;
    KernelEvents&   kernelEvents;

    Result& returnCode;
    bool*   hasBeenReactivated;
    CompleteAsyncPhase(int32_t eventIndex, AsyncEventLoop& eventLoop, KernelEvents& kernelEvents, Result& result,
                       bool* hasBeenReactivated = nullptr)
        : eventIndex(eventIndex), eventLoop(eventLoop), kernelEvents(kernelEvents), returnCode(result),
          hasBeenReactivated(hasBeenReactivated)
    {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        using AsyncType       = T;
        using AsyncResultType = typename AsyncType::Result;
        using AsyncCompletion = typename AsyncType::CompletionData;
        AsyncResultType result(eventLoop, async, returnCode, hasBeenReactivated);
        result.eventIndex = eventIndex;
        if (result.returnCode)
        {
            AsyncTaskSequence* asyncTask = async.getTask();
            if (asyncTask)
            {
                result.returnCode     = asyncTask->returnCode;
                result.completionData = move(asyncTask->completion.getCompletion(async));
                // The task is already finished but we need waitForTask to make it available for next runs.
                SC_TRY(asyncTask->threadPool->waitForTask(asyncTask->task));
                async.flags &= ~AsyncEventLoop::Internal::Flag_AsyncTaskSequenceInUse;
            }
            else
            {
                result.returnCode = Result(kernelEvents.completeAsync(result));
            }
        }
        auto callback = result.getAsync().callback; // copy callback to allow it releasing the request
        if (result.shouldCallCallback and callback.isValid())
        {
            callback(result);
        }
        return SC::Result(true);
    }
};

void SC::AsyncEventLoop::Internal::prepareTeardown(AsyncEventLoop& eventLoop, AsyncRequest& async,
                                                   AsyncTeardown& teardown)
{
    teardown.eventLoop = &eventLoop;
    teardown.type      = async.type;
    teardown.flags     = async.flags;
    teardown.sequence  = async.sequence;
#if SC_ASYNC_ENABLE_LOG
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
    ::strncpy_s(teardown.debugName, async.debugName, sizeof(teardown.debugName));
#else
    ::strncpy(teardown.debugName, async.debugName, sizeof(teardown.debugName));
#endif
#endif
    // clang-format off
    switch (async.type)
    {
    // Loop
    case AsyncRequest::Type::LoopTimeout: break;
    case AsyncRequest::Type::LoopWakeUp: break;
    case AsyncRequest::Type::LoopWork: break;

    // Process
    case AsyncRequest::Type::ProcessExit:
#if SC_PLATFORM_LINUX
        (void)static_cast<AsyncProcessExit&>(async).pidFd.get(teardown.fileHandle, Result::Error("missing pidfd"));
#endif
        teardown.processHandle = static_cast<AsyncProcessExit&>(async).handle;
        break;

    // Socket
    case AsyncRequest::Type::SocketAccept:  teardown.socketHandle = static_cast<AsyncSocketAccept&>(async).handle; break;
    case AsyncRequest::Type::SocketConnect: teardown.socketHandle = static_cast<AsyncSocketConnect&>(async).handle; break;
    case AsyncRequest::Type::SocketSend:    teardown.socketHandle = static_cast<AsyncSocketSend&>(async).handle; break;
    case AsyncRequest::Type::SocketSendTo:  teardown.socketHandle = static_cast<AsyncSocketSendTo&>(async).handle; break;
    case AsyncRequest::Type::SocketReceive: teardown.socketHandle = static_cast<AsyncSocketReceive&>(async).handle; break;
    case AsyncRequest::Type::SocketReceiveFrom: teardown.socketHandle = static_cast<AsyncSocketReceiveFrom&>(async).handle; break;

    // File
    case AsyncRequest::Type::FileRead:      teardown.fileHandle = static_cast<AsyncFileRead&>(async).handle; break;
    case AsyncRequest::Type::FileWrite:     teardown.fileHandle = static_cast<AsyncFileWrite&>(async).handle; break;
    case AsyncRequest::Type::FilePoll:      teardown.fileHandle = static_cast<AsyncFilePoll&>(async).handle; break;

    // FileSystemOperation
    case AsyncRequest::Type::FileSystemOperation:  break;
    }
    // clang-format on
}
SC::Result SC::AsyncEventLoop::Internal::setupAsync(AsyncEventLoop& eventLoop, KernelEvents& kernelEvents,
                                                    AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    // Reset flags that may have been left by previous activations
    async.flags &= ~Flag_ManualCompletion;
    // Note that we're preserving the Flag_ExcludeFromActiveCount
    return Internal::applyOnAsync(async, SetupAsyncPhase{eventLoop, kernelEvents});
}

SC::Result SC::AsyncEventLoop::Internal::activateAsync(AsyncEventLoop& eventLoop, KernelEvents& kernelEvents,
                                                       AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} ACTIVATE\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Submitting);
    SC_TRY(Internal::applyOnAsync(async, ActivateAsyncPhase{eventLoop, kernelEvents}));
    addActiveHandle(async);
    return Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::teardownAsync(AsyncTeardown& teardown)
{
    SC_LOG_MESSAGE("{} {} TEARDOWN\n", teardown.debugName, AsyncRequest::TypeToString(teardown.type));

    switch (teardown.type)
    {
    case AsyncRequest::Type::LoopTimeout:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncLoopTimeout*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::LoopWakeUp:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncLoopWakeUp*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::LoopWork:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncLoopWork*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::ProcessExit:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncProcessExit*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::SocketAccept:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncSocketAccept*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::SocketConnect:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncSocketConnect*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::SocketSend:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncSocketSend*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::SocketSendTo:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncSocketSendTo*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::SocketReceive:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncSocketReceive*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::SocketReceiveFrom:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncSocketReceiveFrom*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::FileRead:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncFileRead*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::FileWrite:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncFileWrite*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::FilePoll:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncFilePoll*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::FileSystemOperation:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncFileSystemOperation*>(nullptr), teardown));
        break;
    }

    if ((teardown.flags & Internal::Flag_ExcludeFromActiveCount) != 0)
    {
        numberOfExternals += 1;
    }
    return Result(true);
}

void SC::AsyncEventLoop::Internal::reportError(AsyncEventLoop& eventLoop, KernelEvents& kernelEvents,
                                               AsyncRequest& async, Result& returnCode, int32_t eventIndex)
{
    SC_LOG_MESSAGE("{} ERROR {}\n", async.debugName, AsyncRequest::TypeToString(async.type));
    if (async.state == AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    if (async.sequence and async.sequence->clearSequenceOnError)
    {
        clearSequence(*async.sequence);
    }
    (void)completeAsync(eventLoop, kernelEvents, async, eventIndex, returnCode);
    if (not async.isCancelling())
    {
        async.markAsFree();
    }
}

SC::Result SC::AsyncEventLoop::Internal::completeAsync(AsyncEventLoop& eventLoop, KernelEvents& kernelEvents,
                                                       AsyncRequest& async, int32_t eventIndex, Result returnCode,
                                                       bool* hasBeenReactivated)
{
    if (returnCode)
    {
        SC_LOG_MESSAGE("{} {} COMPLETE\n", async.debugName, AsyncRequest::TypeToString(async.type));
    }
    else
    {
        SC_LOG_MESSAGE("{} {} COMPLETE (Error = \"{}\")\n", async.debugName, AsyncRequest::TypeToString(async.type),
                       returnCode.message);
    }
    return Internal::applyOnAsync(
        async, CompleteAsyncPhase(eventIndex, eventLoop, kernelEvents, returnCode, hasBeenReactivated));
}

SC::Result SC::AsyncEventLoop::Internal::cancelAsync(AsyncEventLoop& eventLoop, KernelEvents& kernelEvents,
                                                     AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} CANCEL\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_TRY(Internal::applyOnAsync(async, CancelAsyncPhase{eventLoop, kernelEvents}))
    if (async.state == AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::stop(AsyncEventLoop& eventLoop, AsyncRequest& async,
                                              Function<void(AsyncResult&)>* onClose)
{
    SC_LOG_MESSAGE("{} {} STOP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    async.closeCallback = onClose;
    switch (async.state)
    {
    case AsyncRequest::State::Active: {
        // Request is active so it needs cancelAsync and teardown before pushing it to cancellation queue
        removeActiveHandle(async);
        async.state = AsyncRequest::State::Cancelling;
        if (async.flags & Internal::Flag_ManualCompletion)
        {
            manualCompletions.remove(async);
            async.flags &= ~Internal::Flag_ManualCompletion;
        }
        submissions.queueBack(async);
        numberOfSubmissions += 1;
    }
    break;
    case AsyncRequest::State::Setup: {
        // Request was not even setup, so it can go straight to cancellation queue
        async.state = AsyncRequest::State::Cancelling;
        numberOfSubmissions -= 1;
        submissions.remove(async);
        pushToCancellationQueue(async);
    }
    break;
    case AsyncRequest::State::Submitting:
    case AsyncRequest::State::Reactivate: {
        // Request was setup so teardown must be done before pushing it to cancellation queue
        AsyncTeardown teardown;
        prepareTeardown(eventLoop, async, teardown);
        SC_TRY(teardownAsync(teardown));
        async.state = AsyncRequest::State::Cancelling;
        numberOfSubmissions -= 1;
        submissions.remove(async);
        pushToCancellationQueue(async);
    }
    break;
    case AsyncRequest::State::Free:
        // TODO: Not sure if we should error out here
        return SC::Result::Error("Trying to stop AsyncRequest that is not active");
    case AsyncRequest::State::Cancelling:
        // Already Cancelling, but now we update the stop function
        break;
    }
    return Result(true);
}

void SC::AsyncEventLoop::Internal::updateTime()
{
    Time::Monotonic newTime = Time::Monotonic::now();
    SC_ASSERT_RELEASE(newTime.isLaterThanOrEqualTo(loopTime));
    loopTime = newTime;
}

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread(AsyncLoopWakeUp& async)
{
    SC_ASSERT_DEBUG(async.type == AsyncRequest::Type::LoopWakeUp);
    AsyncLoopWakeUp& notifier = *static_cast<AsyncLoopWakeUp*>(&async);
    if (not notifier.pending.exchange(true))
    {
        return wakeUpFromExternalThread();
    }
    return Result(true);
}

void SC::AsyncEventLoop::Internal::executeWakeUps(AsyncEventLoop& eventLoop)
{
    AsyncLoopWakeUp* async = activeLoopWakeUps.front;
    while (async != nullptr)
    {
        SC_ASSERT_DEBUG(async->type == AsyncRequest::Type::LoopWakeUp);
        AsyncLoopWakeUp* current = async;
        async                    = static_cast<AsyncLoopWakeUp*>(async->next);
        if (current->pending.load() == true)
        {
            Result                  res(true);
            AsyncLoopWakeUp::Result result(eventLoop, *current, res);
            removeActiveHandle(*current);
            current->callback(result);
            if (current->eventObject)
            {
                current->eventObject->signal();
            }
            current->pending.exchange(false); // allow executing the notification again
        }
    }
    wakeUpPending.exchange(false);
}

void SC::AsyncEventLoop::Internal::removeActiveHandle(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Active);
    async.state = AsyncRequest::State::Free;

    if (async.sequence)
    {
        async.sequence->runningAsync = false;
    }

    if ((async.flags & Internal::Flag_ManualCompletion) != 0)
    {
        numberOfManualCompletions -= 1;
        return; // Async flagged to be manually completed, are not added to active handles and do not count as active
    }

    numberOfActiveHandles -= 1;

    if (async.sequence)
    {
        return; // Async flagged to be manually completed for thread pool, are not added to active handles
    }
    // clang-format off
    switch (async.type)
    {
        case AsyncRequest::Type::LoopTimeout:   activeLoopTimeouts.remove(*static_cast<AsyncLoopTimeout*>(&async));     break;
        case AsyncRequest::Type::LoopWakeUp:    activeLoopWakeUps.remove(*static_cast<AsyncLoopWakeUp*>(&async));       break;
        case AsyncRequest::Type::LoopWork:      activeLoopWork.remove(*static_cast<AsyncLoopWork*>(&async));            break;
        case AsyncRequest::Type::ProcessExit:   activeProcessExits.remove(*static_cast<AsyncProcessExit*>(&async));     break;
        case AsyncRequest::Type::SocketAccept:  activeSocketAccepts.remove(*static_cast<AsyncSocketAccept*>(&async));   break;
        case AsyncRequest::Type::SocketConnect: activeSocketConnects.remove(*static_cast<AsyncSocketConnect*>(&async)); break;
        case AsyncRequest::Type::SocketSend:    activeSocketSends.remove(*static_cast<AsyncSocketSend*>(&async));       break;
        case AsyncRequest::Type::SocketSendTo:  activeSocketSendsTo.remove(*static_cast<AsyncSocketSendTo*>(&async));   break;
        case AsyncRequest::Type::SocketReceive: activeSocketReceives.remove(*static_cast<AsyncSocketReceive*>(&async)); break;
        case AsyncRequest::Type::SocketReceiveFrom: activeSocketReceivesFrom.remove(*static_cast<AsyncSocketReceiveFrom*>(&async)); break;
        case AsyncRequest::Type::FileRead:      activeFileReads.remove(*static_cast<AsyncFileRead*>(&async));           break;
        case AsyncRequest::Type::FileWrite:     activeFileWrites.remove(*static_cast<AsyncFileWrite*>(&async));         break;
        case AsyncRequest::Type::FilePoll:      activeFilePolls.remove(*static_cast<AsyncFilePoll*>(&async));           break;

        // FileSystemOperation
        case AsyncRequest::Type::FileSystemOperation: activeFileSystemOperations.remove(*static_cast<AsyncFileSystemOperation*>(&async)); break;
    }
    // clang-format on
}

void SC::AsyncEventLoop::Internal::addActiveHandle(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Submitting);
    async.state = AsyncRequest::State::Active;

    if ((async.flags & Internal::Flag_ManualCompletion) != 0)
    {
        numberOfManualCompletions += 1;
        return; // Async flagged to be manually completed, are not added to active handles
    }

    numberOfActiveHandles += 1;

    if (async.sequence)
    {
        return; // Async flagged to be manually completed for thread pool, are not added to active handles
    }
    switch (async.type)
    {
    case AsyncRequest::Type::LoopTimeout: {
        // Timeouts needs to be ordered
        AsyncLoopTimeout& timeout = *static_cast<AsyncLoopTimeout*>(&async);

        AsyncLoopTimeout* iterator = activeLoopTimeouts.front;
        // TODO: Replace code below with a heap or some sorted data structure...
        while (iterator)
        {
            // isLaterThan ensures items with same expiration time to be sub-ordered by their scheduling order
            if (iterator->expirationTime.isLaterThan(timeout.expirationTime))
            {
                // middle
                timeout.prev = iterator->prev;
                timeout.next = iterator;
                if (timeout.prev)
                {
                    timeout.prev->next = &timeout;
                }
                else
                {
                    activeLoopTimeouts.front = &timeout;
                }
                if (timeout.next)
                {
                    timeout.next->prev = &timeout;
                }
                else
                {
                    activeLoopTimeouts.back = &timeout;
                }
                break;
            }
            iterator = static_cast<AsyncLoopTimeout*>(iterator->next);
        }
        if (iterator == nullptr)
        {
            activeLoopTimeouts.queueBack(timeout);
        }
    }
    break;
        // clang-format off
    case AsyncRequest::Type::LoopWakeUp:    activeLoopWakeUps.queueBack(*static_cast<AsyncLoopWakeUp*>(&async));        break;
    case AsyncRequest::Type::LoopWork:      activeLoopWork.queueBack(*static_cast<AsyncLoopWork*>(&async));             break;
    case AsyncRequest::Type::ProcessExit:   activeProcessExits.queueBack(*static_cast<AsyncProcessExit*>(&async));      break;
    case AsyncRequest::Type::SocketAccept:  activeSocketAccepts.queueBack(*static_cast<AsyncSocketAccept*>(&async));    break;
    case AsyncRequest::Type::SocketConnect: activeSocketConnects.queueBack(*static_cast<AsyncSocketConnect*>(&async));  break;
    case AsyncRequest::Type::SocketSend:    activeSocketSends.queueBack(*static_cast<AsyncSocketSend*>(&async));        break;
    case AsyncRequest::Type::SocketSendTo:  activeSocketSendsTo.queueBack(*static_cast<AsyncSocketSendTo*>(&async));    break;
    case AsyncRequest::Type::SocketReceive: activeSocketReceives.queueBack(*static_cast<AsyncSocketReceive*>(&async));  break;
    case AsyncRequest::Type::SocketReceiveFrom: activeSocketReceivesFrom.queueBack(*static_cast<AsyncSocketReceiveFrom*>(&async));  break;
    case AsyncRequest::Type::FileRead:      activeFileReads.queueBack(*static_cast<AsyncFileRead*>(&async));            break;
    case AsyncRequest::Type::FileWrite:     activeFileWrites.queueBack(*static_cast<AsyncFileWrite*>(&async));          break;
    case AsyncRequest::Type::FilePoll: 	    activeFilePolls.queueBack(*static_cast<AsyncFilePoll*>(&async));            break;

    // FileSystemOperation
    case AsyncRequest::Type::FileSystemOperation: activeFileSystemOperations.queueBack(*static_cast<AsyncFileSystemOperation*>(&async)); break;
    }
    // clang-format on
}

void SC::AsyncEventLoop::Internal::scheduleManualCompletion(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Setup or async.state == AsyncRequest::State::Submitting);
    manualCompletions.queueBack(async);
}

template <typename Lambda>
SC::Result SC::AsyncEventLoop::Internal::applyOnAsync(AsyncRequest& async, Lambda&& lambda)
{
    switch (async.type)
    {
    case AsyncRequest::Type::LoopTimeout: SC_TRY(lambda(*static_cast<AsyncLoopTimeout*>(&async))); break;
    case AsyncRequest::Type::LoopWakeUp: SC_TRY(lambda(*static_cast<AsyncLoopWakeUp*>(&async))); break;
    case AsyncRequest::Type::LoopWork: SC_TRY(lambda(*static_cast<AsyncLoopWork*>(&async))); break;
    case AsyncRequest::Type::ProcessExit: SC_TRY(lambda(*static_cast<AsyncProcessExit*>(&async))); break;
    case AsyncRequest::Type::SocketAccept: SC_TRY(lambda(*static_cast<AsyncSocketAccept*>(&async))); break;
    case AsyncRequest::Type::SocketConnect: SC_TRY(lambda(*static_cast<AsyncSocketConnect*>(&async))); break;
    case AsyncRequest::Type::SocketSend: SC_TRY(lambda(*static_cast<AsyncSocketSend*>(&async))); break;
    case AsyncRequest::Type::SocketSendTo: SC_TRY(lambda(*static_cast<AsyncSocketSendTo*>(&async))); break;
    case AsyncRequest::Type::SocketReceive: SC_TRY(lambda(*static_cast<AsyncSocketReceive*>(&async))); break;
    case AsyncRequest::Type::SocketReceiveFrom: SC_TRY(lambda(*static_cast<AsyncSocketReceiveFrom*>(&async))); break;
    case AsyncRequest::Type::FileRead: SC_TRY(lambda(*static_cast<AsyncFileRead*>(&async))); break;
    case AsyncRequest::Type::FileWrite: SC_TRY(lambda(*static_cast<AsyncFileWrite*>(&async))); break;
    case AsyncRequest::Type::FilePoll: SC_TRY(lambda(*static_cast<AsyncFilePoll*>(&async))); break;
    case AsyncRequest::Type::FileSystemOperation:
        SC_TRY(lambda(*static_cast<AsyncFileSystemOperation*>(&async)));
        break;
    }
    return SC::Result(true);
}

template <>
void SC::AsyncEventLoop::Internal::KernelQueueOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::AsyncEventLoop::Internal::KernelQueueOpaque::destruct(Object& obj)
{
    obj.~Object();
}

//-------------------------------------------------------------------------------------------------------
// AsyncCompletionVariant
//-------------------------------------------------------------------------------------------------------

void SC::detail::AsyncCompletionVariant::destroy()
{
    if (not inited)
        return;
    switch (type)
    {
    case AsyncRequest::Type::LoopWork: dtor(completionDataLoopWork); break;
    case AsyncRequest::Type::LoopTimeout: dtor(completionDataLoopTimeout); break;
    case AsyncRequest::Type::LoopWakeUp: dtor(completionDataLoopWakeUp); break;
    case AsyncRequest::Type::ProcessExit: dtor(completionDataProcessExit); break;
    case AsyncRequest::Type::SocketAccept: dtor(completionDataSocketAccept); break;
    case AsyncRequest::Type::SocketConnect: dtor(completionDataSocketConnect); break;
    case AsyncRequest::Type::SocketSend: dtor(completionDataSocketSend); break;
    case AsyncRequest::Type::SocketSendTo: dtor(completionDataSocketSendTo); break;
    case AsyncRequest::Type::SocketReceive: dtor(completionDataSocketReceive); break;
    case AsyncRequest::Type::SocketReceiveFrom: dtor(completionDataSocketReceiveFrom); break;
    case AsyncRequest::Type::FileRead: dtor(completionDataFileRead); break;
    case AsyncRequest::Type::FileWrite: dtor(completionDataFileWrite); break;
    case AsyncRequest::Type::FilePoll: dtor(completionDataFilePoll); break;

    // FileSystemOperation
    case AsyncRequest::Type::FileSystemOperation: dtor(completionDataFileSystemOperation); break;
    }
}
