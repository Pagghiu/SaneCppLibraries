// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Async.h"
#include "../Foundation/Result.h"
#include "../Threading/Threading.h" // EventObject

#if SC_PLATFORM_WINDOWS
#include "Internal/AsyncWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "Internal/AsyncEmscripten.inl"
#elif SC_PLATFORM_APPLE
#include "Internal/AsyncApple.inl"
#endif

#define SC_ASYNC_ENABLE_LOG 0
#if SC_ASYNC_ENABLE_LOG
#include "../System/Console.h"
#else
#if defined(SC_LOG_MESSAGE)
#undef SC_LOG_MESSAGE
#endif
#define SC_LOG_MESSAGE(a, ...)
#endif

template <>
void SC::EventLoop::InternalOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::EventLoop::InternalOpaque::destruct(Object& obj)
{
    obj.~Object();
}
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
#if SC_PLATFORM_WINDOWS
    case Type::WindowsPoll: return "WindowsPoll";
#endif
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

SC::Result SC::AsyncRequest::queueSubmission(EventLoop& loop) { return loop.queueSubmission(*this); }

void SC::AsyncRequest::updateTime() { eventLoop->updateTime(); }

SC::Result SC::AsyncRequest::stop()
{
    if (eventLoop)
        return eventLoop->stopAsync(*this);
    return SC::Result::Error("stop failed. eventLoop is nullptr");
}

SC::Result SC::AsyncLoopTimeout::start(EventLoop& loop, Time::Milliseconds expiration)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    updateTime();
    expirationTime = loop.getLoopTime().offsetBy(expiration);
    timeout        = expiration;
    return SC::Result(true);
}

SC::Result SC::AsyncLoopWakeUp::start(EventLoop& loop, EventObject* eo)
{
    SC_TRY(queueSubmission(loop));
    eventObject = eo;
    return SC::Result(true);
}

SC::Result SC::AsyncProcessExit::start(EventLoop& loop, ProcessDescriptor::Handle process)
{
    SC_TRY(queueSubmission(loop));
    handle = process;
    return SC::Result(true);
}

SC::Result SC::AsyncSocketAccept::start(EventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(socketDescriptor.getAddressFamily(addressFamily));
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncSocketConnect::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                         SocketIPAddress socketIpAddress)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    ipAddress = socketIpAddress;
    return SC::Result(true);
}

SC::Result SC::AsyncSocketSend::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                      Span<const char> dataToSend)
{

    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    data = dataToSend;
    return SC::Result(true);
}

SC::Result SC::AsyncSocketReceive::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                         Span<char> receiveData)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    data = receiveData;
    return SC::Result(true);
}

