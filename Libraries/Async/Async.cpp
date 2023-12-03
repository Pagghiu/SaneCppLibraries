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
void SC::Async::EventLoop::InternalOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}
template <>
void SC::Async::EventLoop::InternalOpaque::destruct(Object& obj)
{
    obj.~Object();
}
#if SC_ASYNC_ENABLE_LOG
const char* SC::Async::AsyncRequest::TypeToString(Type type)
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

SC::Result SC::Async::AsyncRequest::validateAsync()
{
    const bool asyncStateIsFree      = state == Async::AsyncRequest::State::Free;
    const bool asyncIsNotOwnedByLoop = eventLoop == nullptr;
    SC_LOG_MESSAGE("{} {} QUEUE\n", debugName, Async::AsyncRequest::TypeToString(type));
    SC_TRY_MSG(asyncStateIsFree, "Trying to stage Async::AsyncRequest that is in use");
    SC_TRY_MSG(asyncIsNotOwnedByLoop, "Trying to add Async::AsyncRequest belonging to another Loop");
    return Result(true);
}

SC::Result SC::Async::AsyncRequest::queueSubmission(EventLoop& loop) { return loop.queueSubmission(*this); }

void SC::Async::AsyncRequest::updateTime() { eventLoop->updateTime(); }

SC::Result SC::Async::AsyncRequest::stop()
{
    if (eventLoop)
        return eventLoop->stopAsync(*this);
    return SC::Result::Error("stop failed. eventLoop is nullptr");
}

SC::Result SC::Async::LoopTimeout::start(EventLoop& loop, Time::Milliseconds expiration)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    updateTime();
    expirationTime = loop.getLoopTime().offsetBy(expiration);
    timeout        = expiration;
    return SC::Result(true);
}

SC::Result SC::Async::LoopWakeUp::start(EventLoop& loop, EventObject* eo)
{
    SC_TRY(queueSubmission(loop));
    eventObject = eo;
    return SC::Result(true);
}

SC::Result SC::Async::ProcessExit::start(EventLoop& loop, ProcessDescriptor::Handle process)
{
    SC_TRY(queueSubmission(loop));
    handle = process;
    return SC::Result(true);
}

SC::Result SC::Async::SocketAccept::start(EventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(socketDescriptor.getAddressFamily(addressFamily));
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::Async::SocketConnect::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                           SocketIPAddress socketIpAddress)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    ipAddress = socketIpAddress;
    return SC::Result(true);
}

SC::Result SC::Async::SocketSend::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                        Span<const char> dataToSend)
{

    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    data = dataToSend;
    return SC::Result(true);
}

SC::Result SC::Async::SocketReceive::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                           Span<char> receiveData)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    data = receiveData;
    return SC::Result(true);
}

SC::Result SC::Async::SocketClose::start(EventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, SC::Result::Error("Invalid handle")));
    SC_TRY(queueSubmission(loop));
    return SC::Result(true);
}

SC::Result SC::Async::FileRead::start(EventLoop& loop, FileDescriptor::Handle fd, Span<char> rb)
{
    SC_TRY_MSG(rb.sizeInBytes() > 0, "EventLoop::startFileRead - Zero sized read buffer");
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    readBuffer     = rb;
    return SC::Result(true);
}

SC::Result SC::Async::FileWrite::start(EventLoop& loop, FileDescriptor::Handle fd, Span<const char> wb)
{
    SC_TRY_MSG(wb.sizeInBytes() > 0, "EventLoop::startFileWrite - Zero sized write buffer");
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    writeBuffer    = wb;
    return SC::Result(true);
}

SC::Result SC::Async::FileClose::start(EventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    return SC::Result(true);
}

#if SC_PLATFORM_WINDOWS
SC::Result SC::Async::WindowsPoll::start(EventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    return SC::Result(true);
}
#endif

SC::Result SC::Async::EventLoop::queueSubmission(Async::AsyncRequest& async)
{
    async.state = Async::AsyncRequest::State::Submitting;
    submissions.queueBack(async);
    async.eventLoop = this;
    return SC::Result(true);
}

SC::Result SC::Async::EventLoop::run()
{
    while (getTotalNumberOfActiveHandle() > 0 or not submissions.isEmpty())
    {
        SC_TRY(runOnce());
    };
    return SC::Result(true);
}

const SC::Time::HighResolutionCounter* SC::Async::EventLoop::findEarliestTimer() const
{
    const Time::HighResolutionCounter* earliestTime = nullptr;
    for (Async::AsyncRequest* async = activeTimers.front; async != nullptr; async = async->next)
    {
        SC_ASSERT_DEBUG(async->type == Async::AsyncRequest::Type::LoopTimeout);
        const auto& expirationTime = static_cast<Async::LoopTimeout*>(async)->expirationTime;
        if (earliestTime == nullptr or earliestTime->isLaterThanOrEqualTo(expirationTime))
        {
            earliestTime = &expirationTime;
        }
    };
    return earliestTime;
}

