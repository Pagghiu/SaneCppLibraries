// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Platform.h"

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

SC::Result SC::AsyncRequest::validateAsync()
{
    const bool asyncStateIsFree      = state == AsyncRequest::State::Free;
    const bool asyncIsNotOwnedByLoop = eventLoop == nullptr;
    SC_LOG_MESSAGE("{} {} QUEUE\n", debugName, AsyncRequest::TypeToString(type));
    SC_TRY_MSG(asyncStateIsFree, "Trying to stage AsyncRequest that is in use");
    SC_TRY_MSG(asyncIsNotOwnedByLoop, "Trying to add AsyncRequest belonging to another Loop");
    return Result(true);
}

void SC::AsyncRequest::markAsFree()
{
    state     = AsyncRequest::State::Free;
    eventLoop = nullptr;
    flags     = 0;
}

SC::Result SC::AsyncRequest::queueSubmission(AsyncEventLoop& loop)
{
    return loop.internal.queueSubmission(*this, nullptr);
}

SC::Result SC::AsyncRequest::queueSubmission(AsyncEventLoop& loop, ThreadPool& threadPool, AsyncTask& task)
{
    task.threadPool = &threadPool;
    return loop.internal.queueSubmission(*this, &task);
}

void SC::AsyncRequest::updateTime(AsyncEventLoop& loop) { loop.internal.updateTime(); }

SC::Result SC::AsyncRequest::stop()
{
    if (eventLoop)
        return eventLoop->internal.cancelAsync(*this);
    return SC::Result::Error("stop failed. eventLoop is nullptr");
}