SC::Result SC::AsyncSocketClose::start(EventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::AsyncFileRead::start(EventLoop& loop, FileDescriptor::Handle fd, Span<char> rb)
{
    SC_TRY_MSG(rb.sizeInBytes() > 0, "EventLoop::startFileRead - Zero sized read buffer");
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    readBuffer     = rb;
    return SC::Result(true);
}

SC::Result SC::AsyncFileWrite::start(EventLoop& loop, FileDescriptor::Handle fd, Span<const char> wb)
{
    SC_TRY_MSG(wb.sizeInBytes() > 0, "EventLoop::startFileWrite - Zero sized write buffer");
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    writeBuffer    = wb;
    return SC::Result(true);
}

SC::Result SC::AsyncFileClose::start(EventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    return SC::Result(true);
}

#if SC_PLATFORM_WINDOWS
SC::Result SC::AsyncWindowsPoll::start(EventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    return SC::Result(true);
}
#endif

SC::Result SC::EventLoop::queueSubmission(AsyncRequest& async)
{
    async.state = AsyncRequest::State::Submitting;
    submissions.queueBack(async);
    async.eventLoop = this;
    return SC::Result(true);
}

SC::Result SC::EventLoop::run()
{
    while (getTotalNumberOfActiveHandle() > 0 or not submissions.isEmpty())
    {
        SC_TRY(runOnce());
    };
    return SC::Result(true);
}

const SC::Time::HighResolutionCounter* SC::EventLoop::findEarliestTimer() const
{
    const Time::HighResolutionCounter* earliestTime = nullptr;
    for (AsyncRequest* async = activeTimers.front; async != nullptr; async = async->next)
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

void SC::EventLoop::invokeExpiredTimers()
{
    for (AsyncRequest* async = activeTimers.front; async != nullptr;)
    {
        SC_ASSERT_DEBUG(async->type == AsyncRequest::Type::LoopTimeout);
        const auto& expirationTime = static_cast<AsyncLoopTimeout*>(async)->expirationTime;
        if (loopTime.isLaterThanOrEqualTo(expirationTime))
        {
            AsyncRequest* currentAsync = async;
            async                      = async->next;
            activeTimers.remove(*currentAsync);
            currentAsync->state          = AsyncRequest::State::Free;
            Result                   res = Result(true);
            AsyncLoopTimeout::Result result(*static_cast<AsyncLoopTimeout*>(currentAsync), move(res));
            result.async.eventLoop = nullptr; // Allow reusing it
            static_cast<AsyncLoopTimeout*>(currentAsync)->callback(result);
        }
        else
        {
            async = async->next;
        }
    }
}

SC::Result SC::EventLoop::create()
{
    Internal& self = internal.get();
    SC_TRY(self.createEventLoop());
    SC_TRY(self.createWakeup(*this));
    return SC::Result(true);
}

SC::Result SC::EventLoop::close()
{
    Internal& self = internal.get();
    return self.close();
}

SC::Result SC::EventLoop::stageSubmission(KernelQueue& queue, AsyncRequest& async)
{
    switch (async.state)
    {
    case AsyncRequest::State::Submitting: {
        SC_TRY(setupAsync(queue, async));
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
    }
    break;
    case AsyncRequest::State::Active: {
        SC_ASSERT_DEBUG(false);
        return SC::Result::Error("EventLoop::processSubmissions() got Active handle");
    }
    break;
    }
    return SC::Result(true);
}

void SC::EventLoop::increaseActiveCount() { numberOfExternals += 1; }

void SC::EventLoop::decreaseActiveCount() { numberOfExternals -= 1; }

int SC::EventLoop::getTotalNumberOfActiveHandle() const
{
    return numberOfActiveHandles + numberOfTimers + numberOfWakeups + numberOfExternals;
}

SC::Result SC::EventLoop::runOnce() { return runStep(PollMode::ForcedForwardProgress); }

SC::Result SC::EventLoop::runNoWait() { return runStep(PollMode::NoWait); }

void SC::EventLoop::completeAndEventuallyReactivate(KernelQueue& queue, AsyncRequest& async, Result&& returnCode)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Active);
    bool reactivate = false;
    completeAsync(queue, async, move(returnCode), reactivate);
    if (reactivate)
    {
        returnCode = activateAsync(queue, async);
    }
    else
    {
        returnCode = cancelAsync(queue, async);
    }
    if (not returnCode)
    {
        reportError(queue, async, move(returnCode));
    }
}

