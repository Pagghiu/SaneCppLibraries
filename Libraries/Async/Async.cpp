// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Platform.h"
#include "../Socket/Socket.h"

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

#define SC_ASYNC_ENABLE_LOG 0
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
    case Type::SocketReceive: return "SocketReceive";
    case Type::SocketClose: return "SocketClose";
    case Type::FileRead: return "FileRead";
    case Type::FileWrite: return "FileWrite";
    case Type::FileClose: return "FileClose";
    case Type::FilePoll: return "FilePoll";
    }
    Assert::unreachable();
}
#endif

void SC::AsyncRequest::setDebugName(const char* newDebugName)
{
#if SC_CONFIGURATION_DEBUG
    debugName = newDebugName;
#else
    SC_COMPILER_UNUSED(newDebugName);
#endif
}

SC::Result SC::AsyncRequest::setThreadPoolAndTask(ThreadPool& pool, AsyncTask& task)
{
    if (task.async != nullptr)
    {
        return Result::Error("AsyncTask is bound to a different async being started");
    }
    task.threadPool = &pool;
    asyncTask       = &task;
    task.async      = this;
    return Result(true);
}

void SC::AsyncRequest::resetThreadPoolAndTask()
{
    if (asyncTask)
    {
        asyncTask->async      = nullptr;
        asyncTask->threadPool = nullptr;
        asyncTask             = nullptr;
    }
}

SC::Result SC::AsyncRequest::validateAsync()
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

void SC::AsyncRequest::queueSubmission(AsyncEventLoop& loop) { loop.internal.queueSubmission(loop, *this); }

SC::Result SC::AsyncRequest::stop(Function<void(AsyncResult&)>* onClose)
{
    if (eventLoop)
        return eventLoop->internal.stop(*eventLoop, *this, onClose);
    return SC::Result::Error("stop failed. eventLoop is nullptr");
}

bool SC::AsyncRequest::isFree() const { return state == State::Free; }

bool SC::AsyncRequest::isCancelling() const { return state == State::Cancelling; }

bool SC::AsyncRequest::isActive() const { return state == State::Active or state == State::Reactivate; }

//-------------------------------------------------------------------------------------------------------
// AsyncResult
//-------------------------------------------------------------------------------------------------------

void SC::AsyncResult::reactivateRequest(bool value)
{
    shouldBeReactivated = value;
    if (shouldBeReactivated)
    {
        async.state = AsyncRequest::State::Reactivate;
    }
    else if (async.state == AsyncRequest::State::Reactivate)
    {
        async.state = AsyncRequest::State::Free;
    }
}

