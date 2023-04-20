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

SC::ReturnCode SC::EventLoop::addTimeout(AsyncTimeout& async, IntegerMilliseconds expiration,
                                         Function<void(AsyncResult&)>&& callback)
{
    Async::Timeout operation;
    updateTime();
    operation.expirationTime = loopTime.offsetBy(expiration);
    operation.timeout        = expiration;
    async.operation.assignValue(move(operation));
    async.callback = move(callback);
    submitAsync(async);
    return true;
}

SC::ReturnCode SC::EventLoop::addRead(AsyncRead& async, FileDescriptorNative fileDescriptor, Span<uint8_t> readBuffer)
{
    SC_TRY_MSG(readBuffer.sizeInBytes() > 0, "EventLoop::addRead - Zero sized read buffer"_a8);
    Async::Read operation;
    operation.fileDescriptor = fileDescriptor;
    operation.readBuffer     = readBuffer;
    async.operation.assignValue(move(operation));
    submitAsync(async);
    return true;
}

SC::ReturnCode SC::EventLoop::addWakeUp(AsyncWakeUp& async, Function<void(AsyncResult&)>&& callback,
                                        EventObject* eventObject)
{
    Async::WakeUp operation;
    operation.eventObject = eventObject;
    async.operation.assignValue(move(operation));
    async.callback = move(callback);
    submitAsync(async);
    return true;
}

SC::ReturnCode SC::EventLoop::addProcessExit(AsyncProcessExit& async, Function<void(AsyncResult&)>&& callback,
                                             ProcessNative process)
{
    Async::ProcessExit operation;
    operation.handle = process;
    async.operation.assignValue(move(operation));
    async.callback = move(callback);
    submitAsync(async);
    return true;
}

void SC::EventLoop::submitAsync(Async& async)
{
    SC_RELEASE_ASSERT(async.state == Async::State::Free);
    SC_RELEASE_ASSERT(async.eventLoop == nullptr);
    async.state = Async::State::Submitting;
    submission.queueBack(async);
    async.eventLoop = this;
}

bool SC::EventLoop::shouldQuit() { return submission.isEmpty(); }

SC::ReturnCode SC::EventLoop::run()
{
    while (!shouldQuit())
    {
        SC_TRY_IF(runOnce());
    }
    return true;
}

const SC::TimeCounter* SC::EventLoop::findEarliestTimer() const
{
    const TimeCounter* earliestTime = nullptr;
    for (Async* async = activeTimers.front; async != nullptr; async = async->next)
    {
        SC_RELEASE_ASSERT(async->operation.type == Async::Type::Timeout);
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
        SC_RELEASE_ASSERT(async->operation.type == Async::Type::Timeout);
        const auto& expirationTime = async->operation.fields.timeout.expirationTime;
        if (loopTime.isLaterThanOrEqualTo(expirationTime))
        {
            Async* currentAsync = async;
            async               = async->next;
            activeTimers.remove(*currentAsync);
            currentAsync->state = Async::State::Free;
            AsyncResult res{*this, *currentAsync};
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
    while (Async* async = submission.dequeueFront())
    {
        switch (async->state)
        {
        case Async::State::Submitting: {
            SC_TRY_IF(queue.pushAsync(*this, async));
            async->state = Async::State::Active;
            if (queue.isFull())
            {
                SC_TRY_IF(queue.commitQueue(*this));
                stagedHandles.clear();
            }
        }
        break;
        case Async::State::Free: {
            // TODO: Stop the completion, it has been cancelled before being submitted
            SC_RELEASE_ASSERT(false);
        }
        break;
        case Async::State::Cancelling: {
            // TODO: issue an actual kevent for removal
            SC_RELEASE_ASSERT(false);
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

SC::ReturnCode SC::EventLoop::runOnce()
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

    SC_RELEASE_ASSERT(submission.isEmpty());

    const TimeCounter* nextTimer = findEarliestTimer();
    SC_TRY_IF(queue.poll(*this, nextTimer));
    stagedHandles.clear();

    if (nextTimer)
    {
        const bool timeoutOccurredWithoutIO = queue.newEvents == 0;
        const bool timeoutWasAlreadyExpired = loopTime.isLaterThanOrEqualTo(*nextTimer);
        if (timeoutOccurredWithoutIO or timeoutWasAlreadyExpired)
        {
            if (timeoutWasAlreadyExpired)
            {
                // Not sure if this is really possible.
                updateTime();
            }
            else
            {
                loopTime = *nextTimer;
            }
            invokeExpiredTimers();
        }
    }

    Internal& self = internal.get();
    for (decltype(KernelQueue::newEvents) idx = 0; idx < queue.newEvents; ++idx)
    {
        Async*      async = self.getAsync(queue.events[idx]);
        AsyncResult result{*this, *async, self.getUserData(queue.events[idx])};
        auto        res = self.runCompletionFor(result, queue.events[idx]);
        if (res and result.async.callback.isValid())
        {
            result.async.callback(result);
        }
    }

    return true;
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

void SC::EventLoop::runCompletionForNotifiers()
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

SC::ReturnCode SC::EventLoop::getLoopFileDescriptor(SC::FileDescriptorNative& fileDescriptor) const
{
    return internal.get().loopFd.handle.get(fileDescriptor, "EventLoop::getLoopFileDescriptor invalid handle"_a8);
}

SC::ReturnCode SC::AsyncWakeUp::wakeUp() { return eventLoop->wakeUpFromExternalThread(*this); }