SC::Result SC::EventLoop::runStep(PollMode pollMode)
{
    KernelQueue queue;
    SC_LOG_MESSAGE("---------------\n");

    while (AsyncRequest* async = submissions.dequeueFront())
    {
        auto res = stageSubmission(queue, *async);
        if (not res)
        {
            reportError(queue, *async, move(res));
        }
    }

    if (getTotalNumberOfActiveHandle() <= 0 and manualCompletions.isEmpty())
    {
        // happens when we do cancelAsync on the last active async for example
        return SC::Result(true);
    }

    if (getTotalNumberOfActiveHandle() > 0)
    {
        // We may have some manualCompletions queued (for SocketClose for example) but no active handles
        SC_LOG_MESSAGE("Active Requests Before Poll = {}\n", getTotalNumberOfActiveHandle());
        SC_TRY(queue.pollAsync(*this, pollMode));
        SC_LOG_MESSAGE("Active Requests After Poll = {}\n", getTotalNumberOfActiveHandle());
    }

    Internal& self = internal.get();
    for (decltype(KernelQueue::newEvents) idx = 0; idx < queue.newEvents; ++idx)
    {
        SC_LOG_MESSAGE(" Iteration = {}\n", (int)idx);
        SC_LOG_MESSAGE(" Active Requests = {}\n", getTotalNumberOfActiveHandle());
        bool continueProcessing = true;

        Result        result = Result(queue.validateEvent(queue.events[idx], continueProcessing));
        AsyncRequest& async  = *self.getAsyncRequest(queue.events[idx]);
        if (not result)
        {
            reportError(queue, async, move(result));
            continue;
        }

        if (not continueProcessing)
        {
            continue;
        }
        async.eventIndex = idx;
        if (async.state == AsyncRequest::State::Cancelling)
        {
            async.state     = AsyncRequest::State::Free;
            async.eventLoop = nullptr;
        }
        else
        {
            completeAndEventuallyReactivate(queue, async, move(result));
        }
    }

    while (AsyncRequest* async = manualCompletions.dequeueFront())
    {
        completeAndEventuallyReactivate(queue, *async, Result(true));
    }
    SC_LOG_MESSAGE("Active Requests After Completion = {}\n", getTotalNumberOfActiveHandle());
    return SC::Result(true);
}

template <typename Lambda>
SC::Result SC::AsyncRequest::applyOnAsync(AsyncRequest& async, Lambda&& lambda)
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
#if SC_PLATFORM_WINDOWS
    case AsyncRequest::Type::WindowsPoll: SC_TRY(lambda(*static_cast<AsyncWindowsPoll*>(&async))); break;
#endif
    }
    return SC::Result(true);
}

SC::Result SC::EventLoop::setupAsync(KernelQueue& queue, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    return AsyncRequest::applyOnAsync(async, [&](auto& p) { return queue.setupAsync(p); });
}

SC::Result SC::EventLoop::activateAsync(KernelQueue& queue, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} ACTIVATE\n", async.debugName, AsyncRequest::TypeToString(async.type));
    // Submitting (first time) or Active (for reactivations)
    SC_ASSERT_DEBUG(async.state == AsyncRequest::State::Active or async.state == AsyncRequest::State::Submitting);
    SC_TRY(AsyncRequest::applyOnAsync(async, [&queue](auto& p) { return queue.activateAsync(p); }));
    if (async.state == AsyncRequest::State::Submitting)
    {
        return queue.pushNewSubmission(async);
    }
    return SC::Result(true);
}

void SC::EventLoop::reportError(KernelQueue& queue, AsyncRequest& async, Result&& returnCode)
{
    SC_LOG_MESSAGE("{} ERROR {}\n", async.debugName, AsyncRequest::TypeToString(async.type));
    bool reactivate = false;
    if (async.state == AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    completeAsync(queue, async, forward<Result>(returnCode), reactivate);
    async.state = AsyncRequest::State::Free;
}

void SC::EventLoop::completeAsync(KernelQueue& queue, AsyncRequest& async, Result&& returnCode, bool& reactivate)
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
    bool res = AsyncRequest::applyOnAsync(async,
                                          [&](auto& p)
                                          {
                                              using Type = typename TypeTraits::RemoveReference<decltype(p)>::type;
                                              typename Type::Result result(p, forward<Result>(returnCode));
                                              if (result.returnCode)
                                                  result.returnCode = Result(queue.completeAsync(result));
                                              if (result.async.callback.isValid())
                                                  result.async.callback(result);
                                              reactivate = result.shouldBeReactivated;
                                              return SC::Result(true);
                                          });
    SC_TRUST_RESULT(res);
}

SC::Result SC::EventLoop::cancelAsync(KernelQueue& queue, AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} CANCEL\n", async.debugName, AsyncRequest::TypeToString(async.type));
    SC_TRY(AsyncRequest::applyOnAsync(async, [&](auto& p) { return queue.stopAsync(p); }))
    if (async.state == AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    return SC::Result(true);
}

