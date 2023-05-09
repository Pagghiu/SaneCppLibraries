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

template <>
void SC::OpaqueFuncs<SC::Async::ProcessExitTraits>::construct(Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}
template <>
void SC::OpaqueFuncs<SC::Async::ProcessExitTraits>::destruct(Object& obj)
{
    obj.~Object();
}
template <>
void SC::OpaqueFuncs<SC::Async::ProcessExitTraits>::moveConstruct(Handle& buffer, Object&& obj)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object(move(obj));
}
template <>
void SC::OpaqueFuncs<SC::Async::ProcessExitTraits>::moveAssign(Object& pthis, Object&& obj)
{
    pthis = move(obj);
}

SC::ReturnCode SC::EventLoop::startTimeout(AsyncTimeout& async, IntegerMilliseconds expiration,
                                           Function<void(AsyncResult&)>&& callback)
{
    SC_TRY_IF(queueSubmission(async, move(callback)));
    Async::Timeout operation;
    updateTime();
    operation.expirationTime = loopTime.offsetBy(expiration);
    operation.timeout        = expiration;
    async.operation.assignValue(move(operation));
    return true;
}

SC::ReturnCode SC::EventLoop::startRead(AsyncRead& async, FileDescriptor::Handle fileDescriptor,
                                        Span<uint8_t> readBuffer, Function<void(AsyncResult&)>&& callback)
{
    SC_TRY_MSG(readBuffer.sizeInBytes() > 0, "EventLoop::startRead - Zero sized read buffer"_a8);
    SC_TRY_IF(queueSubmission(async, move(callback)));
    Async::Read operation;
    operation.fileDescriptor = fileDescriptor;
    operation.readBuffer     = readBuffer;
    async.operation.assignValue(move(operation));
    return true;
}

SC::ReturnCode SC::EventLoop::startAccept(AsyncAccept& async, AsyncAccept::Support& support,
                                          const SocketDescriptor&        socketDescriptor,
                                          Function<void(AsyncResult&)>&& callback)
{
    SC_TRY_IF(queueSubmission(async, move(callback)));
    Async::Accept operation;
    SC_TRY_IF(socketDescriptor.get(operation.handle, "Invalid handle"_a8));
    SC_TRY_IF(socketDescriptor.getAddressFamily(operation.addressFamily));
    operation.support = &support;
    async.operation.assignValue(move(operation));
    return true;
}

SC::ReturnCode SC::EventLoop::startConnect(AsyncConnect& async, Async::ConnectSupport& support,
                                           const SocketDescriptor& socketDescriptor, SocketIPAddress ipAddress,
                                           Function<void(AsyncResult&)>&& callback)
{
    SC_TRY_IF(queueSubmission(async, move(callback)));
    Async::Connect operation;
    SC_TRY_IF(socketDescriptor.get(operation.handle, "Invalid handle"_a8));
    operation.support            = &support;
    operation.support->ipAddress = ipAddress;
    async.operation.assignValue(move(operation));
    return true;
}

SC::ReturnCode SC::EventLoop::startSend(AsyncSend& async, AsyncSend::Support& support,
                                        const SocketDescriptor& socketDescriptor, Span<const char> data,
                                        Function<void(AsyncResult&)>&& callback)
{
    SC_TRY_IF(queueSubmission(async, move(callback)));
    Async::Send operation;
    SC_TRY_IF(socketDescriptor.get(operation.handle, "Invalid handle"_a8));
    operation.data    = data;
    operation.support = &support;
    async.operation.assignValue(move(operation));
    return true;
}

SC::ReturnCode SC::EventLoop::startWakeUp(AsyncWakeUp& async, Function<void(AsyncResult&)>&& callback,
                                          EventObject* eventObject)
{
    SC_TRY_IF(queueSubmission(async, move(callback)));
    Async::WakeUp operation;
    operation.eventObject = eventObject;
    async.operation.assignValue(move(operation));
    return true;
}

SC::ReturnCode SC::EventLoop::startProcessExit(AsyncProcessExit& async, Function<void(AsyncResult&)>&& callback,
                                               ProcessDescriptor::Handle process)
{
    SC_TRY_IF(queueSubmission(async, move(callback)));
    Async::ProcessExit operation;
    operation.handle = process;
    async.operation.assignValue(move(operation));
    return true;
}

SC::ReturnCode SC::EventLoop::queueSubmission(Async& async, Function<void(AsyncResult&)>&& callback)
{
    const bool asyncStateIsFree      = async.state == Async::State::Free;
    const bool asyncIsNotOwnedByLoop = async.eventLoop == nullptr;
    SC_DEBUG_ASSERT(asyncStateIsFree and asyncIsNotOwnedByLoop);
    SC_TRY_MSG(asyncStateIsFree, "Trying to stage Async that is in use"_a8);
    SC_TRY_MSG(asyncIsNotOwnedByLoop, "Trying to add Async belonging to another Loop"_a8);
    async.callback = move(callback);
    async.state    = Async::State::Submitting;
    submissions.queueBack(async);
    async.eventLoop = this;
    return true;
}

SC::ReturnCode SC::EventLoop::run()
{
    do
    {
        SC_TRY_IF(runOnce());
    } while (numberOfActiveHandles > 0);
    return true;
}

