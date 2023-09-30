// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "EventLoop.h"
#include "../Foundation/Objects/Optional.h"
#include "../Foundation/Objects/Result.h"
#include "../Threading/Threading.h" // EventObject

#if SC_PLATFORM_WINDOWS
#include "EventLoopInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "EventLoopInternalEmscripten.inl"
#elif SC_PLATFORM_APPLE
#include "EventLoopInternalApple.inl"
#endif

#if 0
#include "../System/Console.h"
#else
#define SC_LOG_MESSAGE(a, ...)
#endif

template <>
void SC::OpaqueFuncs<SC::EventLoop::InternalTraits>::construct(Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}
template <>
void SC::OpaqueFuncs<SC::EventLoop::InternalTraits>::destruct(Object& obj)
{
    obj.~Object();
}

SC::StringView SC::Async::TypeToString(Type type)
{
    switch (type)
    {
    case Type::LoopTimeout: return "LoopTimeout"_a8;
    case Type::LoopWakeUp: return "LoopWakeUp"_a8;
    case Type::ProcessExit: return "ProcessExit"_a8;
    case Type::SocketAccept: return "SocketAccept"_a8;
    case Type::SocketConnect: return "SocketConnect"_a8;
    case Type::SocketSend: return "SocketSend"_a8;
    case Type::SocketReceive: return "SocketReceive"_a8;
    case Type::SocketClose: return "SocketClose"_a8;
    case Type::FileRead: return "FileRead"_a8;
    case Type::FileWrite: return "FileWrite"_a8;
    case Type::FileClose: return "FileClose"_a8;
#if SC_PLATFORM_WINDOWS
    case Type::WindowsPoll: return "WindowsPoll"_a8;
#endif
    }
    Assert::unreachable();
}

SC::ReturnCode SC::Async::validateAsync()
{
    const bool asyncStateIsFree      = state == Async::State::Free;
    const bool asyncIsNotOwnedByLoop = eventLoop == nullptr;
    SC_LOG_MESSAGE("{} {} QUEUE\n", debugName, Async::TypeToString(type));
    SC_TRY_MSG(asyncStateIsFree, "Trying to stage Async that is in use"_a8);
    SC_TRY_MSG(asyncIsNotOwnedByLoop, "Trying to add Async belonging to another Loop"_a8);
    return true;
}

SC::ReturnCode SC::Async::queueSubmission(EventLoop& loop) { return loop.queueSubmission(*this); }

void SC::Async::updateTime() { eventLoop->updateTime(); }

SC::ReturnCode SC::Async::stop()
{
    if (eventLoop)
        return eventLoop->stopAsync(*this);
    return "stop failed. eventLoop is nullptr"_a8;
}

SC::ReturnCode SC::AsyncLoopTimeout::start(EventLoop& loop, IntegerMilliseconds expiration)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    updateTime();
    expirationTime = loop.getLoopTime().offsetBy(expiration);
    timeout        = expiration;
    return true;
}

SC::ReturnCode SC::AsyncLoopWakeUp::start(EventLoop& loop, EventObject* eo)
{
    SC_TRY(queueSubmission(loop));
    eventObject = eo;
    return true;
}

SC::ReturnCode SC::AsyncProcessExit::start(EventLoop& loop, ProcessDescriptor::Handle process)
{
    SC_TRY(queueSubmission(loop));
    handle = process;
    return true;
}

SC::ReturnCode SC::AsyncSocketAccept::start(EventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, "Invalid handle"_a8));
    SC_TRY(socketDescriptor.getAddressFamily(addressFamily));
    SC_TRY(queueSubmission(loop));
    return true;
}

SC::ReturnCode SC::AsyncSocketConnect::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                             SocketIPAddress socketIpAddress)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, "Invalid handle"_a8));
    SC_TRY(queueSubmission(loop));
    ipAddress = socketIpAddress;
    return true;
}

SC::ReturnCode SC::AsyncSocketSend::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                          Span<const char> dataToSend)
{

    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, "Invalid handle"_a8));
    SC_TRY(queueSubmission(loop));
    data = dataToSend;
    return true;
}

SC::ReturnCode SC::AsyncSocketReceive::start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                             Span<char> receiveData)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, "Invalid handle"_a8));
    SC_TRY(queueSubmission(loop));
    data = receiveData;
    return true;
}

SC::ReturnCode SC::AsyncSocketClose::start(EventLoop& loop, const SocketDescriptor& socketDescriptor)
{
    SC_TRY(validateAsync());
    SC_TRY(socketDescriptor.get(handle, "Invalid handle"_a8));
    SC_TRY(queueSubmission(loop));
    return true;
}