void SC::Async::EventLoop::invokeExpiredTimers()
{
    for (Async::AsyncRequest* async = activeTimers.front; async != nullptr;)
    {
        SC_ASSERT_DEBUG(async->type == Async::AsyncRequest::Type::LoopTimeout);
        const auto& expirationTime = static_cast<Async::LoopTimeout*>(async)->expirationTime;
        if (loopTime.isLaterThanOrEqualTo(expirationTime))
        {
            Async::AsyncRequest* currentAsync = async;
            async                             = async->next;
            activeTimers.remove(*currentAsync);
            currentAsync->state            = Async::AsyncRequest::State::Free;
            Result                     res = Result(true);
            Async::LoopTimeout::Result result(*static_cast<Async::LoopTimeout*>(currentAsync), move(res));
            result.async.eventLoop = nullptr; // Allow reusing it
            static_cast<Async::LoopTimeout*>(currentAsync)->callback(result);
        }
        else
        {
            async = async->next;
        }
    }
}

SC::Result SC::Async::EventLoop::create()
{
    Internal& self = internal.get();
    SC_TRY(self.createEventLoop());
    SC_TRY(self.createWakeup(*this));
    return SC::Result(true);
}

SC::Result SC::Async::EventLoop::close()
{
    Internal& self = internal.get();
    return self.close();
}

SC::Result SC::Async::EventLoop::stageSubmission(KernelQueue& queue, Async::AsyncRequest& async)
{
    switch (async.state)
    {
    case Async::AsyncRequest::State::Submitting: {
        SC_TRY(setupAsync(queue, async));
        SC_TRY(activateAsync(queue, async));
    }
    break;
    case Async::AsyncRequest::State::Free: {
        // TODO: Stop the completion, it has been cancelled before being submitted
        SC_ASSERT_RELEASE(false);
    }
    break;
    case Async::AsyncRequest::State::Cancelling: {
        SC_TRY(cancelAsync(queue, async));
    }
    break;
    case Async::AsyncRequest::State::Active: {
        SC_ASSERT_DEBUG(false);
        return SC::Result::Error("EventLoop::processSubmissions() got Active handle");
    }
    break;
    }
    return SC::Result(true);
}

void SC::Async::EventLoop::increaseActiveCount() { numberOfExternals += 1; }

void SC::Async::EventLoop::decreaseActiveCount() { numberOfExternals -= 1; }

int SC::Async::EventLoop::getTotalNumberOfActiveHandle() const
{
    return numberOfActiveHandles + numberOfTimers + numberOfWakeups + numberOfExternals;
}

SC::Result SC::Async::EventLoop::runOnce() { return runStep(PollMode::ForcedForwardProgress); }

SC::Result SC::Async::EventLoop::runNoWait() { return runStep(PollMode::NoWait); }

void SC::Async::EventLoop::completeAndEventuallyReactivate(KernelQueue& queue, Async::AsyncRequest& async,
                                                           Result&& returnCode)
{
    SC_ASSERT_RELEASE(async.state == Async::AsyncRequest::State::Active);
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

SC::Result SC::Async::EventLoop::runStep(PollMode pollMode)
{
    KernelQueue queue;
    SC_LOG_MESSAGE("---------------\n");

    while (Async::AsyncRequest* async = submissions.dequeueFront())
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

        Result               result = Result(queue.validateEvent(queue.events[idx], continueProcessing));
        Async::AsyncRequest& async  = *self.getAsyncRequest(queue.events[idx]);
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
        if (async.state == Async::AsyncRequest::State::Cancelling)
        {
            async.state     = Async::AsyncRequest::State::Free;
            async.eventLoop = nullptr;
        }
        else
        {
            completeAndEventuallyReactivate(queue, async, move(result));
        }
    }

    while (Async::AsyncRequest* async = manualCompletions.dequeueFront())
    {
        completeAndEventuallyReactivate(queue, *async, Result(true));
    }
    SC_LOG_MESSAGE("Active Requests After Completion = {}\n", getTotalNumberOfActiveHandle());
    return SC::Result(true);
}

