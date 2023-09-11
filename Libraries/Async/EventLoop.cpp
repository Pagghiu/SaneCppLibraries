// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "EventLoop.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Result.h"
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
    case Type::Timeout: return "Timeout"_a8;
    case Type::WakeUp: return "WakeUp"_a8;
    case Type::ProcessExit: return "ProcessExit"_a8;
    case Type::Accept: return "Accept"_a8;
    case Type::Connect: return "Connect"_a8;
    case Type::Send: return "Send"_a8;
    case Type::Receive: return "Receive"_a8;
    case Type::Read: return "Read"_a8;
    case Type::Write: return "Write"_a8;
    }
    SC_UNREACHABLE();
}

SC::ReturnCode SC::EventLoop::startTimeout(AsyncTimeout& async, IntegerMilliseconds expiration,
                                           Function<void(AsyncTimeoutResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    updateTime();
    async.expirationTime = loopTime.offsetBy(expiration);
    async.timeout        = expiration;
    return true;
}

SC::ReturnCode SC::EventLoop::startRead(AsyncRead& async, FileDescriptor::Handle fileDescriptor, Span<char> readBuffer,
                                        Function<void(AsyncReadResult&)>&& callback)
{
    SC_TRY_MSG(readBuffer.sizeInBytes() > 0, "EventLoop::startRead - Zero sized read buffer"_a8);
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(queueSubmission(async));
    async.callback       = callback;
    async.fileDescriptor = fileDescriptor;
    async.readBuffer     = readBuffer;
    return true;
}

SC::ReturnCode SC::EventLoop::startWrite(AsyncWrite& async, FileDescriptor::Handle fileDescriptor,
                                         Span<const char> writeBuffer, Function<void(AsyncWriteResult&)>&& callback)
{
    SC_TRY_MSG(writeBuffer.sizeInBytes() > 0, "EventLoop::startWrite - Zero sized write buffer"_a8);
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(queueSubmission(async));
    async.callback       = callback;
    async.fileDescriptor = fileDescriptor;
    async.writeBuffer    = writeBuffer;
    return true;
}

SC::ReturnCode SC::EventLoop::startAccept(AsyncAccept& async, const SocketDescriptor& socketDescriptor,
                                          Function<void(AsyncAcceptResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(socketDescriptor.getAddressFamily(async.addressFamily));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    return true;
}

SC::ReturnCode SC::EventLoop::startConnect(AsyncConnect& async, const SocketDescriptor& socketDescriptor,
                                           SocketIPAddress ipAddress, Function<void(AsyncConnectResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(queueSubmission(async));
    async.callback  = callback;
    async.ipAddress = ipAddress;
    return true;
}

SC::ReturnCode SC::EventLoop::startSend(AsyncSend& async, const SocketDescriptor& socketDescriptor,
                                        Span<const char> data, Function<void(AsyncSendResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    async.data     = data;
    return true;
}

SC::ReturnCode SC::EventLoop::startReceive(AsyncReceive& async, const SocketDescriptor& socketDescriptor,
                                           Span<char> data, Function<void(AsyncReceiveResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    async.data     = data;
    return true;
}

SC::ReturnCode SC::EventLoop::startWakeUp(AsyncWakeUp& async, Function<void(AsyncWakeUpResult&)>&& callback,
                                          EventObject* eventObject)
{
    SC_TRY_IF(queueSubmission(async));
    async.callback    = callback;
    async.eventObject = eventObject;
    return true;
}

SC::ReturnCode SC::EventLoop::startProcessExit(AsyncProcessExit&                         async,
                                               Function<void(AsyncProcessExitResult&)>&& callback,
                                               ProcessDescriptor::Handle                 process)
{
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    async.handle   = process;
    return true;
}

SC::ReturnCode SC::EventLoop::validateAsync(Async& async)
{
    const bool asyncStateIsFree      = async.state == Async::State::Free;
    const bool asyncIsNotOwnedByLoop = async.eventLoop == nullptr;
    SC_LOG_MESSAGE("{} {} QUEUE\n", async.debugName, Async::TypeToString(async.getType()));
    SC_TRY_MSG(asyncStateIsFree, "Trying to stage Async that is in use"_a8);
    SC_TRY_MSG(asyncIsNotOwnedByLoop, "Trying to add Async belonging to another Loop"_a8);
    return true;
}

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
        SC_TRY_IF(runOnce());
    };
    return true;
}

const SC::TimeCounter* SC::EventLoop::findEarliestTimer() const
{
    const TimeCounter* earliestTime = nullptr;
    for (Async* async = activeTimers.front; async != nullptr; async = async->next)
    {
        SC_DEBUG_ASSERT(async->getType() == Async::Type::Timeout);
        const auto& expirationTime = async->asTimeout()->expirationTime;
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
        SC_DEBUG_ASSERT(async->getType() == Async::Type::Timeout);
        const auto& expirationTime = async->asTimeout()->expirationTime;
        if (loopTime.isLaterThanOrEqualTo(expirationTime))
        {
            Async* currentAsync = async;
            async               = async->next;
            activeTimers.remove(*currentAsync);
            currentAsync->state      = Async::State::Free;
            ReturnCode           res = true;
            AsyncResult::Timeout result(*currentAsync, nullptr, -1, move(res));
            result.async.eventLoop = nullptr; // Allow reusing it
            currentAsync->asTimeout()->callback(result);
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
    SC_TRY_IF(self.createEventLoop());
    SC_TRY_IF(self.createWakeup(*this));
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
        SC_TRY_IF(setupAsync(queue, async));
        SC_TRY_IF(activateAsync(queue, async));
    }
    break;
    case Async::State::Free: {
        // TODO: Stop the completion, it has been cancelled before being submitted
        SC_RELEASE_ASSERT(false);
    }
    break;
    case Async::State::Cancelling: {
        SC_TRY_IF(cancelAsync(queue, async));
    }
    break;
    case Async::State::Active: {
        SC_DEBUG_ASSERT(false);
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

    if (getTotalNumberOfActiveHandle() <= 0)
        return true; // happens when we do cancelAsync on the last active one
    SC_LOG_MESSAGE("Active Requests Before Poll = {}\n", getTotalNumberOfActiveHandle());
    SC_TRY_IF(queue.pollAsync(*this, pollMode));
    SC_LOG_MESSAGE("Active Requests After Poll = {}\n", getTotalNumberOfActiveHandle());

    Internal& self = internal.get();
    for (decltype(KernelQueue::newEvents) idx = 0; idx < queue.newEvents; ++idx)
    {
        SC_LOG_MESSAGE(" Iteration = {}\n", (int)idx);
        SC_LOG_MESSAGE(" Active Requests = {}\n", getTotalNumberOfActiveHandle());
        bool continueProcessing = true;

        ReturnCode result       = queue.validateEvent(queue.events[idx], continueProcessing);
        Async*     asyncPointer = self.getAsync(queue.events[idx]);
        SC_RELEASE_ASSERT(asyncPointer);
        Async& async = *asyncPointer;

        if (not result)
        {
            reportError(queue, async, move(result));
            continue;
        }

        if (not continueProcessing)
            continue;

        bool  reactivate = false;
        void* userData   = self.getUserData(queue.events[idx]);
        completeAsync(queue, async, userData, static_cast<int32_t>(idx), move(result), reactivate);

        if (async.state == Async::State::Active)
        {
            if (reactivate)
            {
                result = activateAsync(queue, async);
            }
            else
            {
                result = cancelAsync(queue, async);
            }
            if (not result)
            {
                reportError(queue, async, move(result));
                continue;
            }
        }
    }
    SC_LOG_MESSAGE("Active Requests After Completion = {}\n", getTotalNumberOfActiveHandle());
    return true;
}

SC::ReturnCode SC::EventLoop::setupAsync(KernelQueue& queue, Async& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, Async::TypeToString(async.getType()));
    switch (async.getType())
    {
    case Async::Type::Timeout:
        activeTimers.queueBack(async);
        numberOfTimers += 1;
        return true;
    case Async::Type::WakeUp:
        activeWakeUps.queueBack(async);
        numberOfWakeups += 1;
        return true;
    case Async::Type::ProcessExit: SC_TRY_IF(queue.setupAsync(*async.asProcessExit())); break;
    case Async::Type::Accept: SC_TRY_IF(queue.setupAsync(*async.asAccept())); break;
    case Async::Type::Connect: SC_TRY_IF(queue.setupAsync(*async.asConnect())); break;
    case Async::Type::Send: SC_TRY_IF(queue.setupAsync(*async.asSend())); break;
    case Async::Type::Receive: SC_TRY_IF(queue.setupAsync(*async.asReceive())); break;
    case Async::Type::Read: SC_TRY_IF(queue.setupAsync(*async.asRead())); break;
    case Async::Type::Write: SC_TRY_IF(queue.setupAsync(*async.asWrite())); break;
    }
    return true;
}

SC::ReturnCode SC::EventLoop::activateAsync(KernelQueue& queue, Async& async)
{
    SC_LOG_MESSAGE("{} {} ACTIVATE\n", async.debugName, Async::TypeToString(async.getType()));
    // Submitting (first time) or Active (for reactivations)
    SC_DEBUG_ASSERT(async.state == Async::State::Active or async.state == Async::State::Submitting);
    switch (async.getType())
    {
    case Async::Type::Timeout: async.state = Async::State::Active; return true;
    case Async::Type::WakeUp: async.state = Async::State::Active; return true;
    case Async::Type::Accept: SC_TRY_IF(queue.activateAsync(*async.asAccept())); break;
    case Async::Type::Connect: SC_TRY_IF(queue.activateAsync(*async.asConnect())); break;
    case Async::Type::Receive: SC_TRY_IF(queue.activateAsync(*async.asReceive())); break;
    case Async::Type::Send: SC_TRY_IF(queue.activateAsync(*async.asSend())); break;
    case Async::Type::Read: SC_TRY_IF(queue.activateAsync(*async.asRead())); break;
    case Async::Type::Write: SC_TRY_IF(queue.activateAsync(*async.asWrite())); break;
    case Async::Type::ProcessExit: SC_TRY_IF(queue.activateAsync(*async.asProcessExit())); break;
    }
    if (async.state == Async::State::Submitting)
    {
        return queue.pushStagedAsync(async);
    }
    return true;
}

void SC::EventLoop::reportError(KernelQueue& queue, Async& async, ReturnCode&& returnCode)
{
    SC_LOG_MESSAGE("{} ERROR {}\n", async.debugName, Async::TypeToString(async.getType()));
    bool reactivate = false;
    if (async.state == Async::State::Active)
    {
        removeActiveHandle(async);
    }
    completeAsync(queue, async, nullptr, -1, forward<ReturnCode>(returnCode), reactivate);
    async.state = Async::State::Free;
}

void SC::EventLoop::completeAsync(KernelQueue& queue, Async& async, void* userData, int32_t eventIndex,
                                  ReturnCode&& returnCode, bool& reactivate)
{
    if (returnCode)
    {
        SC_LOG_MESSAGE("{} {} COMPLETE\n", async.debugName, Async::TypeToString(async.getType()));
    }
    else
    {
        SC_LOG_MESSAGE("{} {} COMPLETE (Error = \"{}\")\n", async.debugName, Async::TypeToString(async.getType()),
                       returnCode.message);
    }
    switch (async.getType())
    {
    case Async::Type::Timeout: {
        AsyncResult::Timeout result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    case Async::Type::Read: {
        AsyncResult::Read result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    case Async::Type::Write: {
        AsyncResult::Write result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    case Async::Type::WakeUp: {
        AsyncResult::WakeUp result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    case Async::Type::ProcessExit: {
        AsyncResult::ProcessExit result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    case Async::Type::Accept: {
        AsyncResult::Accept result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    case Async::Type::Connect: {
        AsyncResult::Connect result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    case Async::Type::Send: {
        AsyncResult::Send result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    case Async::Type::Receive: {
        AsyncResult::Receive result(async, userData, eventIndex, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.isRearmed();
        break;
    }
    }
}

SC::ReturnCode SC::EventLoop::cancelAsync(KernelQueue& queue, Async& async)
{
    SC_LOG_MESSAGE("{} {} CANCEL\n", async.debugName, Async::TypeToString(async.getType()));
    switch (async.getType())
    {
    case Async::Type::Timeout: numberOfTimers -= 1; return true;
    case Async::Type::WakeUp: numberOfWakeups -= 1; return true;
    case Async::Type::Read: SC_TRY_IF(queue.stopAsync(*async.asRead())); break;
    case Async::Type::Write: SC_TRY_IF(queue.stopAsync(*async.asWrite())); break;
    case Async::Type::ProcessExit: SC_TRY_IF(queue.stopAsync(*async.asProcessExit())); break;
    case Async::Type::Accept: SC_TRY_IF(queue.stopAsync(*async.asAccept())); break;
    case Async::Type::Connect: SC_TRY_IF(queue.stopAsync(*async.asConnect())); break;
    case Async::Type::Send: SC_TRY_IF(queue.stopAsync(*async.asSend())); break;
    case Async::Type::Receive: SC_TRY_IF(queue.stopAsync(*async.asReceive())); break;
    }
    if (async.state == Async::State::Active)
    {
        removeActiveHandle(async);
    }
    return true;
}

SC::ReturnCode SC::EventLoop::stopAsync(Async& async)
{
    SC_LOG_MESSAGE("{} {} STOP\n", async.debugName, Async::TypeToString(async.getType()));
    const bool asyncStateIsNotFree    = async.state != Async::State::Free;
    const bool asyncIsOwnedByThisLoop = async.eventLoop == this;
    SC_TRY_MSG(asyncStateIsNotFree, "Trying to stop Async that is not active"_a8);
    SC_TRY_MSG(asyncIsOwnedByThisLoop, "Trying to add Async belonging to another Loop"_a8);
    switch (async.state)
    {
    case Async::State::Active: {
        if (async.getType() == Async::Type::Timeout)
        {
            activeTimers.remove(async);
        }
        else if (async.getType() == Async::Type::WakeUp)
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

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread(AsyncWakeUp& wakeUp)
{
    SC_TRY_MSG(wakeUp.eventLoop == this,
               "EventLoop::wakeUpFromExternalThread - Wakeup belonging to different EventLoop"_a8);
    if (not wakeUp.asWakeUp()->pending.exchange(true))
    {
        // This executes if current thread is lucky enough to atomically exchange pending from false to true.
        // This effectively allows coalescing calls from different threads into a single notification.
        SC_TRY_IF(wakeUpFromExternalThread());
    }
    return true;
}

void SC::EventLoop::executeWakeUps(AsyncResult& result)
{
    for (Async* async = activeWakeUps.front; async != nullptr; async = async->next)
    {
        Async::WakeUp* notifier = async->asWakeUp();
        if (notifier->pending.load() == true)
        {
            ReturnCode          res = true;
            AsyncResult::WakeUp asyncResult(*async, nullptr, result.index, move(res));
            asyncResult.async.callback(asyncResult);
            if (notifier->eventObject)
            {
                notifier->eventObject->signal();
            }
            result.rearm(asyncResult.isRearmed());
            notifier->pending.exchange(false); // allow executing the notification again
        }
    }
}

void SC::EventLoop::removeActiveHandle(Async& async)
{
    SC_RELEASE_ASSERT(async.state == Async::State::Active)
    async.eventLoop->activeHandles.remove(async);
    async.state = Async::State::Free;
    async.eventLoop->numberOfActiveHandles -= 1;
}

void SC::EventLoop::addActiveHandle(Async& async)
{
    SC_RELEASE_ASSERT(async.state == Async::State::Submitting);
    async.state = Async::State::Active;
    async.eventLoop->activeHandles.queueBack(async);
    async.eventLoop->numberOfActiveHandles += 1;
}

SC::ReturnCode SC::EventLoop::getLoopFileDescriptor(SC::FileDescriptor::Handle& fileDescriptor) const
{
    return internal.get().loopFd.get(fileDescriptor, "EventLoop::getLoopFileDescriptor invalid handle"_a8);
}

SC::ReturnCode SC::EventLoop::createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor)
{
    auto res = outDescriptor.create(family, SocketFlags::SocketStream, SocketFlags::ProtocolTcp,
                                    SocketFlags::NonBlocking, SocketFlags::NonInheritable);
    SC_TRY_IF(res);
    return associateExternallyCreatedTCPSocket(outDescriptor);
}

SC::ReturnCode SC::AsyncWakeUp::wakeUp() { return eventLoop->wakeUpFromExternalThread(*this); }