//-------------------------------------------------------------------------------------------------------
// Async***
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncLoopTimeout::start(AsyncEventLoop& loop, Time::Milliseconds timeout)
{
    SC_TRY(validateAsync());
    updateTime(loop);
    relativeTimeout = timeout;
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWakeUp::start(AsyncEventLoop& loop, EventObject* eo)
{
    eventObject = eo;
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWakeUp::wakeUp() { return getEventLoop()->wakeUpFromExternalThread(*this); }

SC::Result SC::AsyncLoopWork::start(AsyncEventLoop& loop, ThreadPool& threadPool)
{
    SC_TRY_MSG(work.isValid(), "AsyncLoopWork::start - Invalid work callback");
    return queueSubmission(loop, threadPool, task);
}

SC::Result SC::AsyncProcessExit::start(AsyncEventLoop& loop, ProcessDescriptor::Handle process)
{
    handle = process;
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketAccept::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(socketDescriptor.getAddressFamily(addressFamily));
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketConnect::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                         SocketIPAddress socketIpAddress)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    ipAddress = socketIpAddress;
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketSend::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                      Span<const char> dataToSend)
{

    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer = dataToSend;
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketReceive::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                         Span<char> receiveData)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer = receiveData;
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketClose::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncFileRead::start(AsyncEventLoop& loop)
{
    SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncFileRead::start - Zero sized read buffer");
    SC_TRY_MSG(fileDescriptor != FileDescriptor::Invalid, "AsyncFileRead::start - Invalid file descriptor");
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncFileRead::start(AsyncEventLoop& loop, ThreadPool& threadPool, Task& task)
{
    SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncFileRead::start - Zero sized read buffer");
    SC_TRY_MSG(fileDescriptor != FileDescriptor::Invalid, "AsyncFileRead::start - Invalid file descriptor");
    SC_TRY(validateAsync());
    if (loop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        return queueSubmission(loop, threadPool, task);
    }
    else
    {
        return queueSubmission(loop);
    }
}

SC::Result SC::AsyncFileWrite::start(AsyncEventLoop& loop)
{
    SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncFileWrite::start - Zero sized write buffer");
    SC_TRY_MSG(fileDescriptor != FileDescriptor::Invalid, "AsyncFileWrite::start - Invalid file descriptor");
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncFileWrite::start(AsyncEventLoop& loop, ThreadPool& threadPool, Task& task)
{
    SC_TRY_MSG(buffer.sizeInBytes() > 0, "AsyncFileWrite::start - Zero sized write buffer");
    SC_TRY_MSG(fileDescriptor != FileDescriptor::Invalid, "AsyncFileWrite::start - Invalid file descriptor");
    SC_TRY(validateAsync());
    if (loop.internal.kernelQueue.get().makesSenseToRunInThreadPool(*this))
    {
        return queueSubmission(loop, threadPool, task);
    }
    else
    {
        return queueSubmission(loop);
    }
}

SC::Result SC::AsyncFileClose::start(AsyncEventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    fileDescriptor = fd;
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncFilePoll::start(AsyncEventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    fileDescriptor = fd;
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

//-------------------------------------------------------------------------------------------------------
// AsyncEventLoop
//-------------------------------------------------------------------------------------------------------

SC::AsyncEventLoop::AsyncEventLoop() : internal(internalOpaque.get()) { internal.loop = this; }

SC::Result SC::AsyncEventLoop::create(Options options)
{
    SC_TRY(internal.kernelQueue.get().createEventLoop(options));
    SC_TRY(internal.kernelQueue.get().createSharedWatchers(*this));
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::close() { return internal.close(); }

SC::Result SC::AsyncEventLoop::runOnce() { return internal.runStep(Internal::SyncMode::ForcedForwardProgress); }

SC::Result SC::AsyncEventLoop::runNoWait() { return internal.runStep(Internal::SyncMode::NoWait); }

SC::Result SC::AsyncEventLoop::run()
{
    // It may happen that getTotalNumberOfActiveHandle() < 0 when re-activating an async that has been calling
    // decreaseActiveCount() during initial setup. Now that async would be in the submissions.
    // One example that matches this case is re-activation of the FilePoll used for shared wakeups.
    while (internal.getTotalNumberOfActiveHandle() != 0 or not internal.submissions.isEmpty())
    {
        SC_TRY(runOnce());
    };
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::submitRequests(AsyncKernelEvents& kernelEvents)
{
    return internal.submitRequests(kernelEvents);
}

SC::Result SC::AsyncEventLoop::blockingPoll(AsyncKernelEvents& kernelEvents)
{
    return internal.blockingPoll(Internal::SyncMode::ForcedForwardProgress, kernelEvents);
}

SC::Result SC::AsyncEventLoop::dispatchCompletions(AsyncKernelEvents& kernelEvents)
{
    return internal.dispatchCompletions(Internal::SyncMode::ForcedForwardProgress, kernelEvents);
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
/// Get Loop time
SC::Time::HighResolutionCounter SC::AsyncEventLoop::getLoopTime() const { return internal.loopTime; }

#if SC_PLATFORM_LINUX
#else
bool SC::AsyncEventLoop::tryLoadingLiburing() { return false; }
#endif

//-------------------------------------------------------------------------------------------------------
// AsyncEventLoop::Internal
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncEventLoop::Internal::queueSubmission(AsyncRequest& async, AsyncTask* task)
{
    if (task)
    {
        if (task->async != nullptr)
        {
            return Result::Error("AsyncTask is bound to a different async being started");
        }
    }
    async.eventLoop = loop;
    async.state     = AsyncRequest::State::Setup;

    // Only set the async tasks for operations and backends that are not io_uring
    if (task)
    {
        async.asyncTask = task;
        task->async     = &async;
    }
    else
    {
        async.asyncTask = nullptr;
    }

    submissions.queueBack(async);
    return Result(true);
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
    AsyncLoopTimeout* async;
    for (async = activeLoopTimeouts.front; //
         async != nullptr;                 //
         async = static_cast<AsyncLoopTimeout*>(async->next))
    {
        if (currentTime.isLaterThanOrEqualTo(async->expirationTime))
        {
            removeActiveHandle(*async);
            AsyncLoopTimeout::Result result(*async, Result(true));
            async->callback(result);

            if (result.shouldBeReactivated)
            {
                async->state = AsyncRequest::State::Submitting;
                submissions.queueBack(*async);
            }
        }
    }
}

template <typename T>
void SC::AsyncEventLoop::Internal::freeAsyncRequests(IntrusiveDoubleLinkedList<T>& linkedList)
{
    for (auto async = linkedList.front; async != nullptr; async = static_cast<T*>(async->next))
    {
        async->markAsFree();
    }
    linkedList.clear();
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

SC::Result SC::AsyncEventLoop::Internal::close()
{
    Result res = Result(true);

    // Wait for all thread pool tasks
    Result threadPoolRes1 = waitForThreadPoolTasks(activeFileReads);
    Result threadPoolRes2 = waitForThreadPoolTasks(activeFileWrites);

    if (not threadPoolRes1)
        res = threadPoolRes1;

    if (not threadPoolRes2)
        res = threadPoolRes2;

    while (AsyncRequest* async = manualThreadPoolCompletions.pop())
    {
        async->state     = AsyncRequest::State::Free;
        async->eventLoop = nullptr;
    }

    freeAsyncRequests(submissions);

    freeAsyncRequests(activeLoopTimeouts);
    freeAsyncRequests(activeLoopWakeUps);
    freeAsyncRequests(activeProcessExits);
    freeAsyncRequests(activeSocketAccepts);
    freeAsyncRequests(activeSocketConnects);
    freeAsyncRequests(activeSocketSends);
    freeAsyncRequests(activeSocketReceives);
    freeAsyncRequests(activeSocketCloses);
    freeAsyncRequests(activeFileReads);
    freeAsyncRequests(activeFileWrites);
    freeAsyncRequests(activeFileCloses);
    freeAsyncRequests(activeFilePolls);

    freeAsyncRequests(manualCompletions);
    numberOfActiveHandles = 0;
    numberOfExternals     = 0;
    SC_TRY(loop->internal.kernelQueue.get().close());
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
    case AsyncRequest::State::Free: {
        // TODO: Stop the completion, it has been cancelled before being submitted
        SC_ASSERT_RELEASE(false);
    }
    break;
    case AsyncRequest::State::Cancelling: {
        SC_TRY(cancelAsync(kernelEvents, async));
        SC_TRY(teardownAsync(kernelEvents, async));
    }
    break;
    case AsyncRequest::State::Teardown: {
        SC_TRY(teardownAsync(kernelEvents, async));
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

void SC::AsyncEventLoop::Internal::increaseActiveCount() { numberOfExternals += 1; }

void SC::AsyncEventLoop::Internal::decreaseActiveCount() { numberOfExternals -= 1; }

int SC::AsyncEventLoop::Internal::getTotalNumberOfActiveHandle() const
{
    return numberOfActiveHandles + numberOfExternals;
}

SC::Result SC::AsyncEventLoop::Internal::completeAndEventuallyReactivate(KernelEvents& kernelEvents,
                                                                         AsyncRequest& async, Result&& returnCode)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Active);
    bool reactivate = false;
    removeActiveHandle(async);
    SC_TRY(completeAsync(kernelEvents, async, move(returnCode), reactivate));
    if (reactivate)
    {
        async.state = AsyncRequest::State::Submitting;
        submissions.queueBack(async);
    }
    else
    {
        SC_TRY(teardownAsync(kernelEvents, async));
    }
    if (not returnCode)
    {
        reportError(kernelEvents, async, move(returnCode));
    }
    return Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::runStep(SyncMode syncMode)
{
    alignas(uint64_t) uint8_t buffer[8 * 1024]; // 8 Kb of kernel events
    AsyncKernelEvents         kernelEvents;
    kernelEvents.eventsMemory = buffer;
    SC_TRY(submitRequests(kernelEvents));
    SC_TRY(blockingPoll(syncMode, kernelEvents));
    return dispatchCompletions(syncMode, kernelEvents);
}

SC::Result SC::AsyncEventLoop::Internal::submitRequests(AsyncKernelEvents& asyncKernelEvents)
{
    KernelEvents kernelEvents(loop->internal.kernelQueue.get(), asyncKernelEvents);
    asyncKernelEvents.numberOfEvents = 0;
    // TODO: Check if it's possible to avoid zeroing kernel events memory
    memset(asyncKernelEvents.eventsMemory.data(), 0, asyncKernelEvents.eventsMemory.sizeInBytes());
    SC_LOG_MESSAGE("---------------\n");

    while (AsyncRequest* async = submissions.dequeueFront())
    {
        auto res = stageSubmission(kernelEvents, *async);
        if (not res)
        {
            reportError(kernelEvents, *async, move(res));
        }
    }

    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::blockingPoll(SyncMode syncMode, AsyncKernelEvents& asyncKernelEvents)
{
    KernelEvents kernelEvents(loop->internal.kernelQueue.get(), asyncKernelEvents);
    if (getTotalNumberOfActiveHandle() <= 0 and numberOfManualCompletions == 0)
    {
        // happens when we do cancelAsync on the last active async for example
        return SC::Result(true);
    }

    if (getTotalNumberOfActiveHandle() != 0)
    {
        // We may have some manualCompletions queued (for SocketClose for example) but no active handles
        SC_LOG_MESSAGE("Active Requests Before Poll = {}\n", getTotalNumberOfActiveHandle());
        SC_TRY(kernelEvents.syncWithKernel(*loop, syncMode));
        SC_LOG_MESSAGE("Active Requests After Poll = {}\n", getTotalNumberOfActiveHandle());
    }
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Internal::dispatchCompletions(SyncMode syncMode, AsyncKernelEvents& asyncKernelEvents)
{
    KernelEvents kernelEvents(loop->internal.kernelQueue.get(), asyncKernelEvents);
    switch (syncMode)
    {
    case SyncMode::NoWait: {
        updateTime();
        invokeExpiredTimers(loopTime);
    }
    break;
    case SyncMode::ForcedForwardProgress: {
        if (expiredTimer)
        {
            invokeExpiredTimers(expiredTimer->expirationTime);
            expiredTimer = nullptr;
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
            SC_ASSERT_RELEASE(async.state != AsyncRequest::State::Free);
            async.markAsFree();
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

struct SC::AsyncEventLoop::Internal::TeardownAsyncPhase
{
    KernelEvents& kernelEvents;
    TeardownAsyncPhase(KernelEvents& kernelEvents) : kernelEvents(kernelEvents) {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        return Result(kernelEvents.teardownAsync(async));
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
        if (result.getAsync().callback.isValid())
        {
            result.getAsync().callback(result);
        }
        reactivate = result.shouldBeReactivated;
        return SC::Result(true);
    }
};

SC::Result SC::AsyncEventLoop::Internal::setupAsync(KernelEvents& kernelEvents, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, AsyncRequest::TypeToString(async.type));
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

SC::Result SC::AsyncEventLoop::Internal::teardownAsync(KernelEvents& kernelEvents, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} TEARDOWN\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_TRY(Internal::applyOnAsync(async, TeardownAsyncPhase(kernelEvents)));
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
    async.state = AsyncRequest::State::Free;
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

SC::Result SC::AsyncEventLoop::Internal::cancelAsync(AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} STOP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    const bool asyncStateIsNotFree    = async.state != AsyncRequest::State::Free;
    const bool asyncIsOwnedByThisLoop = async.eventLoop == loop;
    SC_TRY_MSG(asyncStateIsNotFree, "Trying to stop AsyncRequest that is not active");
    SC_TRY_MSG(asyncIsOwnedByThisLoop, "Trying to add AsyncRequest belonging to another Loop");
    switch (async.state)
    {
    case AsyncRequest::State::Active: {
        removeActiveHandle(async);
        async.state = AsyncRequest::State::Cancelling;
        submissions.queueBack(async);
        break;
    }
    case AsyncRequest::State::Submitting: {
        async.state = AsyncRequest::State::Teardown;
        break;
    }
    case AsyncRequest::State::Setup: {
        submissions.remove(async);
        break;
    }
    case AsyncRequest::State::Teardown: //
        return SC::Result::Error("Trying to stop AsyncRequest that is already being cancelled (teardown)");
    case AsyncRequest::State::Free: //
        return SC::Result::Error("Trying to stop AsyncRequest that is not active");
    case AsyncRequest::State::Cancelling: //
        return SC::Result::Error("Trying to Stop AsyncRequest that is already being cancelled");
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

void SC::AsyncEventLoop::Internal::executeWakeUps(AsyncResult& result)
{
    AsyncLoopWakeUp* async;
    for (async = activeLoopWakeUps.front; //
         async != nullptr;                //
         async = static_cast<AsyncLoopWakeUp*>(async->next))
    {
        SC_ASSERT_DEBUG(async->type == AsyncRequest::Type::LoopWakeUp);
        AsyncLoopWakeUp* notifier = async;
        if (notifier->pending.load() == true)
        {
            AsyncLoopWakeUp::Result asyncResult(*notifier, Result(true));
            asyncResult.getAsync().callback(asyncResult);
            if (notifier->eventObject)
            {
                notifier->eventObject->signal();
            }
            result.reactivateRequest(asyncResult.shouldBeReactivated);
            notifier->pending.exchange(false); // allow executing the notification again
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