template <typename Lambda>
SC::Result SC::Async::AsyncRequest::applyOnAsync(Async::AsyncRequest& async, Lambda&& lambda)
{
    switch (async.type)
    {
    case Async::AsyncRequest::Type::LoopTimeout: SC_TRY(lambda(*static_cast<Async::LoopTimeout*>(&async))); break;
    case Async::AsyncRequest::Type::LoopWakeUp: SC_TRY(lambda(*static_cast<Async::LoopWakeUp*>(&async))); break;
    case Async::AsyncRequest::Type::ProcessExit: SC_TRY(lambda(*static_cast<Async::ProcessExit*>(&async))); break;
    case Async::AsyncRequest::Type::SocketAccept: SC_TRY(lambda(*static_cast<Async::SocketAccept*>(&async))); break;
    case Async::AsyncRequest::Type::SocketConnect: SC_TRY(lambda(*static_cast<Async::SocketConnect*>(&async))); break;
    case Async::AsyncRequest::Type::SocketSend: SC_TRY(lambda(*static_cast<Async::SocketSend*>(&async))); break;
    case Async::AsyncRequest::Type::SocketReceive: SC_TRY(lambda(*static_cast<Async::SocketReceive*>(&async))); break;
    case Async::AsyncRequest::Type::SocketClose: SC_TRY(lambda(*static_cast<Async::SocketClose*>(&async))); break;
    case Async::AsyncRequest::Type::FileRead: SC_TRY(lambda(*static_cast<Async::FileRead*>(&async))); break;
    case Async::AsyncRequest::Type::FileWrite: SC_TRY(lambda(*static_cast<Async::FileWrite*>(&async))); break;
    case Async::AsyncRequest::Type::FileClose: SC_TRY(lambda(*static_cast<Async::FileClose*>(&async))); break;
#if SC_PLATFORM_WINDOWS
    case Async::AsyncRequest::Type::WindowsPoll: SC_TRY(lambda(*static_cast<Async::WindowsPoll*>(&async))); break;
#endif
    }
    return SC::Result(true);
}

SC::Result SC::Async::EventLoop::setupAsync(KernelQueue& queue, Async::AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, Async::AsyncRequest::TypeToString(async.type));
    return Async::AsyncRequest::applyOnAsync(async, [&](auto& p) { return queue.setupAsync(p); });
}

SC::Result SC::Async::EventLoop::activateAsync(KernelQueue& queue, Async::AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} ACTIVATE\n", async.debugName, Async::AsyncRequest::TypeToString(async.type));
    // Submitting (first time) or Active (for reactivations)
    SC_ASSERT_DEBUG(async.state == Async::AsyncRequest::State::Active or
                    async.state == Async::AsyncRequest::State::Submitting);
    SC_TRY(Async::AsyncRequest::applyOnAsync(async, [&queue](auto& p) { return queue.activateAsync(p); }));
    if (async.state == Async::AsyncRequest::State::Submitting)
    {
        return queue.pushNewSubmission(async);
    }
    return SC::Result(true);
}