//-------------------------------------------------------------------------------------------------------
// Async***
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncLoopTimeout::start(AsyncEventLoop& loop)
{
    SC_TRY(validateAsync());
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncLoopTimeout::start(AsyncEventLoop& loop, Time::Milliseconds timeout)
{
    relativeTimeout = timeout;
    return start(loop);
}

SC::Result SC::AsyncLoopWakeUp::start(AsyncEventLoop& loop, EventObject* eo)
{
    SC_TRY(validateAsync());
    eventObject = eo;
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWakeUp::wakeUp() { return getEventLoop()->wakeUpFromExternalThread(*this); }

SC::Result SC::AsyncLoopWork::start(AsyncEventLoop& loop)
{
    SC_TRY_MSG(work.isValid(), "AsyncLoopWork::start - Invalid work callback");
    SC_TRY_MSG(asyncTask != nullptr, "AsyncLoopWork::start - setThreadPool not called");
    SC_TRY(validateAsync());
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWork::setThreadPool(ThreadPool& threadPool) { return setThreadPoolAndTask(threadPool, task); }

SC::Result SC::AsyncProcessExit::start(AsyncEventLoop& loop, ProcessDescriptor::Handle process)
{
    handle = process;
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncSocketAccept::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(socketDescriptor.getAddressFamily(addressFamily));
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncSocketConnect::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                         SocketIPAddress socketIpAddress)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    ipAddress = socketIpAddress;
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncSocketSend::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                      Span<const char> dataToSend)
{
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer = dataToSend;
    return start(loop);
}

SC::Result SC::AsyncSocketSend::start(AsyncEventLoop& loop)
{
    SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncSocketSend::start - Zero sized write buffer");
    SC_TRY_MSG(handle != SocketDescriptor::Invalid, "AsyncSocketSend::start - Invalid file descriptor");
    SC_TRY(validateAsync());
#if SC_PLATFORM_WINDOWS
#else
    totalBytesSent = 0;
#endif
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncSocketReceive::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                         Span<char> receiveData)
{
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer = receiveData;
    return start(loop);
}

SC::Result SC::AsyncSocketReceive::start(AsyncEventLoop& loop)
{
    SC_TRY(validateAsync());
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncSocketClose::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncFileRead::start(AsyncEventLoop& loop)
{
    SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncFileRead::start - Zero sized read buffer");
    SC_TRY_MSG(fileDescriptor != FileDescriptor::Invalid, "AsyncFileRead::start - Invalid file descriptor");
    SC_TRY(validateAsync());
    // Only use the async tasks for operations and backends that are not io_uring
    if (not loop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        resetThreadPoolAndTask();
    }
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncFileWrite::start(AsyncEventLoop& loop)
{
    SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncFileWrite::start - Zero sized write buffer");
    SC_TRY_MSG(fileDescriptor != FileDescriptor::Invalid, "AsyncFileWrite::start - Invalid file descriptor");
    SC_TRY(validateAsync());
    // Only use the async tasks for operations and backends that are not io_uring
    if (not loop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        resetThreadPoolAndTask();
    }
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncFileClose::start(AsyncEventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    fileDescriptor = fd;
    queueSubmission(loop);
    return SC::Result(true);
}

SC::Result SC::AsyncFilePoll::start(AsyncEventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    fileDescriptor = fd;
    queueSubmission(loop);
    return SC::Result(true);
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
    while (internal.getTotalNumberOfActiveHandle() != 0 or not internal.submissions.isEmpty())
    {
        SC_TRY(runOnce());
        if (internal.interrupted)
        {
            internal.interrupted = false;
            break;
        }
    };
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
    return associateExternallyCreatedTCPSocket(outDescriptor);
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

SC::Result SC::AsyncEventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor)
{
    return internal.kernelQueue.get().associateExternallyCreatedTCPSocket(outDescriptor);
}

SC::Result SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor)
{
    return internal.kernelQueue.get().associateExternallyCreatedFileDescriptor(outDescriptor);
}
void SC::AsyncEventLoop::updateTime() { return internal.updateTime(); }

SC::Time::HighResolutionCounter SC::AsyncEventLoop::getLoopTime() const { return internal.loopTime; }

int SC::AsyncEventLoop::getNumberOfActiveRequests() const { return internal.getTotalNumberOfActiveHandle(); }

int SC::AsyncEventLoop::getNumberOfSubmittedRequests() const { return internal.numberOfSubmissions; }

void SC::AsyncEventLoop::excludeFromActiveCount(AsyncRequest& async)
{
    if (not async.isFree() and not isExcludedFromActiveCount(async))
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
    internal.enumerateRequests(internal.submissions, enumerationCallback);
    internal.enumerateRequests(internal.activeLoopTimeouts, enumerationCallback);
    internal.enumerateRequests(internal.activeLoopWakeUps, enumerationCallback);
    internal.enumerateRequests(internal.activeProcessExits, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketAccepts, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketConnects, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketSends, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketReceives, enumerationCallback);
    internal.enumerateRequests(internal.activeSocketCloses, enumerationCallback);
    internal.enumerateRequests(internal.activeFileReads, enumerationCallback);
    internal.enumerateRequests(internal.activeFileWrites, enumerationCallback);
    internal.enumerateRequests(internal.activeFileCloses, enumerationCallback);
    internal.enumerateRequests(internal.activeFilePolls, enumerationCallback);
    internal.enumerateRequests(internal.manualCompletions, enumerationCallback);
}

void SC::AsyncEventLoop::setListeners(AsyncEventLoopListeners* listeners) { internal.listeners = listeners; }

#if SC_PLATFORM_LINUX
#else
bool SC::AsyncEventLoop::tryLoadingLiburing() { return false; }
#endif

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
        SC_TRY(eventLoopWakeUp.wakeUp());
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
    SC_TRY(eventLoopWakeUp.stop());
    eventLoop = nullptr;
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// AsyncEventLoop::Internal
//-------------------------------------------------------------------------------------------------------

void SC::AsyncEventLoop::Internal::queueSubmission(AsyncEventLoop& loop, AsyncRequest& async)
{
    async.eventLoop = &loop;
    async.state     = AsyncRequest::State::Setup;
    submissions.queueBack(async);
    numberOfSubmissions += 1;
}

SC::AsyncLoopTimeout* SC::AsyncEventLoop::Internal::findEarliestLoopTimeout() const
{
    AsyncLoopTimeout* earliestTime = nullptr;
    for (AsyncRequest* async = activeLoopTimeouts.front; async != nullptr; async = async->next)
    {
        SC_ASSERT_DEBUG(async->type == AsyncRequest::Type::LoopTimeout);
        const auto& expirationTime = static_cast<AsyncLoopTimeout*>(async)->expirationTime;
        if (earliestTime == nullptr or earliestTime->expirationTime.isLaterThanOrEqualTo(expirationTime))
        {
            earliestTime = static_cast<AsyncLoopTimeout*>(async);
        }
    };
    return earliestTime;
}

void SC::AsyncEventLoop::Internal::invokeExpiredTimers(Time::HighResolutionCounter currentTime)
{
    AsyncLoopTimeout* async = activeLoopTimeouts.front;
    while (async != nullptr)
    {
        AsyncLoopTimeout* current = async;
        async                     = static_cast<AsyncLoopTimeout*>(async->next);
        if (currentTime.isLaterThanOrEqualTo(current->expirationTime))
        {
            removeActiveHandle(*current);
            AsyncLoopTimeout::Result result(*current, Result(true));
            if (current->callback.isValid())
            {
                current->callback(result);
            }

            if (result.shouldBeReactivated and current->state == AsyncRequest::State::Reactivate)
            {
                current->state = AsyncRequest::State::Submitting;
                submissions.queueBack(*current);
                numberOfSubmissions += 1;
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
    }
}

template <typename T>
void SC::AsyncEventLoop::Internal::stopRequests(IntrusiveDoubleLinkedList<T>& linkedList)
{
    auto async = linkedList.front;
    while (async != nullptr)
    {
        auto asyncNext = static_cast<T*>(async->next);
        if (!async->isCancelling() and not async->isFree())
        {
            Result res = async->stop();
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
        if (it->asyncTask != nullptr)
        {
            if (not it->asyncTask->threadPool->waitForTask(it->asyncTask->task))
            {
                res = Result::Error("Threadpool was already stopped");
            }
            it->asyncTask->freeTask();
        }
    }
    return res;
}

SC::Result SC::AsyncEventLoop::Internal::close(AsyncEventLoop& loop)
{
    Result res = Result(true);

    // Wait for all thread pool tasks
    Result threadPoolRes1 = waitForThreadPoolTasks(activeFileReads);
    Result threadPoolRes2 = waitForThreadPoolTasks(activeFileWrites);

    if (not threadPoolRes1)
        res = threadPoolRes1;

    if (not threadPoolRes2)
        res = threadPoolRes2;

    // TODO: Consolidate this list with enumerateRequests
    stopRequests(submissions);

    while (AsyncRequest* async = manualThreadPoolCompletions.pop())
    {
        Result stopRes = async->stop();
        (void)stopRes;
        SC_ASSERT_DEBUG(stopRes);
    }

    stopRequests(activeLoopTimeouts);
    stopRequests(activeLoopWakeUps);
    stopRequests(activeProcessExits);
    stopRequests(activeSocketAccepts);
    stopRequests(activeSocketConnects);
    stopRequests(activeSocketSends);
    stopRequests(activeSocketReceives);
    stopRequests(activeSocketCloses);
    stopRequests(activeFileReads);
    stopRequests(activeFileWrites);
    stopRequests(activeFileCloses);
    stopRequests(activeFilePolls);

    stopRequests(manualCompletions);

    SC_TRY(loop.run());
    SC_TRY(loop.internal.kernelQueue.get().close());

    if (numberOfExternals != 0 or numberOfActiveHandles != 0 or numberOfManualCompletions != 0)
    {
        return Result::Error("Non-Zero active count after close");
    }
    return res;
}

SC::Result SC::AsyncEventLoop::Internal::stageSubmission(KernelEvents& kernelEvents, AsyncRequest& async)
{
    switch (async.state)
    {
    case AsyncRequest::State::Setup: {
        SC_TRY(setupAsync(kernelEvents, async));
        async.state = AsyncRequest::State::Submitting;
        SC_TRY(activateAsync(kernelEvents, async));
    }
    break;
    case AsyncRequest::State::Submitting: {
        SC_TRY(activateAsync(kernelEvents, async));
    }
    break;
    case AsyncRequest::State::Reactivate:
    case AsyncRequest::State::Free: {
        // TODO: Stop the completion, it has been cancelled before being submitted
        SC_ASSERT_RELEASE(false);
    }
    break;
    case AsyncRequest::State::Cancelling: {
        SC_TRY(cancelAsync(kernelEvents, async));
        AsyncTeardown teardown;
        prepareTeardown(async, teardown);
        SC_TRY(teardownAsync(teardown));
        async.markAsFree(); // This may still come up in kernel events
        if (async.closeCallback)
        {
            cancellations.queueBack(async);
        }
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

struct SC::AsyncEventLoop::Internal::ReactivateAsyncPhase
{
    template <typename T>
    SC::Result operator()(T& async)
    {
        async.state = AsyncRequest::State::Submitting;
        if (KernelEvents::needsSubmissionWhenReactivating(async))
        {
            async.eventLoop->internal.submissions.queueBack(async);
            async.eventLoop->internal.numberOfSubmissions += 1;
        }
        else
        {
            async.eventLoop->internal.addActiveHandle(async);
        }
        return Result(true);
    }
};

SC::Result SC::AsyncEventLoop::Internal::completeAndEventuallyReactivate(KernelEvents& kernelEvents,
                                                                         AsyncRequest& async, Result&& returnCode)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Active);
    bool reactivate = false;
    removeActiveHandle(async);
    AsyncTeardown teardown;
    prepareTeardown(async, teardown);
    SC_TRY(completeAsync(kernelEvents, async, move(returnCode), reactivate));
    if (reactivate and async.state == AsyncRequest::State::Reactivate)
    {
        SC_TRY(Internal::applyOnAsync(async, ReactivateAsyncPhase()));
    }
    else
    {
        SC_TRY(teardownAsync(teardown));
    }
    if (not returnCode)
    {
        // TODO: We shouldn't probably access async if it has not been reactivated...
        reportError(kernelEvents, async, move(returnCode));
    }
    return Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::runStep(AsyncEventLoop& loop, SyncMode syncMode)
{
    alignas(uint64_t) uint8_t buffer[8 * 1024]; // 8 Kb of kernel events
    AsyncKernelEvents         kernelEvents;
    kernelEvents.eventsMemory = buffer;
    SC_TRY(submitRequests(loop, kernelEvents));
    SC_TRY(blockingPoll(loop, syncMode, kernelEvents));
    return dispatchCompletions(loop, syncMode, kernelEvents);
}

SC::Result SC::AsyncEventLoop::Internal::submitRequests(AsyncEventLoop& loop, AsyncKernelEvents& asyncKernelEvents)
{
    KernelEvents kernelEvents(loop.internal.kernelQueue.get(), asyncKernelEvents);
    asyncKernelEvents.numberOfEvents = 0;
    // TODO: Check if it's possible to avoid zeroing kernel events memory
    memset(asyncKernelEvents.eventsMemory.data(), 0, asyncKernelEvents.eventsMemory.sizeInBytes());
    SC_LOG_MESSAGE("---------------\n");

    updateTime();
    while (AsyncRequest* async = submissions.dequeueFront())
    {
        numberOfSubmissions -= 1;
        auto res = stageSubmission(kernelEvents, *async);
        if (not res)
        {
            reportError(kernelEvents, *async, move(res));
        }
    }

    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::blockingPoll(AsyncEventLoop& loop, SyncMode syncMode,
                                                      AsyncKernelEvents& asyncKernelEvents)
{
    if (listeners and listeners->beforeBlockingPoll.isValid())
    {
        listeners->beforeBlockingPoll(loop);
    }
    KernelEvents kernelEvents(loop.internal.kernelQueue.get(), asyncKernelEvents);
    const auto   numActiveHandles = getTotalNumberOfActiveHandle();
    SC_ASSERT_RELEASE(numActiveHandles >= 0);
    if (numActiveHandles > 0 or numberOfManualCompletions != 0)
    {
        // We may have some manualCompletions queued (for SocketClose for example) but no active handles
        SC_LOG_MESSAGE("Active Requests Before Poll = {}\n", getTotalNumberOfActiveHandle());

        // If there are manual completions the loop can't block waiting for I/O, to dispatch them immediately
        const bool canBlockForIO = numberOfManualCompletions == 0;
        SC_TRY(kernelEvents.syncWithKernel(loop, canBlockForIO ? syncMode : SyncMode::NoWait));
        SC_LOG_MESSAGE("Active Requests After Poll = {}\n", getTotalNumberOfActiveHandle());
    }
    if (listeners and listeners->afterBlockingPoll.isValid())
    {
        listeners->afterBlockingPoll(loop);
    }
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::dispatchCompletions(AsyncEventLoop& loop, SyncMode syncMode,
                                                             AsyncKernelEvents& asyncKernelEvents)
{
    executeCancellationCallbacks();
    KernelEvents kernelEvents(loop.internal.kernelQueue.get(), asyncKernelEvents);
    switch (syncMode)
    {
    case SyncMode::NoWait: {
        // No need to update time as it was already updated in submitRequests and syncing
        // with kernel has not been blocking (as we are in NoWait mode)
        if (kernelEvents.needsManualTimersProcessing())
        {
            invokeExpiredTimers(loopTime);
        }
    }
    break;
    case SyncMode::ForcedForwardProgress: {
        // Update loop time unconditionally after a (potentially blocking) sync kernel operation
        updateTime();
        if (runTimers)
        {
            runTimers = false;
            invokeExpiredTimers(loopTime);
        }
    }
    break;
    }
    runStepExecuteCompletions(kernelEvents);
    runStepExecuteManualCompletions(kernelEvents);
    runStepExecuteManualThreadPoolCompletions(kernelEvents);

    SC_LOG_MESSAGE("Active Requests After Completion = {} ( + {} manual)\n", getTotalNumberOfActiveHandle(),
                   numberOfManualCompletions);
    return SC::Result(true);
}

void SC::AsyncEventLoop::Internal::executeCancellationCallbacks()
{
    while (AsyncRequest* async = cancellations.dequeueFront())
    {
        AsyncResult res(*async);
        (*async->closeCallback)(res);
    }
}

void SC::AsyncEventLoop::Internal::runStepExecuteManualCompletions(KernelEvents& kernelEvents)
{
    while (AsyncRequest* async = manualCompletions.dequeueFront())
    {
        if (not completeAndEventuallyReactivate(kernelEvents, *async, Result(true)))
        {
            SC_LOG_MESSAGE("Error completing {}", async->debugName);
        }
    }
}

void SC::AsyncEventLoop::Internal::runStepExecuteManualThreadPoolCompletions(KernelEvents& kernelEvents)
{
    while (AsyncRequest* async = manualThreadPoolCompletions.pop())
    {
        if (not completeAndEventuallyReactivate(kernelEvents, *async, Result(true)))
        {
            SC_LOG_MESSAGE("Error completing {}", async->debugName);
        }
    }
}

void SC::AsyncEventLoop::Internal::runStepExecuteCompletions(KernelEvents& kernelEvents)
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

        AsyncRequest& async  = *request;
        Result        result = Result(kernelEvents.validateEvent(idx, continueProcessing));
        if (not result)
        {
            reportError(kernelEvents, async, move(result));
            continue;
        }

        if (not continueProcessing)
        {
            continue;
        }
        async.eventIndex = static_cast<int32_t>(idx);
        if (async.state == AsyncRequest::State::Active)
        {
            if (not completeAndEventuallyReactivate(kernelEvents, async, move(result)))
            {
                SC_LOG_MESSAGE("Error completing {}", async.debugName);
            }
        }
        else
        {
            // We cannot assert that this is free because it may happen to get one
            // more kernel event if it has been marked as free inside stageSubmission
            // SC_ASSERT_RELEASE(async.state != AsyncRequest::State::Free);

            // An async that is in cancelling state here, means it's also in submission
            // queue and it must stay there to continue with teardown.
            if (async.state != AsyncRequest::State::Cancelling)
            {
                async.markAsFree();
            }
        }
    }
}

struct SC::AsyncEventLoop::Internal::SetupAsyncPhase
{
    KernelEvents& kernelEvents;
    SetupAsyncPhase(KernelEvents& kernelEvents) : kernelEvents(kernelEvents) {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        if (async.asyncTask)
        {
            return Result(true);
        }
        else
        {
            return Result(kernelEvents.setupAsync(async));
        }
    }
};

struct SC::AsyncEventLoop::Internal::ActivateAsyncPhase
{
    KernelEvents& kernelEvents;
    ActivateAsyncPhase(KernelEvents& kernelEvents) : kernelEvents(kernelEvents) {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        if (async.asyncTask)
        {
            AsyncTask* asyncTask     = async.asyncTask;
            asyncTask->task.function = [&async] { executeThreadPoolOperation(async); };
            return asyncTask->threadPool->queueTask(asyncTask->task);
        }

        if (async.flags & Internal::Flag_ManualCompletion)
        {
            async.eventLoop->internal.scheduleManualCompletion(async);
        }
        return Result(kernelEvents.activateAsync(async));
    }

    template <typename T>
    static void executeThreadPoolOperation(T& async)
    {
        AsyncTask& task           = *async.asyncTask;
        auto&      completionData = static_cast<typename T::CompletionData&>(task.completionData);
        task.returnCode           = KernelEvents::executeOperation(async, completionData);
        async.eventLoop->internal.manualThreadPoolCompletions.push(async);
        SC_ASSERT_RELEASE(async.eventLoop->wakeUpFromExternalThread());
    }
};

struct SC::AsyncEventLoop::Internal::CancelAsyncPhase
{
    KernelEvents& kernelEvents;
    CancelAsyncPhase(KernelEvents& kernelEvents) : kernelEvents(kernelEvents) {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        if (async.asyncTask)
        {
            // Waiting here is not ideal but we need it to be able to reliably know that
            // the task can be reused soon after cancelling an async that uses it.
            SC_TRY(async.asyncTask->threadPool->waitForTask(async.asyncTask->task));

            // Prevent this async from going in the CompleteAsyncPhase and mark task as free
            async.eventLoop->internal.manualThreadPoolCompletions.remove(async);
            async.asyncTask->freeTask();
            return Result(true);
        }

        if (async.flags & Internal::Flag_ManualCompletion)
        {
            async.eventLoop->internal.manualCompletions.remove(async);
            return Result(true);
        }
        else
        {
            return Result(kernelEvents.cancelAsync(async));
        }
    }
};

struct SC::AsyncEventLoop::Internal::CompleteAsyncPhase
{
    KernelEvents& kernelEvents;

    Result&& returnCode;
    bool&    reactivate;

    CompleteAsyncPhase(KernelEvents& kernelEvents, Result&& result, bool& doReactivate)
        : kernelEvents(kernelEvents), returnCode(move(result)), reactivate(doReactivate)
    {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        using AsyncType       = T;
        using AsyncResultType = typename AsyncType::Result;
        using AsyncCompletion = typename AsyncType::CompletionData;
        AsyncResultType result(async, forward<Result>(returnCode));
        if (result.returnCode)
        {
            if (result.getAsync().asyncTask)
            {
                AsyncTask* asyncTask  = result.getAsync().asyncTask;
                result.returnCode     = asyncTask->returnCode;
                result.completionData = move(static_cast<AsyncCompletion&>(asyncTask->completionData));
                // The task is already finished but we need waitForTask to make it available for next runs.
                SC_TRY(asyncTask->threadPool->waitForTask(asyncTask->task));
                asyncTask->freeTask();
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
        reactivate = result.shouldBeReactivated;
        return SC::Result(true);
    }
};

void SC::AsyncEventLoop::Internal::prepareTeardown(AsyncRequest& async, AsyncTeardown& teardown)
{
    teardown.eventLoop = async.eventLoop;
    teardown.type      = async.type;
    teardown.flags     = async.flags;
#if SC_CONFIGURATION_DEBUG
#if SC_COMPILER_MSVC
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
    case AsyncRequest::Type::SocketReceive: teardown.socketHandle = static_cast<AsyncSocketReceive&>(async).handle; break;
    case AsyncRequest::Type::SocketClose:   teardown.socketHandle = static_cast<AsyncSocketClose&>(async).handle; break;

    // File
    case AsyncRequest::Type::FileRead:      teardown.fileHandle = static_cast<AsyncFileRead&>(async).fileDescriptor; break;
    case AsyncRequest::Type::FileWrite:     teardown.fileHandle = static_cast<AsyncFileWrite&>(async).fileDescriptor; break;
    case AsyncRequest::Type::FileClose:     teardown.fileHandle = static_cast<AsyncFileClose&>(async).fileDescriptor; break;
    case AsyncRequest::Type::FilePoll:      teardown.fileHandle = static_cast<AsyncFilePoll&>(async).fileDescriptor; break;
    }
    // clang-format on
}
SC::Result SC::AsyncEventLoop::Internal::setupAsync(KernelEvents& kernelEvents, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    // Reset flags that may have been left by previous activations
    async.flags &= ~Flag_ManualCompletion;
    // Note that we're preserving the Flag_ExcludeFromActiveCount
    return Internal::applyOnAsync(async, SetupAsyncPhase(kernelEvents));
}

SC::Result SC::AsyncEventLoop::Internal::activateAsync(KernelEvents& kernelEvents, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} ACTIVATE\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Submitting);
    SC_TRY(Internal::applyOnAsync(async, ActivateAsyncPhase(kernelEvents)));
    async.eventLoop->internal.addActiveHandle(async);
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
    case AsyncRequest::Type::SocketReceive:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncSocketReceive*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::SocketClose:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncSocketClose*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::FileRead:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncFileRead*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::FileWrite:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncFileWrite*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::FileClose:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncFileClose*>(nullptr), teardown));
        break;
    case AsyncRequest::Type::FilePoll:
        SC_TRY(KernelEvents::teardownAsync(static_cast<AsyncFilePoll*>(nullptr), teardown));
        break;
    }

    if ((teardown.flags & Internal::Flag_ExcludeFromActiveCount) != 0)
    {
        numberOfExternals += 1;
    }
    return Result(true);
}

void SC::AsyncEventLoop::Internal::reportError(KernelEvents& kernelEvents, AsyncRequest& async, Result&& returnCode)
{
    SC_LOG_MESSAGE("{} ERROR {}\n", async.debugName, AsyncRequest::TypeToString(async.type));
    bool reactivate = false;
    if (async.state == AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    (void)completeAsync(kernelEvents, async, forward<Result>(returnCode), reactivate);
    async.markAsFree();
}

SC::Result SC::AsyncEventLoop::Internal::completeAsync(KernelEvents& kernelEvents, AsyncRequest& async,
                                                       Result&& returnCode, bool& reactivate)
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
    return Internal::applyOnAsync(async, CompleteAsyncPhase(kernelEvents, move(returnCode), reactivate));
}

SC::Result SC::AsyncEventLoop::Internal::cancelAsync(KernelEvents& kernelEvents, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} CANCEL\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_TRY(Internal::applyOnAsync(async, CancelAsyncPhase(kernelEvents)))
    if (async.state == AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::stop(AsyncEventLoop& loop, AsyncRequest& async,
                                              Function<void(AsyncResult&)>* onClose)
{
    SC_LOG_MESSAGE("{} {} STOP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_TRY_MSG(async.eventLoop == &loop, "Trying to stop AsyncRequest belonging to another Loop");
    async.closeCallback = onClose;
    switch (async.state)
    {
    case AsyncRequest::State::Active: {
        removeActiveHandle(async);
        async.state = AsyncRequest::State::Cancelling;
        submissions.queueBack(async);
        numberOfSubmissions += 1;
        break;
    }
    case AsyncRequest::State::Submitting: {
        async.state = AsyncRequest::State::Cancelling;
        break;
    }
    case AsyncRequest::State::Setup: {
        async.state = AsyncRequest::State::Cancelling;
        break;
    }
    case AsyncRequest::State::Reactivate: {
        async.state = AsyncRequest::State::Cancelling;
        submissions.queueBack(async);
        numberOfSubmissions += 1;
        break;
    }
    case AsyncRequest::State::Free:
        // TODO: Not sure if we should error out here
        return SC::Result::Error("Trying to stop AsyncRequest that is not active");
    case AsyncRequest::State::Cancelling:
        // Already Cancelling, but now we update the stop function
        break;
    }
    return Result(true);
}

void SC::AsyncEventLoop::Internal::updateTime() { loopTime.snap(); }

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread(AsyncLoopWakeUp& async)
{
    SC_TRY_MSG(async.eventLoop == this,
               "AsyncEventLoop::wakeUpFromExternalThread - Wakeup belonging to different AsyncEventLoop");
    SC_ASSERT_DEBUG(async.type == AsyncRequest::Type::LoopWakeUp);
    AsyncLoopWakeUp& notifier = *static_cast<AsyncLoopWakeUp*>(&async);
    if (not notifier.pending.exchange(true))
    {
        return wakeUpFromExternalThread();
    }
    return Result(true);
}

void SC::AsyncEventLoop::Internal::executeWakeUps()
{
    AsyncLoopWakeUp* async = activeLoopWakeUps.front;
    while (async != nullptr)
    {
        SC_ASSERT_DEBUG(async->type == AsyncRequest::Type::LoopWakeUp);
        AsyncLoopWakeUp* current = async;
        async                    = static_cast<AsyncLoopWakeUp*>(async->next);
        if (current->pending.load() == true)
        {
            AsyncLoopWakeUp::Result result(*current, Result(true));
            removeActiveHandle(*current);
            current->callback(result);

            if (result.shouldBeReactivated and current->state == AsyncRequest::State::Reactivate)
            {
                current->state = AsyncRequest::State::Submitting;
                current->eventLoop->internal.addActiveHandle(*current);
            }

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

    if ((async.flags & Internal::Flag_ManualCompletion) != 0)
    {
        numberOfManualCompletions -= 1;
        return; // Async flagged to be manually completed, are not added to active handles and do not count as active
    }

    numberOfActiveHandles -= 1;

    if (async.asyncTask)
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
        case AsyncRequest::Type::SocketReceive: activeSocketReceives.remove(*static_cast<AsyncSocketReceive*>(&async)); break;
        case AsyncRequest::Type::SocketClose:   activeSocketCloses.remove(*static_cast<AsyncSocketClose*>(&async));     break;
        case AsyncRequest::Type::FileRead:      activeFileReads.remove(*static_cast<AsyncFileRead*>(&async));           break;
        case AsyncRequest::Type::FileWrite:     activeFileWrites.remove(*static_cast<AsyncFileWrite*>(&async));         break;
        case AsyncRequest::Type::FileClose:     activeFileCloses.remove(*static_cast<AsyncFileClose*>(&async));         break;
        case AsyncRequest::Type::FilePoll:      activeFilePolls.remove(*static_cast<AsyncFilePoll*>(&async));           break;
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

    if (async.asyncTask)
    {
        return; // Async flagged to be manually completed for thread pool, are not added to active handles
    }
    // clang-format off
    switch (async.type)
    {
        case AsyncRequest::Type::LoopTimeout:   activeLoopTimeouts.queueBack(*static_cast<AsyncLoopTimeout*>(&async));      break;
        case AsyncRequest::Type::LoopWakeUp:    activeLoopWakeUps.queueBack(*static_cast<AsyncLoopWakeUp*>(&async));        break;
        case AsyncRequest::Type::LoopWork:      activeLoopWork.queueBack(*static_cast<AsyncLoopWork*>(&async));             break;
        case AsyncRequest::Type::ProcessExit:   activeProcessExits.queueBack(*static_cast<AsyncProcessExit*>(&async));      break;
        case AsyncRequest::Type::SocketAccept:  activeSocketAccepts.queueBack(*static_cast<AsyncSocketAccept*>(&async));    break;
        case AsyncRequest::Type::SocketConnect: activeSocketConnects.queueBack(*static_cast<AsyncSocketConnect*>(&async));  break;
        case AsyncRequest::Type::SocketSend:    activeSocketSends.queueBack(*static_cast<AsyncSocketSend*>(&async));        break;
        case AsyncRequest::Type::SocketReceive: activeSocketReceives.queueBack(*static_cast<AsyncSocketReceive*>(&async));  break;
        case AsyncRequest::Type::SocketClose:   activeSocketCloses.queueBack(*static_cast<AsyncSocketClose*>(&async));      break;
        case AsyncRequest::Type::FileRead:      activeFileReads.queueBack(*static_cast<AsyncFileRead*>(&async));            break;
        case AsyncRequest::Type::FileWrite:     activeFileWrites.queueBack(*static_cast<AsyncFileWrite*>(&async));          break;
        case AsyncRequest::Type::FileClose:     activeFileCloses.queueBack(*static_cast<AsyncFileClose*>(&async));          break;
        case AsyncRequest::Type::FilePoll: 	    activeFilePolls.queueBack(*static_cast<AsyncFilePoll*>(&async));            break;
    }
    // clang-format on
}

void SC::AsyncEventLoop::Internal::scheduleManualCompletion(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Setup or async.state == AsyncRequest::State::Submitting);
    async.eventLoop->internal.manualCompletions.queueBack(async);
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
    case AsyncRequest::Type::SocketReceive: SC_TRY(lambda(*static_cast<AsyncSocketReceive*>(&async))); break;
    case AsyncRequest::Type::SocketClose: SC_TRY(lambda(*static_cast<AsyncSocketClose*>(&async))); break;
    case AsyncRequest::Type::FileRead: SC_TRY(lambda(*static_cast<AsyncFileRead*>(&async))); break;
    case AsyncRequest::Type::FileWrite: SC_TRY(lambda(*static_cast<AsyncFileWrite*>(&async))); break;
    case AsyncRequest::Type::FileClose: SC_TRY(lambda(*static_cast<AsyncFileClose*>(&async))); break;
    case AsyncRequest::Type::FilePoll: SC_TRY(lambda(*static_cast<AsyncFilePoll*>(&async))); break;
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