SC::ReturnCode SC::AsyncFileRead::start(EventLoop& loop, FileDescriptor::Handle fd, Span<char> rb)
{
    SC_TRY_MSG(rb.sizeInBytes() > 0, "EventLoop::startFileRead - Zero sized read buffer"_a8);
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    readBuffer     = rb;
    return true;
}

SC::ReturnCode SC::AsyncFileWrite::start(EventLoop& loop, FileDescriptor::Handle fd, Span<const char> wb)
{
    SC_TRY_MSG(wb.sizeInBytes() > 0, "EventLoop::startFileWrite - Zero sized write buffer"_a8);
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    writeBuffer    = wb;
    return true;
}

SC::ReturnCode SC::AsyncFileClose::start(EventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    return true;
}

#if SC_PLATFORM_WINDOWS
SC::ReturnCode SC::AsyncWindowsPoll::start(EventLoop& loop, FileDescriptor::Handle fd)
{
    SC_TRY(validateAsync());
    SC_TRY(queueSubmission(loop));
    fileDescriptor = fd;
    return true;
}
#endif

SC::ReturnCode SC::EventLoop::queueSubmission(Async& async)
{
    async.state = Async::State::Submitting;
    submissions.queueBack(async);
    async.eventLoop = this;
    return true;
}

SC::ReturnCode SC::EventLoop::run()
{
    while (getTotalNumberOfActiveHandle() > 0 or not submissions.isEmpty())
    {
        SC_TRY(runOnce());
    };
    return true;
}

