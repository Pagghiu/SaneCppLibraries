// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Loop.h"
#include "../Foundation/Optional.h"

#if SC_PLATFORM_WINDOWS
#include "LoopInternalWindows.inl"
#elif SC_PLATFORM_EMSCRIPTEN
#include "LoopInternalEmscripten.inl"
#elif SC_PLATFORM_APPLE
#include "LoopInternalApple.inl"
#endif

template <>
void SC::OpaqueFunctions<SC::Loop::Internal, SC::Loop::InternalSize, SC::Loop::InternalAlignment>::construct(
    Handle& buffer)
{
    new (&buffer.reinterpret_as<Object>(), PlacementNew()) Object();
}

template <>
void SC::OpaqueFunctions<SC::Loop::Internal, SC::Loop::InternalSize, SC::Loop::InternalAlignment>::destruct(Object& obj)
{
    obj.~Internal();
}

SC::ReturnCode SC::Loop::addTimer(IntegerMilliseconds expiration, Action&& callback)
{
    Timer timerToInsert;
    timerToInsert.callback       = forward<Action>(callback);
    timerToInsert.expiration     = expiration;
    timerToInsert.expirationTime = TimeCounter().snap().offsetBy(expiration);
    SC_TRY_IF(registeredTimers.push_back(move(timerToInsert)));
    return true;
}

SC::ReturnCode SC::Loop::run()
{
    while (!shouldQuit())
    {
        SC_TRY_IF(runOnce());
    }
    return true;
}

const SC::TimeCounter* SC::Loop::findEarliestTimer() const
{
    if (registeredTimers.isEmpty())
    {
        return nullptr;
    }
    const TimeCounter* earliestTime = &registeredTimers[0].expirationTime;
    size_t             foundIndex   = 0;
    for (size_t idx = 1; idx < registeredTimers.size(); ++idx)
    {
        const auto& timer = registeredTimers[idx];
        if (earliestTime->isLaterThanOrEqualTo(timer.expirationTime))
        {
            foundIndex   = idx;
            earliestTime = &timer.expirationTime;
        }
    }
    return earliestTime;
}

void SC::Loop::invokeExpiredTimers()
{
    for (size_t idx = 1; idx <= registeredTimers.size(); ++idx)
    {
        const Timer& timer = registeredTimers[idx - 1];
        if (loopTime.isLaterThanOrEqualTo(timer.expirationTime))
        {
            timer.callback();
            SC_TRUST_RESULT(registeredTimers.removeAt(idx - 1));
            idx--;
        }
    }
}

[[nodiscard]] SC::ReturnCode SC::Loop::create()
{
    Internal& self = internal.get();
    SC_TRY_IF(self.createLoop());
    SC_TRY_IF(self.createLoopAsyncWakeup());
    updateTime();
    return true;
}

SC::ReturnCode SC::Loop::runOnce()
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
    else if (not registeredTimers.isEmpty())
    {
        // This means that all timers are already expired, but don't want to block for IO indefintively
        potentialTimeout = IntegerMilliseconds(0);
    }
    FileDescriptorNative loopFd;
    SC_TRY_IF(self.loopFd.handle.get(loopFd, "Invalid loopFd"_a8));
    for (;;)
    {
        IntegerMilliseconds* actualTimeout = nullptr;
        SC_TRUST_RESULT(potentialTimeout.get(actualTimeout));
        Internal::KernelQueue queue;
        for (FileDescriptorNative fd : self.watchersQueue)
        {
            SC_TRY_IF(queue.addReadWatcher(self.loopFd, fd));
        }
        self.watchersQueue.clear();
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