void SC::Async::EventLoop::reportError(KernelQueue& queue, Async::AsyncRequest& async, Result&& returnCode)
{
    SC_LOG_MESSAGE("{} ERROR {}\n", async.debugName, Async::AsyncRequest::TypeToString(async.type));
    bool reactivate = false;
    if (async.state == Async::AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    completeAsync(queue, async, forward<Result>(returnCode), reactivate);
    async.state = Async::AsyncRequest::State::Free;
}

void SC::Async::EventLoop::completeAsync(KernelQueue& queue, Async::AsyncRequest& async, Result&& returnCode,
                                         bool& reactivate)
{
    if (returnCode)
    {
        SC_LOG_MESSAGE("{} {} COMPLETE\n", async.debugName, Async::AsyncRequest::TypeToString(async.type));
    }
    else
    {
        SC_LOG_MESSAGE("{} {} COMPLETE (Error = \"{}\")\n", async.debugName,
                       Async::AsyncRequest::TypeToString(async.type), returnCode.message);
    }
    bool res = Async::AsyncRequest::applyOnAsync(async,
                                                 [&](auto& p)
                                                 {
                                                     using Type =
                                                         typename TypeTraits::RemoveReference<decltype(p)>::type;
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

SC::Result SC::Async::EventLoop::cancelAsync(KernelQueue& queue, Async::AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} CANCEL\n", async.debugName, Async::AsyncRequest::TypeToString(async.type));
    SC_TRY(Async::AsyncRequest::applyOnAsync(async, [&](auto& p) { return queue.stopAsync(p); }))
    if (async.state == Async::AsyncRequest::State::Active)
    {
        removeActiveHandle(async);
    }
    return SC::Result(true);
}

SC::Result SC::Async::EventLoop::stopAsync(Async::AsyncRequest& async)
{
    SC_LOG_MESSAGE("{} {} STOP\n", async.debugName, Async::AsyncRequest::TypeToString(async.type));
    const bool asyncStateIsNotFree    = async.state != Async::AsyncRequest::State::Free;
    const bool asyncIsOwnedByThisLoop = async.eventLoop == this;
    SC_TRY_MSG(asyncStateIsNotFree, "Trying to stop Async::AsyncRequest that is not active");
    SC_TRY_MSG(asyncIsOwnedByThisLoop, "Trying to add Async::AsyncRequest belonging to another Loop");
    switch (async.state)
    {
    case Async::AsyncRequest::State::Active: {
        if (async.type == Async::AsyncRequest::Type::LoopTimeout)
        {
            activeTimers.remove(async);
        }
        else if (async.type == Async::AsyncRequest::Type::LoopWakeUp)
        {
            activeWakeUps.remove(async);
        }
        else
        {
            removeActiveHandle(async);
        }
        async.state = Async::AsyncRequest::State::Cancelling;
        submissions.queueBack(async);
        break;
    }
    case Async::AsyncRequest::State::Submitting: {
        submissions.remove(async);
        break;
    }
    case Async::AsyncRequest::State::Free:
        return SC::Result::Error("Trying to stop Async::AsyncRequest that is not active");
    case Async::AsyncRequest::State::Cancelling:
        return SC::Result::Error("Trying to Stop Async::AsyncRequest that is already being cancelled");
    }
    return Result(true);
}

void SC::Async::EventLoop::updateTime() { loopTime.snap(); }

void SC::Async::EventLoop::executeTimers(KernelQueue& queue, const Time::HighResolutionCounter& nextTimer)
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

SC::Result SC::Async::EventLoop::wakeUpFromExternalThread(Async::LoopWakeUp& async)
{
    SC_TRY_MSG(async.eventLoop == this,
               "EventLoop::wakeUpFromExternalThread - Wakeup belonging to different EventLoop");
    SC_ASSERT_DEBUG(async.type == Async::AsyncRequest::Type::LoopWakeUp);
    Async::LoopWakeUp& notifier = *static_cast<Async::LoopWakeUp*>(&async);
    if (not notifier.pending.exchange(true))
    {
        // This executes if current thread is lucky enough to atomically exchange pending from false to true.
        // This effectively allows coalescing calls from different threads into a single notification.
        SC_TRY(wakeUpFromExternalThread());
    }
    return Result(true);
}

void SC::Async::EventLoop::executeWakeUps(Async::AsyncResult& result)
{
    for (Async::AsyncRequest* async = activeWakeUps.front; async != nullptr; async = async->next)
    {
        SC_ASSERT_DEBUG(async->type == Async::AsyncRequest::Type::LoopWakeUp);
        Async::LoopWakeUp* notifier = static_cast<Async::LoopWakeUp*>(async);
        if (notifier->pending.load() == true)
        {
            Async::LoopWakeUp::Result asyncResult(*notifier, Result(true));
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

void SC::Async::EventLoop::removeActiveHandle(Async::AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == Async::AsyncRequest::State::Active);
    async.state = Async::AsyncRequest::State::Free;
    async.eventLoop->numberOfActiveHandles -= 1;
}

void SC::Async::EventLoop::addActiveHandle(Async::AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == Async::AsyncRequest::State::Submitting);
    async.state = Async::AsyncRequest::State::Active;
    async.eventLoop->numberOfActiveHandles += 1;
}

void SC::Async::EventLoop::scheduleManualCompletion(Async::AsyncRequest& async)
{
    SC_ASSERT_RELEASE(async.state == Async::AsyncRequest::State::Submitting);
    async.eventLoop->manualCompletions.queueBack(async);
    async.state = Async::AsyncRequest::State::Active;
}

SC::Result SC::Async::EventLoop::getLoopFileDescriptor(SC::FileDescriptor::Handle& fileDescriptor) const
{
    return internal.get().loopFd.get(fileDescriptor,
                                     SC::Result::Error("EventLoop::getLoopFileDescriptor invalid handle"));
}

SC::Result SC::Async::EventLoop::createAsyncTCPSocket(SocketFlags::AddressFamily family,
                                                      SocketDescriptor&          outDescriptor)
{
    auto res = outDescriptor.create(family, SocketFlags::SocketStream, SocketFlags::ProtocolTcp,
                                    SocketFlags::NonBlocking, SocketFlags::NonInheritable);
    SC_TRY(res);
    return associateExternallyCreatedTCPSocket(outDescriptor);
}

SC::Result SC::Async::LoopWakeUp::wakeUp() { return getEventLoop()->wakeUpFromExternalThread(*this); }