const SC::TimeCounter* SC::EventLoop::findEarliestTimer() const
{
    const TimeCounter* earliestTime = nullptr;
    for (Async* async = activeTimers.front; async != nullptr; async = async->next)
    {
        SC_ASSERT_DEBUG(async->type == Async::Type::LoopTimeout);
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
    for (Async* async = activeTimers.front; async != nullptr;)
    {
        SC_ASSERT_DEBUG(async->type == Async::Type::LoopTimeout);
        const auto& expirationTime = static_cast<AsyncLoopTimeout*>(async)->expirationTime;
        if (loopTime.isLaterThanOrEqualTo(expirationTime))
        {
            Async* currentAsync = async;
            async               = async->next;
            activeTimers.remove(*currentAsync);
            currentAsync->state          = Async::State::Free;
            ReturnCode               res = true;
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

SC::ReturnCode SC::EventLoop::create()
{
    Internal& self = internal.get();
    SC_TRY(self.createEventLoop());
    SC_TRY(self.createWakeup(*this));
    return true;
}

SC::ReturnCode SC::EventLoop::close()
{
    Internal& self = internal.get();
    return self.close();
}

SC::ReturnCode SC::EventLoop::stageSubmission(KernelQueue& queue, Async& async)
{
    switch (async.state)
    {
    case Async::State::Submitting: {
        SC_TRY(setupAsync(queue, async));
        SC_TRY(activateAsync(queue, async));
    }
    break;
    case Async::State::Free: {
        // TODO: Stop the completion, it has been cancelled before being submitted
        SC_ASSERT_RELEASE(false);
    }
    break;
    case Async::State::Cancelling: {
        SC_TRY(cancelAsync(queue, async));
    }
    break;
    case Async::State::Active: {
        SC_ASSERT_DEBUG(false);
        return "EventLoop::processSubmissions() got Active handle"_a8;
    }
    break;
    }
    return true;
}

void SC::EventLoop::increaseActiveCount() { numberOfExternals += 1; }

void SC::EventLoop::decreaseActiveCount() { numberOfExternals -= 1; }

int SC::EventLoop::getTotalNumberOfActiveHandle() const
{
    return numberOfActiveHandles + numberOfTimers + numberOfWakeups + numberOfExternals;
}

SC::ReturnCode SC::EventLoop::runOnce() { return runStep(PollMode::ForcedForwardProgress); }

SC::ReturnCode SC::EventLoop::runNoWait() { return runStep(PollMode::NoWait); }

void SC::EventLoop::completeAndEventuallyReactivate(KernelQueue& queue, Async& async, ReturnCode&& returnCode)
{
    SC_ASSERT_RELEASE(async.state == Async::State::Active);
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

SC::ReturnCode SC::EventLoop::runStep(PollMode pollMode)
{
    KernelQueue queue;
    SC_LOG_MESSAGE("---------------\n");

    while (Async* async = submissions.dequeueFront())
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
        return true;
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

        ReturnCode result = queue.validateEvent(queue.events[idx], continueProcessing);
        Async&     async  = *self.getAsync(queue.events[idx]);
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
        if (async.state == Async::State::Cancelling)
        {
            async.state     = Async::State::Free;
            async.eventLoop = nullptr;
        }
        else
        {
            completeAndEventuallyReactivate(queue, async, move(result));
        }
    }

    while (Async* async = manualCompletions.dequeueFront())
    {
        completeAndEventuallyReactivate(queue, *async, ReturnCode(true));
    }
    SC_LOG_MESSAGE("Active Requests After Completion = {}\n", getTotalNumberOfActiveHandle());
    return true;
}

template <typename Lambda>
SC::ReturnCode SC::Async::applyOnAsync(Async& async, Lambda&& lambda)
{
    switch (async.type)
    {
    case Async::Type::LoopTimeout: SC_TRY(lambda(*static_cast<AsyncLoopTimeout*>(&async))); break;
    case Async::Type::LoopWakeUp: SC_TRY(lambda(*static_cast<AsyncLoopWakeUp*>(&async))); break;
    case Async::Type::ProcessExit: SC_TRY(lambda(*static_cast<AsyncProcessExit*>(&async))); break;
    case Async::Type::SocketAccept: SC_TRY(lambda(*static_cast<AsyncSocketAccept*>(&async))); break;
    case Async::Type::SocketConnect: SC_TRY(lambda(*static_cast<AsyncSocketConnect*>(&async))); break;
    case Async::Type::SocketSend: SC_TRY(lambda(*static_cast<AsyncSocketSend*>(&async))); break;
    case Async::Type::SocketReceive: SC_TRY(lambda(*static_cast<AsyncSocketReceive*>(&async))); break;
    case Async::Type::SocketClose: SC_TRY(lambda(*static_cast<AsyncSocketClose*>(&async))); break;
    case Async::Type::FileRead: SC_TRY(lambda(*static_cast<AsyncFileRead*>(&async))); break;
    case Async::Type::FileWrite: SC_TRY(lambda(*static_cast<AsyncFileWrite*>(&async))); break;
    case Async::Type::FileClose: SC_TRY(lambda(*static_cast<AsyncFileClose*>(&async))); break;
#if SC_PLATFORM_WINDOWS
    case Async::Type::WindowsPoll: SC_TRY(lambda(*static_cast<AsyncWindowsPoll*>(&async))); break;
#endif
    }
    return true;
}

SC::ReturnCode SC::EventLoop::setupAsync(KernelQueue& queue, Async& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, Async::TypeToString(async.type));
    return Async::applyOnAsync(async, [&](auto& p) { return queue.setupAsync(p); });
}

SC::ReturnCode SC::EventLoop::activateAsync(KernelQueue& queue, Async& async)
{
    SC_LOG_MESSAGE("{} {} ACTIVATE\n", async.debugName, Async::TypeToString(async.type));
    // Submitting (first time) or Active (for reactivations)
    SC_ASSERT_DEBUG(async.state == Async::State::Active or async.state == Async::State::Submitting);
    SC_TRY(Async::applyOnAsync(async, [&](auto& p) { return queue.activateAsync(p); }));
    if (async.state == Async::State::Submitting)
    {
        return queue.pushNewSubmission(async);
    }
    return true;
}

void SC::EventLoop::reportError(KernelQueue& queue, Async& async, ReturnCode&& returnCode)
{
    SC_LOG_MESSAGE("{} ERROR {}\n", async.debugName, Async::TypeToString(async.type));
    bool reactivate = false;
    if (async.state == Async::State::Active)
    {
        removeActiveHandle(async);
    }
    completeAsync(queue, async, forward<ReturnCode>(returnCode), reactivate);
    async.state = Async::State::Free;
}

void SC::EventLoop::completeAsync(KernelQueue& queue, Async& async, ReturnCode&& returnCode, bool& reactivate)
{
    if (returnCode)
    {
        SC_LOG_MESSAGE("{} {} COMPLETE\n", async.debugName, Async::TypeToString(async.type));
    }
    else
    {
        SC_LOG_MESSAGE("{} {} COMPLETE (Error = \"{}\")\n", async.debugName, Async::TypeToString(async.type),
                       returnCode.message);
    }
    bool res = Async::applyOnAsync(async,
                                   [&](auto& p)
                                   {
                                       using Type = typename RemoveReference<decltype(p)>::type;
                                       typename Type::Result result(p, forward<ReturnCode>(returnCode));
                                       if (result.returnCode)
                                           result.returnCode = queue.completeAsync(result);
                                       if (result.async.callback.isValid())
                                           result.async.callback(result);
                                       reactivate = result.shouldBeReactivated;
                                       return true;
                                   });
    SC_TRUST_RESULT(res);
}

SC::ReturnCode SC::EventLoop::cancelAsync(KernelQueue& queue, Async& async)
{
    SC_LOG_MESSAGE("{} {} CANCEL\n", async.debugName, Async::TypeToString(async.type));
    SC_TRY(Async::applyOnAsync(async, [&](auto& p) { return queue.stopAsync(p); }))
    if (async.state == Async::State::Active)
    {
        removeActiveHandle(async);
    }
    return true;
}

SC::ReturnCode SC::EventLoop::stopAsync(Async& async)
{
    SC_LOG_MESSAGE("{} {} STOP\n", async.debugName, Async::TypeToString(async.type));
    const bool asyncStateIsNotFree    = async.state != Async::State::Free;
    const bool asyncIsOwnedByThisLoop = async.eventLoop == this;
    SC_TRY_MSG(asyncStateIsNotFree, "Trying to stop Async that is not active"_a8);
    SC_TRY_MSG(asyncIsOwnedByThisLoop, "Trying to add Async belonging to another Loop"_a8);
    switch (async.state)
    {
    case Async::State::Active: {
        if (async.type == Async::Type::LoopTimeout)
        {
            activeTimers.remove(async);
        }
        else if (async.type == Async::Type::LoopWakeUp)
        {
            activeWakeUps.remove(async);
        }
        else
        {
            removeActiveHandle(async);
        }
        async.state = Async::State::Cancelling;
        submissions.queueBack(async);
        break;
    }
    case Async::State::Submitting: {
        submissions.remove(async);
        break;
    }
    case Async::State::Free: return "Trying to stop Async that is not active"_a8;
    case Async::State::Cancelling: return "Trying to Stop Async that is already being cancelled"_a8;
    }
    return true;
}

void SC::EventLoop::updateTime() { loopTime.snap(); }

void SC::EventLoop::executeTimers(KernelQueue& queue, const TimeCounter& nextTimer)
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

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread(AsyncLoopWakeUp& async)
{
    SC_TRY_MSG(async.eventLoop == this,
               "EventLoop::wakeUpFromExternalThread - Wakeup belonging to different EventLoop"_a8);
    SC_ASSERT_DEBUG(async.type == Async::Type::LoopWakeUp);
    AsyncLoopWakeUp& notifier = *static_cast<AsyncLoopWakeUp*>(&async);
    if (not notifier.pending.exchange(true))
    {
        // This executes if current thread is lucky enough to atomically exchange pending from false to true.
        // This effectively allows coalescing calls from different threads into a single notification.
        SC_TRY(wakeUpFromExternalThread());
    }
    return true;
}

void SC::EventLoop::executeWakeUps(AsyncResultBase& result)
{
    for (Async* async = activeWakeUps.front; async != nullptr; async = async->next)
    {
        SC_ASSERT_DEBUG(async->type == Async::Type::LoopWakeUp);
        AsyncLoopWakeUp* notifier = static_cast<AsyncLoopWakeUp*>(async);
        if (notifier->pending.load() == true)
        {
            ReturnCode              res = true;
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

void SC::EventLoop::removeActiveHandle(Async& async)
{
    SC_ASSERT_RELEASE(async.state == Async::State::Active);
    async.state = Async::State::Free;
    async.eventLoop->numberOfActiveHandles -= 1;
}

void SC::EventLoop::addActiveHandle(Async& async)
{
    SC_ASSERT_RELEASE(async.state == Async::State::Submitting);
    async.state = Async::State::Active;
    async.eventLoop->numberOfActiveHandles += 1;
}

void SC::EventLoop::scheduleManualCompletion(Async& async)
{
    SC_ASSERT_RELEASE(async.state == Async::State::Submitting);
    async.eventLoop->manualCompletions.queueBack(async);
    async.state = Async::State::Active;
}

SC::ReturnCode SC::EventLoop::getLoopFileDescriptor(SC::FileDescriptor::Handle& fileDescriptor) const
{
    return internal.get().loopFd.get(fileDescriptor, "EventLoop::getLoopFileDescriptor invalid handle"_a8);
}

SC::ReturnCode SC::EventLoop::createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor)
{
    auto res = outDescriptor.create(family, SocketFlags::SocketStream, SocketFlags::ProtocolTcp,
                                    SocketFlags::NonBlocking, SocketFlags::NonInheritable);
    SC_TRY(res);
    return associateExternallyCreatedTCPSocket(outDescriptor);
}

SC::ReturnCode SC::AsyncLoopWakeUp::wakeUp() { return getEventLoop()->wakeUpFromExternalThread(*this); }
