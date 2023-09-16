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
    SC_UNREACHABLE();
}

SC::ReturnCode SC::EventLoop::startLoopTimeout(AsyncLoopTimeout& async, IntegerMilliseconds expiration,
                                               Function<void(AsyncLoopTimeoutResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    updateTime();
    async.expirationTime = loopTime.offsetBy(expiration);
    async.timeout        = expiration;
    return true;
}

SC::ReturnCode SC::EventLoop::startFileRead(AsyncFileRead& async, FileDescriptor::Handle fileDescriptor,
                                            Span<char> readBuffer, Function<void(AsyncFileReadResult&)>&& callback)
{
    SC_TRY_MSG(readBuffer.sizeInBytes() > 0, "EventLoop::startFileRead - Zero sized read buffer"_a8);
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(queueSubmission(async));
    async.callback       = callback;
    async.fileDescriptor = fileDescriptor;
    async.readBuffer     = readBuffer;
    return true;
}

SC::ReturnCode SC::EventLoop::startFileWrite(AsyncFileWrite& async, FileDescriptor::Handle fileDescriptor,
                                             Span<const char>                        writeBuffer,
                                             Function<void(AsyncFileWriteResult&)>&& callback)
{
    SC_TRY_MSG(writeBuffer.sizeInBytes() > 0, "EventLoop::startFileWrite - Zero sized write buffer"_a8);
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(queueSubmission(async));
    async.callback       = callback;
    async.fileDescriptor = fileDescriptor;
    async.writeBuffer    = writeBuffer;
    return true;
}

SC::ReturnCode SC::EventLoop::startFileClose(AsyncFileClose& async, FileDescriptor::Handle fileDescriptor,
                                             Function<void(AsyncFileCloseResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(queueSubmission(async));
    async.callback       = callback;
    async.fileDescriptor = fileDescriptor;
    return true;
}

#if SC_PLATFORM_WINDOWS
SC::ReturnCode SC::EventLoop::startWindowsPoll(AsyncWindowsPoll& async, FileDescriptor::Handle fileDescriptor,
                                               Function<void(AsyncWindowsPollResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(queueSubmission(async));
    async.callback       = callback;
    async.fileDescriptor = fileDescriptor;
    return true;
}
#endif

SC::ReturnCode SC::EventLoop::startSocketAccept(AsyncSocketAccept& async, const SocketDescriptor& socketDescriptor,
                                                Function<void(AsyncSocketAcceptResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(socketDescriptor.getAddressFamily(async.addressFamily));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    return true;
}

SC::ReturnCode SC::EventLoop::startSocketConnect(AsyncSocketConnect& async, const SocketDescriptor& socketDescriptor,
                                                 SocketIPAddress                             ipAddress,
                                                 Function<void(AsyncSocketConnectResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(queueSubmission(async));
    async.callback  = callback;
    async.ipAddress = ipAddress;
    return true;
}

SC::ReturnCode SC::EventLoop::startSocketSend(AsyncSocketSend& async, const SocketDescriptor& socketDescriptor,
                                              Span<const char> data, Function<void(AsyncSocketSendResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    async.data     = data;
    return true;
}

SC::ReturnCode SC::EventLoop::startSocketReceive(AsyncSocketReceive& async, const SocketDescriptor& socketDescriptor,
                                                 Span<char> data, Function<void(AsyncSocketReceiveResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    async.data     = data;
    return true;
}

SC::ReturnCode SC::EventLoop::startSocketClose(AsyncSocketClose& async, const SocketDescriptor& socketDescriptor,
                                               Function<void(AsyncSocketCloseResult&)>&& callback)
{
    SC_TRY_IF(validateAsync(async));
    SC_TRY_IF(socketDescriptor.get(async.handle, "Invalid handle"_a8));
    SC_TRY_IF(queueSubmission(async));
    async.callback = callback;
    return true;
}

SC::ReturnCode SC::EventLoop::startLoopWakeUp(AsyncLoopWakeUp& async, Function<void(AsyncLoopWakeUpResult&)>&& callback,
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
        SC_DEBUG_ASSERT(async->getType() == Async::Type::LoopTimeout);
        const auto& expirationTime = async->asLoopTimeout()->expirationTime;
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
        SC_DEBUG_ASSERT(async->getType() == Async::Type::LoopTimeout);
        const auto& expirationTime = async->asLoopTimeout()->expirationTime;
        if (loopTime.isLaterThanOrEqualTo(expirationTime))
        {
            Async* currentAsync = async;
            async               = async->next;
            activeTimers.remove(*currentAsync);
            currentAsync->state          = Async::State::Free;
            ReturnCode               res = true;
            AsyncResult::LoopTimeout result(*currentAsync, move(res));
            result.async.eventLoop = nullptr; // Allow reusing it
            currentAsync->asLoopTimeout()->callback(result);
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

void SC::EventLoop::completeAndEventuallyReactivate(KernelQueue& queue, Async& async, ReturnCode&& returnCode)
{
    SC_RELEASE_ASSERT(async.state == Async::State::Active);
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
        SC_TRY_IF(queue.pollAsync(*this, pollMode));
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

SC::ReturnCode SC::EventLoop::setupAsync(KernelQueue& queue, Async& async)
{
    SC_LOG_MESSAGE("{} {} SETUP\n", async.debugName, Async::TypeToString(async.getType()));
    switch (async.getType())
    {
    case Async::Type::LoopTimeout:
        activeTimers.queueBack(async);
        numberOfTimers += 1;
        return true;
    case Async::Type::LoopWakeUp:
        activeWakeUps.queueBack(async);
        numberOfWakeups += 1;
        return true;
    case Async::Type::ProcessExit: SC_TRY_IF(queue.setupAsync(*async.asProcessExit())); break;
    case Async::Type::SocketAccept: SC_TRY_IF(queue.setupAsync(*async.asSocketAccept())); break;
    case Async::Type::SocketConnect: SC_TRY_IF(queue.setupAsync(*async.asSocketConnect())); break;
    case Async::Type::SocketSend: SC_TRY_IF(queue.setupAsync(*async.asSocketSend())); break;
    case Async::Type::SocketReceive: SC_TRY_IF(queue.setupAsync(*async.asSocketReceive())); break;
    case Async::Type::SocketClose: SC_TRY_IF(queue.setupAsync(*async.asSocketClose())); break;
    case Async::Type::FileRead: SC_TRY_IF(queue.setupAsync(*async.asFileRead())); break;
    case Async::Type::FileWrite: SC_TRY_IF(queue.setupAsync(*async.asFileWrite())); break;
    case Async::Type::FileClose: SC_TRY_IF(queue.setupAsync(*async.asFileClose())); break;
#if SC_PLATFORM_WINDOWS
    case Async::Type::WindowsPoll: SC_TRY_IF(queue.setupAsync(*async.asWindowsPoll())); break;
#endif
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
    case Async::Type::LoopTimeout: async.state = Async::State::Active; return true;
    case Async::Type::LoopWakeUp: async.state = Async::State::Active; return true;
    case Async::Type::SocketAccept: SC_TRY_IF(queue.activateAsync(*async.asSocketAccept())); break;
    case Async::Type::SocketConnect: SC_TRY_IF(queue.activateAsync(*async.asSocketConnect())); break;
    case Async::Type::SocketReceive: SC_TRY_IF(queue.activateAsync(*async.asSocketReceive())); break;
    case Async::Type::SocketSend: SC_TRY_IF(queue.activateAsync(*async.asSocketSend())); break;
    case Async::Type::SocketClose: SC_TRY_IF(queue.activateAsync(*async.asSocketClose())); break;
    case Async::Type::FileRead: SC_TRY_IF(queue.activateAsync(*async.asFileRead())); break;
    case Async::Type::FileWrite: SC_TRY_IF(queue.activateAsync(*async.asFileWrite())); break;
    case Async::Type::FileClose: SC_TRY_IF(queue.activateAsync(*async.asFileClose())); break;
    case Async::Type::ProcessExit: SC_TRY_IF(queue.activateAsync(*async.asProcessExit())); break;
#if SC_PLATFORM_WINDOWS
    case Async::Type::WindowsPoll: SC_TRY_IF(queue.activateAsync(*async.asWindowsPoll())); break;
#endif
    }
    if (async.state == Async::State::Submitting)
    {
        return queue.pushNewSubmission(async);
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
    completeAsync(queue, async, forward<ReturnCode>(returnCode), reactivate);
    async.state = Async::State::Free;
}

void SC::EventLoop::completeAsync(KernelQueue& queue, Async& async, ReturnCode&& returnCode, bool& reactivate)
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
    case Async::Type::LoopTimeout: {
        AsyncResult::LoopTimeout result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::FileRead: {
        AsyncResult::FileRead result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::FileWrite: {
        AsyncResult::FileWrite result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::FileClose: {
        AsyncResult::FileClose result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::LoopWakeUp: {
        AsyncResult::LoopWakeUp result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::ProcessExit: {
        AsyncResult::ProcessExit result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::SocketAccept: {
        AsyncResult::SocketAccept result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::SocketConnect: {
        AsyncResult::SocketConnect result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::SocketSend: {
        AsyncResult::SocketSend result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::SocketReceive: {
        AsyncResult::SocketReceive result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
    case Async::Type::SocketClose: {
        AsyncResult::SocketClose result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
#if SC_PLATFORM_WINDOWS
    case Async::Type::WindowsPoll: {
        AsyncResult::WindowsPoll result(async, forward<ReturnCode>(returnCode));
        if (result.returnCode)
            result.returnCode = queue.completeAsync(result);
        if (result.async.callback.isValid())
            result.async.callback(result);
        reactivate = result.shouldBeReactivated;
        break;
    }
#endif
    }
}

SC::ReturnCode SC::EventLoop::cancelAsync(KernelQueue& queue, Async& async)
{
    SC_LOG_MESSAGE("{} {} CANCEL\n", async.debugName, Async::TypeToString(async.getType()));
    switch (async.getType())
    {
    case Async::Type::LoopTimeout: numberOfTimers -= 1; return true;
    case Async::Type::LoopWakeUp: numberOfWakeups -= 1; return true;
    case Async::Type::FileRead: SC_TRY_IF(queue.stopAsync(*async.asFileRead())); break;
    case Async::Type::FileWrite: SC_TRY_IF(queue.stopAsync(*async.asFileWrite())); break;
    case Async::Type::FileClose: SC_TRY_IF(queue.stopAsync(*async.asFileClose())); break;
    case Async::Type::ProcessExit: SC_TRY_IF(queue.stopAsync(*async.asProcessExit())); break;
    case Async::Type::SocketAccept: SC_TRY_IF(queue.stopAsync(*async.asSocketAccept())); break;
    case Async::Type::SocketConnect: SC_TRY_IF(queue.stopAsync(*async.asSocketConnect())); break;
    case Async::Type::SocketSend: SC_TRY_IF(queue.stopAsync(*async.asSocketSend())); break;
    case Async::Type::SocketReceive: SC_TRY_IF(queue.stopAsync(*async.asSocketReceive())); break;
    case Async::Type::SocketClose: SC_TRY_IF(queue.stopAsync(*async.asSocketClose())); break;
#if SC_PLATFORM_WINDOWS
    case Async::Type::WindowsPoll: SC_TRY_IF(queue.stopAsync(*async.asWindowsPoll())); break;
#endif
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
        if (async.getType() == Async::Type::LoopTimeout)
        {
            activeTimers.remove(async);
        }
        else if (async.getType() == Async::Type::LoopWakeUp)
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

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread(AsyncLoopWakeUp& wakeUp)
{
    SC_TRY_MSG(wakeUp.eventLoop == this,
               "EventLoop::wakeUpFromExternalThread - Wakeup belonging to different EventLoop"_a8);
    if (not wakeUp.asLoopWakeUp()->pending.exchange(true))
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
        Async::LoopWakeUp* notifier = async->asLoopWakeUp();
        if (notifier->pending.load() == true)
        {
            ReturnCode              res = true;
            AsyncResult::LoopWakeUp asyncResult(*async, move(res));
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
    SC_RELEASE_ASSERT(async.state == Async::State::Active)
    async.state = Async::State::Free;
    async.eventLoop->numberOfActiveHandles -= 1;
}

void SC::EventLoop::addActiveHandle(Async& async)
{
    SC_RELEASE_ASSERT(async.state == Async::State::Submitting);
    async.state = Async::State::Active;
    async.eventLoop->numberOfActiveHandles += 1;
}

void SC::EventLoop::scheduleManualCompletion(Async& async)
{
    SC_RELEASE_ASSERT(async.state == Async::State::Submitting);
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
    SC_TRY_IF(res);
    return associateExternallyCreatedTCPSocket(outDescriptor);
}

SC::ReturnCode SC::AsyncLoopWakeUp::wakeUp() { return eventLoop->wakeUpFromExternalThread(*this); }