SC::Result SC::EventLoop::stopAsync(AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} STOP\n", async.debugName, AsyncRequest::TypeToString(async.type));
    const bool asyncStateIsNotFree    = async.state != AsyncRequest::State::Free;
    const bool asyncIsOwnedByThisLoop = async.eventLoop == this;
    SC_TRY_MSG(asyncStateIsNotFree, "Trying to stop AsyncRequest that is not active");
    SC_TRY_MSG(asyncIsOwnedByThisLoop, "Trying to add AsyncRequest belonging to another Loop");
    switch (async.state)
    {
    case AsyncRequest::State::Active: {
        if (async.type == AsyncRequest::Type::LoopTimeout)
        {
            activeTimers.remove(async);
        }
        else if (async.type == AsyncRequest::Type::LoopWakeUp)
        {
            activeWakeUps.remove(async);
        }
        else
        {
            removeActiveHandle(async);
        }
        async.state = AsyncRequest::State::Cancelling;
        submissions.queueBack(async);
        break;
    }
    case AsyncRequest::State::Submitting: {
        submissions.remove(async);
        break;
    }
    case AsyncRequest::State::Free: return SC::Result::Error("Trying to stop AsyncRequest that is not active");
    case AsyncRequest::State::Cancelling:
        return SC::Result::Error("Trying to Stop AsyncRequest that is already being cancelled");
    }
    return Result(true);
}

void SC::EventLoop::updateTime() { loopTime.snap(); }

void SC::EventLoop::executeTimers(KernelQueue& queue, const Time::HighResolutionCounter& nextTimer)
{
    const bool timeoutOccurredWithoutIO = queue.newEvents == 0;
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

SC::Result SC::EventLoop::wakeUpFromExternalThread(AsyncLoopWakeUp& async)
{
    SC_TRY_MSG(async.eventLoop == this,
               "EventLoop::wakeUpFromExternalThread - Wakeup belonging to different EventLoop");
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

void SC::EventLoop::executeWakeUps(AsyncResult& result)
{
    for (AsyncRequest* async = activeWakeUps.front; async != nullptr; async = async->next)
    {
        SC_ASSERT_DEBUG(async->type == AsyncRequest::Type::LoopWakeUp);
        AsyncLoopWakeUp* notifier = static_cast<AsyncLoopWakeUp*>(async);
        if (notifier->pending.load() == true)
        {
            Result                  res = Result(true);
            AsyncLoopWakeUp::Result asyncResult(*notifier, move(res));
            asyncResult.async.callback(asyncResult);
            if (notifier->eventObject)
            {
                notifier->eventObject->signal();
            }
            result.reactivateRequest(asyncResult.shouldBeReactivated);
            notifier->pending.exchange(false); // allow executing the notification again
        }
    }
}

void SC::EventLoop::removeActiveHandle(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Active);
    async.state = AsyncRequest::State::Free;
    async.eventLoop->numberOfActiveHandles -= 1;
}

void SC::EventLoop::addActiveHandle(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Submitting);
    async.state = AsyncRequest::State::Active;
    async.eventLoop->numberOfActiveHandles += 1;
}

void SC::EventLoop::scheduleManualCompletion(AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == AsyncRequest::State::Submitting);
    async.eventLoop->manualCompletions.queueBack(async);
    async.state = AsyncRequest::State::Active;
}

SC::Result SC::EventLoop::getLoopFileDescriptor(SC::FileDescriptor::Handle& fileDescriptor) const
{
    return internal.get().loopFd.get(fileDescriptor,
                                     SC::Result::Error("EventLoop::getLoopFileDescriptor invalid handle"));
}

SC::Result SC::EventLoop::createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor)
{
    auto res = outDescriptor.create(family, SocketFlags::SocketStream, SocketFlags::ProtocolTcp,
                                    SocketFlags::NonBlocking, SocketFlags::NonInheritable);
    SC_TRY(res);
    return associateExternallyCreatedTCPSocket(outDescriptor);
}

SC::Result SC::AsyncLoopWakeUp::wakeUp() { return getEventLoop()->wakeUpFromExternalThread(*this); }
