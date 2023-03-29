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
void SC::OpaqueFunctions<SC::EventLoop::Internal, SC::EventLoop::InternalSize,
                         SC::EventLoop::InternalAlignment>::construct(Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}

template <>
void SC::OpaqueFunctions<SC::EventLoop::Internal, SC::EventLoop::InternalSize,
                         SC::EventLoop::InternalAlignment>::destruct(Object& obj)
{
    obj.~Internal();
}

SC::ReturnCode SC::EventLoop::addTimeout(AsyncTimeout& async, IntegerMilliseconds expiration,
                                         Function<void(AsyncResult&)>&& callback)
{
    Async::Timeout timeout;
    updateTime();
    timeout.expirationTime = loopTime.offsetBy(expiration);
    timeout.timeout        = expiration;
    async.operation.assignValue(move(timeout));
    async.callback = move(callback);
    submitAsync(async);
    return true;
}

SC::ReturnCode SC::EventLoop::addRead(AsyncRead& async, FileDescriptorNative fileDescriptor, Span<uint8_t> readBuffer)
{
    SC_TRY_MSG(readBuffer.sizeInBytes() > 0, "EventLoop::addRead - Zero sized read buffer"_a8);
    Async::Read readOp;
    readOp.fileDescriptor = fileDescriptor;
    readOp.readBuffer     = readBuffer;
    async.operation.assignValue(move(readOp));
    submitAsync(async);
    return true;
}

SC::ReturnCode SC::EventLoop::addWakeUp(AsyncWakeUp& async, Function<void(EventLoop&)>&& callback,
                                        EventObject* eventObject)
{
    Async::WakeUp wakeUp;
    wakeUp.callback    = move(callback);
    wakeUp.eventObject = eventObject;
    wakeUp.eventLoop   = this;
    async.operation.assignValue(move(wakeUp));
    submitAsync(async);
    return true;
}

void SC::EventLoop::submitAsync(Async& async)
{
    SC_RELEASE_ASSERT(async.state == Async::State::Free);
    async.state = Async::State::Submitting;
    submission.queueBack(async);
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
        SC_RELEASE_ASSERT(async->operation.type == Async::Operation::Type::Timeout);
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
        SC_RELEASE_ASSERT(async->operation.type == Async::Operation::Type::Timeout);
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

[[nodiscard]] SC::ReturnCode SC::EventLoop::create()
{
    Internal& self = internal.get();
    SC_TRY_IF(self.createEventLoop());
    SC_TRY_IF(self.createWakeup(*this));
    return true;
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

    for (decltype(KernelQueue::newEvents) idx = 0; idx < queue.newEvents; ++idx)
    {
        Async* async = internal.get().getAsync(queue.events[idx]);
        if (internal.get().isWakeUp(queue.events[idx]))
        {
            if (async)
            {
                internal.get().runCompletionForWakeUp(*async);
            }
            runCompletionForNotifiers();
        }
        if (async)
        {
            switch (async->operation.type)
            {
            case Async::Operation::Type::Timeout: {
                return "Unexpected operation"_a8;
            }
            break;
            case Async::Operation::Type::Read: {
            }
            break;
            case Async::Operation::Type::WakeUp: {
            }
            break;
            }
        }
    }

    return true;
}

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread(Async::WakeUp& wakeUp)
{
    SC_TRY_MSG(wakeUp.eventLoop == this,
               "EventLoop::wakeUpFromExternalThread - Wakeup belonging to different EventLoop"_a8);
    if (not wakeUp.pending.exchange(true))
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
            notifier->callback(*this);
            if (notifier->eventObject)
            {
                notifier->eventObject->signal();
            }
            notifier->pending.exchange(false); // allow executing the notification again
        }
    }
}

[[nodiscard]] SC::ReturnCode SC::AsyncWakeUp::wakeUp()
{
    Async::WakeUp& self = *operation.unionAs<Async::WakeUp>();
    return self.eventLoop->wakeUpFromExternalThread(self);
}
