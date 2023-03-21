// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "EventLoop.h"
#include "../Foundation/Optional.h"

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

void SC::EventLoop::addTimeout(IntegerMilliseconds expiration, Async& async, Function<void(AsyncResult&)>&& callback)
{
    Async::Timeout timeout;
    // TODO: Should probably use loopTime
    timeout.expirationTime = TimeCounter().snap().offsetBy(expiration);
    timeout.timeout        = expiration;
    async.operation.assignValue(move(timeout));
    async.callback = move(callback);
    submitAsync(async);
}

void SC::EventLoop::addRead(FileDescriptorNative fd, Async& async)
{
    Async::Read read;
    read.fileDescriptor = fd;
    async.operation.assignValue(move(read));
    submitAsync(async);
}

void SC::EventLoop::submitAsync(Async& async)
{
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
    submission.forEachFrontToBack(
        [&earliestTime](const Async* async)
        {
            if (async->operation.type == Async::Operation::Type::Timeout)
            {
                const auto& expirationTime = async->operation.fields.timeout.expirationTime;
                if (earliestTime == nullptr or earliestTime->isLaterThanOrEqualTo(expirationTime))
                {
                    earliestTime = &expirationTime;
                }
            }
        });
    return earliestTime;
}

void SC::EventLoop::invokeExpiredTimers()
{
    submission.forEachFrontToBack(
        [this](Async* async)
        {
            if (async->operation.type == Async::Operation::Type::Timeout)
            {
                const auto& expirationTime = async->operation.fields.timeout.expirationTime;
                if (loopTime.isLaterThanOrEqualTo(expirationTime))
                {
                    submission.remove(*async);
                    AsyncResult res{*this, *async};
                    async->callback(res);
                }
            }
        });
}

[[nodiscard]] SC::ReturnCode SC::EventLoop::create()
{
    Internal& self = internal.get();
    SC_TRY_IF(self.createEventLoop());
    SC_TRY_IF(self.createWakeup(*this));
    updateTime();
    return true;
}

SC::ReturnCode SC::EventLoop::runOnce()
{
    Internal& self = internal.get();
    updateTime();

    Optional<IntegerMilliseconds> potentialTimeout;
    const TimeCounter*            earliestTimer = findEarliestTimer();
    if (earliestTimer)
    {
        updateTime();
        IntegerMilliseconds timeout = earliestTimer->subtract(loopTime).inMilliseconds();
        if (timeout.ms < 0)
        {
            timeout.ms = 0;
        }
        potentialTimeout = timeout;
    }

    FileDescriptorNative loopFd;
    SC_TRY_IF(self.loopFd.handle.get(loopFd, "Invalid loopFd"_a8));
    for (;;)
    {
        IntegerMilliseconds* actualTimeout = nullptr;
        SC_TRUST_RESULT(potentialTimeout.get(actualTimeout));
        Internal::KernelQueue queue;
        for (Async* async = submission.front; async != nullptr; async = async->next)
        {
            if (async->operation.type == Async::Operation::Type::Read)
            {
                submission.remove(*async);
                SC_TRY_IF(queue.addReadWatcher(self.loopFd, async->operation.fields.read.fileDescriptor));
            }
        }

        SC_TRY_IF(queue.poll(self.loopFd, actualTimeout));
        updateTime();
        // We should be rounding to the upper millisecond or so but this is fine
        loopTime = loopTime.offsetBy(1_ms);
        if (queue.newEvents == 0 && earliestTimer) // if no io event happened that interrupted timeout
        {
            // This will also handle WAIT_TIMEOUT events on windows and EINTR cases
            // When we will be actually dequeing IO it will be important to know whow many
            // actual events we need to handle
            if (not loopTime.isLaterThanOrEqualTo(*earliestTimer))
            {
                IntegerMilliseconds timeout = earliestTimer->subtract(loopTime).inMilliseconds();
                if (timeout.ms < 0)
                {
                    timeout.ms = 0;
                }
                potentialTimeout = timeout;
                continue;
            }
        }
        break;
    }
    invokeExpiredTimers();
    return true;
}
