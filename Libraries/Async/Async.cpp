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

SC::Result SC::AsyncRequest::queueSubmission(AsyncEventLoop& loop, AsyncTask* task)
{
    return loop.privateSelf.queueSubmission(*this, task);
}

void SC::AsyncRequest::updateTime(AsyncEventLoop& loop) { loop.privateSelf.updateTime(); }

SC::Result SC::AsyncRequest::stop()
{
    if (eventLoop)
        return eventLoop->privateSelf.cancelAsync(*this);
    return SC::Result::Error("stop failed. eventLoop is nullptr");
}

//-------------------------------------------------------------------------------------------------------
// Async***
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncLoopTimeout::start(AsyncEventLoop& loop, Time::Milliseconds expiration)
{
    SC_TRY(validateAsync());
    updateTime(loop);
    expirationTime = loop.getLoopTime().offsetBy(expiration);
    timeout        = expiration;
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWakeUp::start(AsyncEventLoop& loop, EventObject* eo)
{
    eventObject = eo;
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWakeUp::wakeUp() { return getEventLoop()->wakeUpFromExternalThread(*this); }

SC::Result SC::AsyncProcessExit::start(AsyncEventLoop& loop, ProcessDescriptor::Handle process)
{
    handle = process;
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketAccept::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(socketDescriptor.getAddressFamily(addressFamily));
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketConnect::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                         SocketIPAddress socketIpAddress)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    ipAddress = socketIpAddress;
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketSend::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                      Span<const char> dataToSend)
{

    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer = dataToSend;
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketReceive::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                         Span<char> receiveData)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    buffer = receiveData;
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketClose::start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncFileRead::start(AsyncEventLoop& loop, FileDescriptor::Handle fd, Span<char> rb, Task* task)
{
    SC_TRY_MSG(rb.sizeInBytes() > 0, "AsyncEventLoop::startFileRead - Zero sized read buffer");
    SC_TRY(validateAsync());
    fileDescriptor = fd;
    buffer         = rb;
    SC_TRY(queueSubmission(loop, task));
    return SC::Result(true);
}

SC::Result SC::AsyncFileWrite::start(AsyncEventLoop& loop, FileDescriptor::Handle fd, Span<const char> wb, Task* task)
{
    SC_TRY_MSG(wb.sizeInBytes() > 0, "AsyncEventLoop::startFileWrite - Zero sized write buffer");
    SC_TRY(validateAsync());
    fileDescriptor = fd;
    buffer         = wb;
    SC_TRY(queueSubmission(loop, task));
    return SC::Result(true);
}

SC::Result SC::AsyncFileClose::start(AsyncEventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    fileDescriptor = fd;
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

SC::Result SC::AsyncFilePoll::start(AsyncEventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    fileDescriptor = fd;
    SC_TRY(queueSubmission(loop, nullptr));
    return SC::Result(true);
}

//-------------------------------------------------------------------------------------------------------
// AsyncEventLoop
//-------------------------------------------------------------------------------------------------------

SC::AsyncEventLoop::AsyncEventLoop() : privateSelf(privateOpaque.get()), internalSelf(internal.get())
{
    privateSelf.eventLoop = this;
}

SC::Result SC::AsyncEventLoop::create(Options options)
{
    SC_TRY(internalSelf.createEventLoop(options));
    SC_TRY(internalSelf.createSharedWatchers(*this));
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::close() { return privateSelf.close(); }

SC::Result SC::AsyncEventLoop::runOnce() { return privateSelf.runStep(Private::SyncMode::ForcedForwardProgress); }

SC::Result SC::AsyncEventLoop::runNoWait() { return privateSelf.runStep(Private::SyncMode::NoWait); }

SC::Result SC::AsyncEventLoop::run()
{
    while (privateSelf.getTotalNumberOfActiveHandle() > 0 or not privateSelf.submissions.isEmpty())
    {
        SC_TRY(runOnce());
    };
    return SC::Result(true);
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

template <>
void SC::AsyncEventLoop::PrivateOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::AsyncEventLoop::PrivateOpaque::destruct(Object& obj)
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

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread() { return internalSelf.wakeUpFromExternalThread(); }

SC::Result SC::AsyncEventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor)
{
    return internalSelf.associateExternallyCreatedTCPSocket(outDescriptor);
}

SC::Result SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor)
{
    return internalSelf.associateExternallyCreatedFileDescriptor(outDescriptor);
}
/// Get Loop time
SC::Time::HighResolutionCounter SC::AsyncEventLoop::getLoopTime() const { return privateSelf.loopTime; }

#if SC_PLATFORM_LINUX
#else
bool SC::AsyncEventLoop::tryLoadingLiburing() { return false; }
#endif

//-------------------------------------------------------------------------------------------------------
// AsyncEventLoop::Private
//-------------------------------------------------------------------------------------------------------

SC::Result SC::AsyncEventLoop::Private::queueSubmission(AsyncRequest& async, AsyncTask* task)
{
    if (task)
    {
        if (&task->asyncResult.async != &async)
        {
            return Result::Error("AsyncTask is bound to a different async being started");
        }
    }
    async.eventLoop = eventLoop;
    async.state     = AsyncRequest::State::Setup;
    // Only set the async tasks for operations and backends where it makes sense to do so
    async.asyncTask = eventLoop->internalSelf.makesSenseToRunInThreadPool(async) ? task : nullptr;
    submissions.queueBack(async);
    return Result(true);
}

const SC::Time::HighResolutionCounter* SC::AsyncEventLoop::Private::findEarliestTimer() const
{
    const Time::HighResolutionCounter* earliestTime = nullptr;
    for (AsyncRequest* async = activeLoopTimeouts.front; async != nullptr; async = async->next)
    {
        SC_ASSERT_DEBUG(async->type == AsyncRequest::Type::LoopTimeout);
        const auto& expirationTime = static_cast<AsyncLoopTimeout*>(async)->expirationTime;
        if (earliestTime == nullptr or earliestTime->isLaterThanOrEqualTo(expirationTime))
        {
            earliestTime = &expirationTime;
        }
    };
    return earliestTime;
}

void SC::AsyncEventLoop::Private::invokeExpiredTimers()
{
    AsyncLoopTimeout* async;
    for (async = activeLoopTimeouts.front; //
         async != nullptr;                 //
         async = static_cast<AsyncLoopTimeout*>(async->next))
    {
        if (loopTime.isLaterThanOrEqualTo(async->expirationTime))
        {
            activeLoopTimeouts.remove(*async);
            async->markAsFree();
            AsyncLoopTimeout::Result result(*async, Result(true));
            async->callback(result);
        }
    }
}

template <typename T>
void SC::AsyncEventLoop::Private::freeAsyncRequests(IntrusiveDoubleLinkedList<T>& linkedList)
{
    for (auto async = linkedList.front; async != nullptr; async = static_cast<T*>(async->next))
    {
        async->markAsFree();
    }
    linkedList.clear();
}

template <typename T>
SC::Result SC::AsyncEventLoop::Private::waitForThreadPoolTasks(IntrusiveDoubleLinkedList<T>& linkedList)
{
    Result res = Result(true);
    // Wait for all thread pool tasks
    for (T* it = linkedList.front; it != nullptr; it = static_cast<T*>(it->next))
    {
        if (it->asyncTask != nullptr)
        {
            if (not it->asyncTask->threadPool.waitForTask(it->asyncTask->task))
            {
                res = Result::Error("Threadpool was already stopped");
            }
        }
    }
    return res;
}

SC::Result SC::AsyncEventLoop::Private::close()
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
    SC_TRY(eventLoop->internalSelf.close());
    return res;
}

SC::Result SC::AsyncEventLoop::Private::stageSubmission(KernelQueue& queue, AsyncRequest& async)
{
    switch (async.state)
    {
    case AsyncRequest::State::Setup: {
        SC_TRY(setupAsync(queue, async));
        async.state = AsyncRequest::State::Submitting;
        SC_TRY(activateAsync(queue, async));
    }
    break;
    case AsyncRequest::State::Submitting: {
        SC_TRY(activateAsync(queue, async));
    }
    break;
    case AsyncRequest::State::Free: {
        // TODO: Stop the completion, it has been cancelled before being submitted
        SC_ASSERT_RELEASE(false);
    }
    break;
    case AsyncRequest::State::Cancelling: {
        SC_TRY(cancelAsync(queue, async));
        SC_TRY(teardownAsync(queue, async));
    }
    break;
    case AsyncRequest::State::Teardown: {
        SC_TRY(teardownAsync(queue, async));
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

void SC::AsyncEventLoop::Private::increaseActiveCount() { numberOfExternals += 1; }

void SC::AsyncEventLoop::Private::decreaseActiveCount() { numberOfExternals -= 1; }

int SC::AsyncEventLoop::Private::getTotalNumberOfActiveHandle() const
{
    return numberOfActiveHandles + numberOfExternals;
}

SC::Result SC::AsyncEventLoop::Private::completeAndEventuallyReactivate(KernelQueue& queue, AsyncRequest& async,
                                                                        Result&& returnCode)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Active);
    bool reactivate = false;
    SC_TRY(completeAsync(queue, async, move(returnCode), reactivate));
    if (reactivate)
    {
        removeActiveHandle(async);
        async.state = AsyncRequest::State::Submitting;
        submissions.queueBack(async);
    }
    else
    {
        SC_TRY(teardownAsync(queue, async));
        removeActiveHandle(async);
    }
    if (not returnCode)
    {
        reportError(queue, async, move(returnCode));
    }
    return Result(true);
}

SC::Result SC::AsyncEventLoop::Private::runStep(SyncMode syncMode)
{
    KernelQueue queue(eventLoop->internalSelf);
    SC_LOG_MESSAGE("---------------\n");

    while (AsyncRequest* async = submissions.dequeueFront())
    {
        auto res = stageSubmission(queue, *async);
        if (not res)
        {
            reportError(queue, *async, move(res));
        }
    }

    if (getTotalNumberOfActiveHandle() <= 0 and numberOfManualCompletions == 0)
    {
        // happens when we do cancelAsync on the last active async for example
        return SC::Result(true);
    }

    if (getTotalNumberOfActiveHandle())
    {
        // We may have some manualCompletions queued (for SocketClose for example) but no active handles
        SC_LOG_MESSAGE("Active Requests Before Poll = {}\n", getTotalNumberOfActiveHandle());
        SC_TRY(queue.syncWithKernel(*eventLoop, syncMode));
        SC_LOG_MESSAGE("Active Requests After Poll = {}\n", getTotalNumberOfActiveHandle());
    }

    runStepExecuteCompletions(queue);
    runStepExecuteManualCompletions(queue);
    runStepExecuteManualThreadPoolCompletions(queue);
    SC_LOG_MESSAGE("Active Requests After Completion = {} ( + {} manual)\n", getTotalNumberOfActiveHandle(),
                   numberOfManualCompletions);
    return SC::Result(true);
}

void SC::AsyncEventLoop::Private::runStepExecuteManualCompletions(KernelQueue& queue)
{
    while (AsyncRequest* async = manualCompletions.dequeueFront())
    {
        if (not completeAndEventuallyReactivate(queue, *async, Result(true)))
        {
            SC_LOG_MESSAGE("Error completing {}", async->debugName);
        }
    }
}

void SC::AsyncEventLoop::Private::runStepExecuteManualThreadPoolCompletions(KernelQueue& queue)
{
    while (AsyncRequest* async = manualThreadPoolCompletions.pop())
    {
        if (not completeAndEventuallyReactivate(queue, *async, Result(true)))
        {
            SC_LOG_MESSAGE("Error completing {}", async->debugName);
        }
    }
}

void SC::AsyncEventLoop::Private::runStepExecuteCompletions(KernelQueue& queue)
{
    for (uint32_t idx = 0; idx < queue.getNumEvents(); ++idx)
    {
        SC_LOG_MESSAGE(" Iteration = {}\n", idx);
        SC_LOG_MESSAGE(" Active Requests = {}\n", getTotalNumberOfActiveHandle());
        bool continueProcessing = true;

        AsyncRequest* request = queue.getAsyncRequest(idx);
        if (request == nullptr)
        {
            continue;
        }

        AsyncRequest& async  = *request;
        Result        result = Result(queue.validateEvent(idx, continueProcessing));
        if (not result)
        {
            reportError(queue, async, move(result));
            continue;
        }

        if (not continueProcessing)
        {
            continue;
        }
        async.eventIndex = static_cast<int32_t>(idx);
        if (async.state == AsyncRequest::State::Active)
        {
            if (not completeAndEventuallyReactivate(queue, async, move(result)))
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

struct SC::AsyncEventLoop::Private::SetupAsyncPhase
{
    KernelQueue& queue;
    SetupAsyncPhase(KernelQueue& kernelQueue) : queue(kernelQueue) {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        if (async.asyncTask)
        {
            return Result(true);
        }
        else
        {
            return Result(queue.setupAsync(async));
        }
    }
};

struct SC::AsyncEventLoop::Private::ActivateAsyncPhase
{
    KernelQueue& queue;
    ActivateAsyncPhase(KernelQueue& kernelQueue) : queue(kernelQueue) {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        if (async.asyncTask)
        {
            AsyncTask* asyncTask     = async.asyncTask;
            asyncTask->task.function = [&async] { executeThreadPoolOperation(async); };
            return asyncTask->threadPool.queueTask(asyncTask->task);
        }

        if (async.flags & AsyncRequest::Flag_ManualCompletion)
        {
            async.eventLoop->privateSelf.scheduleManualCompletion(async);
        }
        return Result(queue.activateAsync(async));
    }

    template <typename T>
    static void executeThreadPoolOperation(T& async)
    {
        AsyncTask& task             = *async.asyncTask;
        auto&      completionData   = static_cast<typename T::Result&>(task.asyncResult).completionData;
        task.asyncResult.returnCode = KernelQueue::executeOperation(async, completionData);
        async.eventLoop->privateSelf.manualThreadPoolCompletions.push(async);
        SC_ASSERT_RELEASE(async.eventLoop->wakeUpFromExternalThread());
    }
};

struct SC::AsyncEventLoop::Private::CancelAsyncPhase
{
    KernelQueue& queue;
    CancelAsyncPhase(KernelQueue& kernelQueue) : queue(kernelQueue) {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        if (async.asyncTask)
        {
            async.eventLoop->privateSelf.manualThreadPoolCompletions.remove(async);
            return Result(true);
        }

        if (async.flags & AsyncRequest::Flag_ManualCompletion)
        {
            async.eventLoop->privateSelf.manualCompletions.remove(async);
            return Result(true);
        }
        else
        {
            return Result(queue.cancelAsync(async));
        }
    }
};

struct SC::AsyncEventLoop::Private::TeardownAsyncPhase
{
    KernelQueue& queue;
    TeardownAsyncPhase(KernelQueue& kernelQueue) : queue(kernelQueue) {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        return Result(queue.teardownAsync(async));
    }
};

struct SC::AsyncEventLoop::Private::CompleteAsyncPhase
{
    KernelQueue& queue;
    Result&&     returnCode;
    bool&        reactivate;

    CompleteAsyncPhase(KernelQueue& kernelQueue, Result&& result, bool& doReactivate)
        : queue(kernelQueue), returnCode(move(result)), reactivate(doReactivate)
    {}

    template <typename T>
    SC::Result operator()(T& async)
    {
        using AsyncType       = T;
        using AsyncResultType = typename AsyncType::Result;
        AsyncResultType result(async, forward<Result>(returnCode));
        if (result.returnCode)
        {
            if (result.getAsync().asyncTask)
            {
                AsyncTask* asyncTask  = result.getAsync().asyncTask;
                result.returnCode     = asyncTask->asyncResult.returnCode;
                result.completionData = move(static_cast<AsyncResultType&>(asyncTask->asyncResult).completionData);
                // The task is already finished, but we call waitForTask to make sure it's being marked as free
                // In other words this call here will not be waiting at all.
                SC_TRY(asyncTask->threadPool.waitForTask(asyncTask->task));
            }
            else
            {
                result.returnCode = Result(queue.completeAsync(result));
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

SC::Result SC::AsyncEventLoop::Private::setupAsync(KernelQueue& queue, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    return Private::applyOnAsync(async, SetupAsyncPhase(queue));
}

SC::Result SC::AsyncEventLoop::Private::activateAsync(KernelQueue& queue, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} ACTIVATE\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Submitting);
    SC_TRY(Private::applyOnAsync(async, ActivateAsyncPhase(queue)));
    async.eventLoop->privateSelf.addActiveHandle(async);
    return Result(true);
}

SC::Result SC::AsyncEventLoop::Private::teardownAsync(KernelQueue& queue, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} TEARDOWN\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_TRY(Private::applyOnAsync(async, TeardownAsyncPhase(queue)));
    return Result(true);
}

void SC::AsyncEventLoop::Private::reportError(KernelQueue& queue, AsyncRequest& async, Result&& returnCode)
{
    SC_LOG_MESSAGE("{} ERROR {}\n", async.debugName, AsyncRequest::TypeToString(async.type));
    bool reactivate = false;
    if (async.state == AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    (void)completeAsync(queue, async, forward<Result>(returnCode), reactivate);
    async.state = AsyncRequest::State::Free;
}

SC::Result SC::AsyncEventLoop::Private::completeAsync(KernelQueue& queue, AsyncRequest& async, Result&& returnCode,
                                                      bool& reactivate)
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
    return Private::applyOnAsync(async, CompleteAsyncPhase(queue, move(returnCode), reactivate));
}

SC::Result SC::AsyncEventLoop::Private::cancelAsync(KernelQueue& queue, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} CANCEL\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_TRY(Private::applyOnAsync(async, CancelAsyncPhase(queue)))
    if (async.state == AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    return SC::Result(true);
}

SC::Result SC::AsyncEventLoop::Private::cancelAsync(AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} STOP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    const bool asyncStateIsNotFree    = async.state != AsyncRequest::State::Free;
    const bool asyncIsOwnedByThisLoop = async.eventLoop == eventLoop;
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

void SC::AsyncEventLoop::Private::updateTime() { loopTime.snap(); }

void SC::AsyncEventLoop::Private::executeTimers(KernelQueue& queue, const Time::HighResolutionCounter& nextTimer)
{
    const bool timeoutOccurredWithoutIO = queue.getNumEvents() == 0;
    const bool timeoutWasAlreadyExpired = loopTime.isLaterThanOrEqualTo(nextTimer);
    if (timeoutOccurredWithoutIO or timeoutWasAlreadyExpired)
    {
        if (timeoutWasAlreadyExpired)
        {
            // Not sure if this is really possible.
            updateTime();
        }
        else
        {
            loopTime = nextTimer;
        }
        invokeExpiredTimers();
    }
}

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread(AsyncLoopWakeUp& async)
{
    SC_TRY_MSG(async.eventLoop == this,
               "AsyncEventLoop::wakeUpFromExternalThread - Wakeup belonging to different AsyncEventLoop");
    SC_ASSERT_DEBUG(async.type == AsyncRequest::Type::LoopWakeUp);
    AsyncLoopWakeUp& notifier = *static_cast<AsyncLoopWakeUp*>(&async);
    if (not notifier.pending.exchange(true))
    {
        // This executes if current thread is lucky enough to atomically exchange pending from false to true.
        // This effectively allows coalescing calls from different threads into a single notification.
        SC_TRY(wakeUpFromExternalThread());
    }
    return Result(true);
}

void SC::AsyncEventLoop::Private::executeWakeUps(AsyncResult& result)
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
}

void SC::AsyncEventLoop::Private::removeActiveHandle(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Active);
    async.state = AsyncRequest::State::Free;

    if ((async.flags & AsyncRequest::Flag_ManualCompletion) != 0)
    {
        numberOfManualCompletions -= 1;
        return; // Asyncs flagged to be manually completed, are not added to active handles and do not count as active
    }

    numberOfActiveHandles -= 1;

    if (async.asyncTask)
    {
        return; // Asyncs flagged to be manually completed for thread pool, are not added to active handles
    }
    // clang-format off
    switch (async.type)
    {
        case AsyncRequest::Type::LoopTimeout:   activeLoopTimeouts.remove(*static_cast<AsyncLoopTimeout*>(&async));     break;
        case AsyncRequest::Type::LoopWakeUp:    activeLoopWakeUps.remove(*static_cast<AsyncLoopWakeUp*>(&async));       break;
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

void SC::AsyncEventLoop::Private::addActiveHandle(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Submitting);
    async.state = AsyncRequest::State::Active;

    if ((async.flags & AsyncRequest::Flag_ManualCompletion) != 0)
    {
        numberOfManualCompletions += 1;
        return; // Asyncs flagged to be manually completed, are not added to active handles
    }

    numberOfActiveHandles += 1;

    if (async.asyncTask)
    {
        return; // Asyncs flagged to be manually completed for thread pool, are not added to active handles
    }
    // clang-format off
    switch (async.type)
    {
        case AsyncRequest::Type::LoopTimeout:   activeLoopTimeouts.queueBack(*static_cast<AsyncLoopTimeout*>(&async));      break;
        case AsyncRequest::Type::LoopWakeUp:    activeLoopWakeUps.queueBack(*static_cast<AsyncLoopWakeUp*>(&async));        break;
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

void SC::AsyncEventLoop::Private::scheduleManualCompletion(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Setup or async.state == AsyncRequest::State::Submitting);
    async.eventLoop->privateSelf.manualCompletions.queueBack(async);
}

template <typename Lambda>
SC::Result SC::AsyncEventLoop::Private::applyOnAsync(AsyncRequest& async, Lambda&& lambda)
{
    switch (async.type)
    {
    case AsyncRequest::Type::LoopTimeout: SC_TRY(lambda(*static_cast<AsyncLoopTimeout*>(&async))); break;
    case AsyncRequest::Type::LoopWakeUp: SC_TRY(lambda(*static_cast<AsyncLoopWakeUp*>(&async))); break;
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