const SC::TimeCounter* SC::EventLoop::findEarliestTimer() const
{
    const TimeCounter* earliestTime = nullptr;
    for (Async* async = activeTimers.front; async != nullptr; async = async->next)
    {
        SC_DEBUG_ASSERT(async->operation.type == Async::Type::Timeout);
        const auto& expirationTime = async->operation.fields.timeout.expirationTime;
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
        SC_DEBUG_ASSERT(async->operation.type == Async::Type::Timeout);
        const auto& expirationTime = async->operation.fields.timeout.expirationTime;
        if (loopTime.isLaterThanOrEqualTo(expirationTime))
        {
            Async* currentAsync = async;
            async               = async->next;
            activeTimers.remove(*currentAsync);
            currentAsync->state = Async::State::Free;
            AsyncResult res{*this, *currentAsync};
            res.async.eventLoop = nullptr; // Allow reusing it
            currentAsync->callback(res);
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

SC::ReturnCode SC::EventLoop::stageSubmissions(SC::EventLoop::KernelQueue& queue)
{
    while (Async* async = submissions.dequeueFront())
    {
        switch (async->state)
        {
        case Async::State::Submitting: {
            SC_TRY_IF(queue.stageAsync(*this, *async));
            SC_TRY_IF(queue.activateAsync(*this, *async));
            async->state = Async::State::Active;
        }
        break;
        case Async::State::Free: {
            // TODO: Stop the completion, it has been cancelled before being submitted
            SC_RELEASE_ASSERT(false);
        }
        break;
        case Async::State::Cancelling: {
            SC_TRY_IF(queue.cancelAsync(*this, *async));
        }
        break;
        case Async::State::Active: {
            SC_DEBUG_ASSERT(false);
            return "EventLoop::processSubmissions() got Active handle"_a8;
        }
        break;
        }
    }
    return true;
}

SC::ReturnCode SC::EventLoop::runOnce() { return runStep(PollMode::ForcedForwardProgress); }
SC::ReturnCode SC::EventLoop::runNoWait() { return runStep(PollMode::NoWait); }

SC::ReturnCode SC::EventLoop::runStep(PollMode pollMode)
{
    KernelQueue queue;

    auto defer = MakeDeferred(
        [this]
        {
            if (not stagedHandles.isEmpty())
            {
                // TODO: we should re-append stagedHandles to submissions and transition them to Submitting state
                SC_RELEASE_ASSERT(false);
            }
        });
    SC_TRY_IF(stageSubmissions(queue));

    SC_RELEASE_ASSERT(submissions.isEmpty());

    SC_TRY_IF(queue.pollAsync(*this, pollMode));

    Internal& self = internal.get();
    for (decltype(KernelQueue::newEvents) idx = 0; idx < queue.newEvents; ++idx)
    {
        Async&      async = *self.getAsync(queue.events[idx]);
        AsyncResult result{*this, async, self.getUserData(queue.events[idx])};
        ReturnCode  res = self.runCompletionFor(result, queue.events[idx]);
        if (res and result.async.callback.isValid())
        {
            result.async.callback(result);
        }
        switch (async.state)
        {
        case Async::State::Active: SC_TRY_IF(queue.activateAsync(*this, async)); break;
        default: break;
        }
    }

    return true;
}

SC::ReturnCode SC::EventLoop::stopAsync(Async& async)
{
    const bool asyncStateIsNotFree    = async.state != Async::State::Free;
    const bool asyncIsOwnedByThisLoop = async.eventLoop == this;
    SC_DEBUG_ASSERT(asyncStateIsNotFree and asyncIsOwnedByThisLoop);
    SC_TRY_MSG(asyncStateIsNotFree, "Trying to stop Async that is not active"_a8);
    SC_TRY_MSG(asyncIsOwnedByThisLoop, "Trying to add Async belonging to another Loop"_a8);
    switch (async.state)
    {
    case Async::State::Active: {
        // We don't know in which queue this is gone so we remove from all
        activeHandles.remove(async);
        activeTimers.remove(async);
        activeWakeUps.remove(async);
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
    if (not wakeUp.operation.fields.wakeUp.pending.exchange(true))
    {
        // This executes if current thread is lucky enough to atomically exchange pending from false to true.
        // This effectively allows coalescing calls from different threads into a single notification.
        SC_TRY_IF(wakeUpFromExternalThread());
    }
    return true;
}

void SC::EventLoop::executeWakeUps()
{
    for (Async* async = activeWakeUps.front; async != nullptr; async = async->next)
    {
        Async::WakeUp* notifier = &async->operation.fields.wakeUp;
        if (notifier->pending.load() == true)
        {
            AsyncResult res{*this, *async};
            async->callback(res);
            if (notifier->eventObject)
            {
                notifier->eventObject->signal();
            }
            notifier->pending.exchange(false); // allow executing the notification again
        }
    }
}

SC::ReturnCode SC::EventLoop::getLoopFileDescriptor(SC::FileDescriptor::Handle& fileDescriptor) const
{
    return internal.get().loopFd.get(fileDescriptor, "EventLoop::getLoopFileDescriptor invalid handle"_a8);
}

SC::ReturnCode SC::AsyncWakeUp::wakeUp() { return eventLoop->wakeUpFromExternalThread(*this); }
