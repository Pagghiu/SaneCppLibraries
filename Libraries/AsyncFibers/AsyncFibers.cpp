// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncFibers.h"

#define SC_ASSERT_PROVIDER AsyncFibersAssert
#include "../Common/Assert.inl"

#include "../Threading/Threading.h"

namespace SC
{
namespace
{
static constexpr Result AsyncFiberTaskCancelled()
{
    return Result::Error(AsyncFibersResultCategory, AsyncFibersError::Cancelled);
}

struct AsyncFiberStartState
{
    AsyncFiberIO*                     asyncFiber      = nullptr;
    FiberCounter*                     counter         = nullptr;
    Result*                           operationResult = nullptr;
    Function<Result(AsyncEventLoop&)> startProcedure;
    FiberCounter                      startCounter;
    Atomic<int32_t>                   cancelBeforeStart = 0;
    Atomic<int32_t>                   requestStarted    = 0;
};

struct AsyncFiberStopState
{
    AsyncFiberIO* asyncFiber = nullptr;
    FiberCounter* counter    = nullptr;
    AsyncRequest* request    = nullptr;

    Function<void(AsyncResult&)>* stopCallback = nullptr;

    // Both the owner command and the waiting fiber access this only while counter is non-zero.
    Result stopResult = Result(true);
};

struct AsyncFiberSendAllState
{
    AsyncFiberIO*               asyncFiber = nullptr;
    FiberCounter*               counter    = nullptr;
    AsyncSocketSend*            request    = nullptr;
    Span<const char>            data;
    AsyncFiberSocketSendResult* outResult    = nullptr;
    Result                      result       = Result(true);
    size_t                      numBytesSent = 0;
};

} // namespace

AsyncFiberIO::AsyncFiberIO(FiberScheduler& fiberScheduler, AsyncEventLoop& asyncEventLoop,
                           Span<AsyncFiberCommand> commandStorage)
    : scheduler(fiberScheduler), eventLoop(asyncEventLoop), commands(commandStorage),
      ownerThreadID(Thread::CurrentThreadID())
{}

AsyncFiberIO::~AsyncFiberIO()
{
    SC_ASYNC_FIBERS_ASSERT_RELEASE(pendingOperations.load() == 0);
    SC_ASYNC_FIBERS_ASSERT_RELEASE(not hasPendingCommands());
}

FiberScheduler& AsyncFiberIO::fiberScheduler() { return scheduler; }

const FiberScheduler& AsyncFiberIO::fiberScheduler() const { return scheduler; }

AsyncEventLoop& AsyncFiberIO::asyncEventLoop() { return eventLoop; }

const AsyncEventLoop& AsyncFiberIO::asyncEventLoop() const { return eventLoop; }

bool AsyncFiberIO::isOwnerThread() const { return Thread::CurrentThreadID() == ownerThreadID; }

Result AsyncFiberIO::run()
{
    SC_TRY(checkOwnerThread());
    while (scheduler.hasActiveFibers() or pendingOperations.load() != 0 or hasPendingCommands())
    {
        SC_TRY(runOnce());
    }
    return Result(true);
}

Result AsyncFiberIO::runOnce()
{
    SC_TRY(checkOwnerThread());
    SC_TRY(drainCommandQueue());
    if (scheduler.hasReadyFibers())
    {
        SC_TRY(scheduler.runNoWait());
        if (pendingOperations.load() != 0 or hasPendingCommands())
        {
            SC_TRY(eventLoop.runNoWait());
            SC_TRY(drainCommandQueue());
        }
        return Result(true);
    }
    if (pendingOperations.load() != 0 or hasPendingCommands())
    {
        SC_TRY(eventLoop.runOnce());
        SC_TRY(drainCommandQueue());
        if (scheduler.hasReadyFibers())
        {
            return scheduler.runNoWait();
        }
        return Result(true);
    }
    if (scheduler.hasActiveFibers())
    {
        return scheduler.runOnce();
    }
    return eventLoop.runNoWait();
}

Result AsyncFiberIO::runNoWait()
{
    SC_TRY(checkOwnerThread());
    SC_TRY(drainCommandQueue());
    SC_TRY(scheduler.runReadyFibers());
    SC_TRY(eventLoop.runNoWait());
    SC_TRY(drainCommandQueue());
    SC_TRY(scheduler.runReadyFibers());
    return Result(true);
}

Result AsyncFiberIO::runUntilComplete() { return run(); }

Result AsyncFiberIO::runUntilIdle() { return runNoWait(); }

Result AsyncFiberIO::runOwner()
{
    SC_TRY(checkOwnerThread());
    while (scheduler.hasActiveFibers() or pendingOperations.load() != 0 or hasPendingCommands())
    {
        SC_TRY(runOwnerOnce());
    }
    return Result(true);
}

Result AsyncFiberIO::runOwnerOnce()
{
    SC_TRY(checkOwnerThread());
    SC_TRY(drainCommandQueue());
    if (pendingOperations.load() != 0 or hasPendingCommands())
    {
        SC_TRY(eventLoop.runOnce());
        SC_TRY(drainCommandQueue());
        return Result(true);
    }
    return eventLoop.runNoWait();
}

Result AsyncFiberIO::runOwnerNoWait()
{
    SC_TRY(checkOwnerThread());
    SC_TRY(drainCommandQueue());
    SC_TRY(eventLoop.runNoWait());
    SC_TRY(drainCommandQueue());
    return Result(true);
}

Result AsyncFiberIO::runOwnerUntilComplete() { return runOwner(); }

Result AsyncFiberIO::runOwnerUntilIdle() { return runOwnerNoWait(); }

Result AsyncFiberIO::cancelAll()
{
    SC_TRY(checkOwnerThread());
    return scheduler.requestCancelAll();
}

template <typename Request, typename Start, typename Complete>
Result AsyncFiberIO::runSingleOperation(Request& request, Start start, Complete complete)
{
    FiberCounter counter;
    Result       operationResult = Result(true);
    struct CompletionContext
    {
        FiberCounter* counter;
        Result*       result;
        Complete*     complete;
    } context{&counter, &operationResult, &complete};

    request.callback = [this, &context](typename Request::Result& result)
    {
        *context.result = (*context.complete)(result);
        operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(*context.counter));
    };
    // The start command copies only these pointers; both callables live until startOperation returns.
    Function<Result(AsyncEventLoop&)> startProcedure = [&request, &start](AsyncEventLoop& eventLoop)
    { return start(request, eventLoop); };
    return startOperation(counter, request, operationResult, startProcedure);
}

Result AsyncFiberIO::sleep(TimeMs duration)
{
    AsyncLoopTimeout request;

    return runSingleOperation(
        request, [duration](AsyncLoopTimeout& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, duration); },
        [](AsyncLoopTimeout::Result& result) { return result.isValid(); });
}

Result AsyncFiberIO::accept(const SocketDescriptor& serverSocket, SocketDescriptor& outClient)
{
    AsyncSocketAccept request;

    return runSingleOperation(
        request, [&serverSocket](AsyncSocketAccept& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, serverSocket); },
        [&outClient](AsyncSocketAccept::Result& result) { return result.moveTo(outClient); });
}

Result AsyncFiberIO::connect(const SocketDescriptor& socket, SocketIPAddress address)
{
    AsyncSocketConnect request;

    return runSingleOperation(
        request, [&socket, address](AsyncSocketConnect& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, socket, address); },
        [](AsyncSocketConnect::Result& result) { return result.isValid(); });
}

Result AsyncFiberIO::send(const SocketDescriptor& socket, Span<const char> data, AsyncFiberSocketSendResult* outResult)
{
    SC_TRY(checkFiberContext());
    if (data.empty())
    {
        if (outResult != nullptr)
        {
            outResult->numBytes = 0;
        }
        return Result(true);
    }

    AsyncSocketSend request;

    if (outResult != nullptr)
    {
        outResult->numBytes = 0;
    }

    return runSingleOperation(
        request, [&socket, data](AsyncSocketSend& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, socket, data); },
        [outResult](AsyncSocketSend::Result& result)
        {
            SC::Result operationResult = result.isValid();
            if (outResult != nullptr and operationResult)
            {
                outResult->numBytes = result.completionData.numBytes;
            }
            return operationResult;
        });
}

Result AsyncFiberIO::receive(const SocketDescriptor& socket, Span<char> buffer,
                             AsyncFiberSocketReceiveResult& outResult)
{
    AsyncSocketReceive request;

    outResult = {};

    return runSingleOperation(
        request, [&socket, buffer](AsyncSocketReceive& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, socket, buffer); },
        [&outResult](AsyncSocketReceive::Result& result)
        {
            SC::Result operationResult = result.get(outResult.data);
            outResult.disconnected     = result.completionData.disconnected;
            return operationResult;
        });
}

Result AsyncFiberIO::sendAll(const SocketDescriptor& socket, Span<const char> data,
                             AsyncFiberSocketSendResult* outResult)
{
    SC_TRY(checkFiberContext());
    if (data.empty())
    {
        if (outResult != nullptr)
        {
            outResult->numBytes = 0;
        }
        return Result(true);
    }

    FiberCounter           counter;
    AsyncFiberSendAllState state;
    AsyncSocketSend        request;

    state.asyncFiber = this;
    state.counter    = &counter;
    state.request    = &request;
    state.data       = data;
    state.outResult  = outResult;
    if (outResult != nullptr)
    {
        outResult->numBytes = 0;
    }

    request.callback = [&state](AsyncSocketSend::Result& result)
    {
        state.result = result.isValid();
        if (state.result)
        {
            const size_t bytesSent = result.completionData.numBytes;
            if (bytesSent == 0)
            {
                state.result = Result::Error(AsyncFibersResultCategory, AsyncFibersError::SocketSendNoProgress);
            }
            else
            {
                state.numBytesSent += bytesSent;
                if (state.outResult != nullptr)
                {
                    state.outResult->numBytes = state.numBytesSent;
                }
                if (state.numBytesSent < state.data.sizeInBytes())
                {
                    Span<const char> remaining;
                    state.result = Result(state.data.sliceStart(state.numBytesSent, remaining));
                    if (state.result)
                    {
                        state.request->buffer = remaining;
                        result.reactivateRequest(true);
                        return;
                    }
                }
            }
        }

        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncSocketSend*        request = nullptr;
        const SocketDescriptor* socket  = nullptr;
        Span<const char>        data;
    };
    StartContext                      startContext{&request, &socket, data};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, *startContext.socket, startContext.data); };
    return startOperation(counter, request, state.result, startProcedure);
}

Result AsyncFiberIO::fileRead(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult)
{
    return fileReadImpl(file, buffer, outResult, 0, false);
}

Result AsyncFiberIO::fileReadAt(const FileDescriptor& file, uint64_t offset, Span<char> buffer,
                                AsyncFiberFileReadResult& outResult)
{
    return fileReadImpl(file, buffer, outResult, offset, true);
}

Result AsyncFiberIO::fileReadExact(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult)
{
    return fileReadExactImpl(file, buffer, outResult, 0, false);
}

Result AsyncFiberIO::fileReadExactAt(const FileDescriptor& file, uint64_t offset, Span<char> buffer,
                                     AsyncFiberFileReadResult& outResult)
{
    return fileReadExactImpl(file, buffer, outResult, offset, true);
}

Result AsyncFiberIO::filePoll(const FileDescriptor& file)
{
    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TRY(file.get(handle, Result::Error(AsyncFibersResultCategory, AsyncFibersError::InvalidFileHandle)));

    AsyncFileReadiness request;

    return runSingleOperation(
        request, [handle](AsyncFileReadiness& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, handle); },
        [](AsyncFileReadiness::Result& result) { return result.isValid(); });
}

Result AsyncFiberIO::fileWrite(const FileDescriptor& file, Span<const char> data, AsyncFiberFileWriteResult* outResult)
{
    return fileWriteImpl(file, data, outResult, 0, false);
}

Result AsyncFiberIO::fileWriteAt(const FileDescriptor& file, uint64_t offset, Span<const char> data,
                                 AsyncFiberFileWriteResult* outResult)
{
    return fileWriteImpl(file, data, outResult, offset, true);
}

Result AsyncFiberIO::fileWriteAll(const FileDescriptor& file, Span<const char> data,
                                  AsyncFiberFileWriteResult* outResult)
{
    return fileWriteImpl(file, data, outResult, 0, false);
}

Result AsyncFiberIO::fileWriteAllAt(const FileDescriptor& file, uint64_t offset, Span<const char> data,
                                    AsyncFiberFileWriteResult* outResult)
{
    return fileWriteImpl(file, data, outResult, offset, true);
}

Result AsyncFiberIO::fileSend(const FileDescriptor& file, const SocketDescriptor& socket,
                              AsyncFiberFileSendOptions options, AsyncFiberFileSendResult* outResult)
{
    AsyncFileSend request;

    if (outResult != nullptr)
    {
        *outResult = {};
    }

    return runSingleOperation(
        request, [&file, &socket, options](AsyncFileSend& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, file, socket, options.offset, options.length, options.pipeSize); },
        [outResult](AsyncFileSend::Result& result)
        {
            SC::Result operationResult = result.isValid();
            if (outResult != nullptr and operationResult)
            {
                outResult->bytesTransferred = result.getBytesTransferred();
                outResult->usedZeroCopy     = result.usedZeroCopy();
            }
            return operationResult;
        });
}

Result AsyncFiberIO::processExit(FileDescriptor::Handle process, AsyncFiberProcessExitResult& outResult)
{
    AsyncProcessExit request;

    outResult = {};

    return runSingleOperation(
        request, [process](AsyncProcessExit& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, process); },
        [&outResult](AsyncProcessExit::Result& result) { return result.get(outResult.exitStatus); });
}

Result AsyncFiberIO::signal(int signalNumber, AsyncFiberSignalResult& outResult)
{
    AsyncSignal request;

    outResult = {};

    return runSingleOperation(
        request,
        [signalNumber](AsyncSignal& operation, AsyncEventLoop& eventLoop)
        {
            AsyncSignalOptions options;
            options.mode = AsyncSignalOptions::Mode::OneShot;
            return operation.start(eventLoop, signalNumber, options);
        },
        [&outResult](AsyncSignal::Result& result)
        {
            SC::Result operationResult = result.isValid();
            outResult.signalNumber     = result.completionData.signalNumber;
            outResult.deliveryCount    = result.completionData.deliveryCount;
            return operationResult;
        });
}

Result AsyncFiberIO::sendTo(const SocketDescriptor& socket, SocketIPAddress address, Span<const char> data,
                            AsyncFiberSocketSendResult* outResult)
{
    SC_TRY(checkFiberContext());
    if (data.empty())
    {
        if (outResult != nullptr)
        {
            outResult->numBytes = 0;
        }
        return Result(true);
    }

    AsyncSocketSendTo request;

    if (outResult != nullptr)
    {
        outResult->numBytes = 0;
    }

    return runSingleOperation(
        request, [&socket, address, data](AsyncSocketSendTo& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, socket, address, data); },
        [outResult](AsyncSocketSendTo::Result& result)
        {
            SC::Result operationResult = result.isValid();
            if (outResult != nullptr and operationResult)
            {
                outResult->numBytes = result.completionData.numBytes;
            }
            return operationResult;
        });
}

Result AsyncFiberIO::receiveFrom(const SocketDescriptor& socket, Span<char> buffer,
                                 AsyncFiberSocketReceiveFromResult& outResult)
{
    AsyncSocketReceiveFrom request;

    outResult = {};

    return runSingleOperation(
        request, [&socket, buffer](AsyncSocketReceiveFrom& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, socket, buffer); },
        [&outResult](AsyncSocketReceiveFrom::Result& result)
        {
            SC::Result operationResult = result.get(outResult.data);
            outResult.sourceAddress    = result.getSourceAddress();
            return operationResult;
        });
}

Result AsyncFiberIO::fileReadImpl(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult,
                                  uint64_t offset, bool useOffset)
{
    AsyncFileRead request;
    return fileReadImpl(file, buffer, outResult, offset, useOffset, request);
}

Result AsyncFiberIO::fileReadImpl(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult,
                                  uint64_t offset, bool useOffset, AsyncFileRead& request)
{
    outResult = {};

    if (useOffset)
    {
        request.setOffset(offset);
    }

    return runSingleOperation(
        request, [&file, buffer](AsyncFileRead& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, file, buffer); },
        [&outResult](AsyncFileRead::Result& result)
        {
            SC::Result operationResult = result.get(outResult.data);
            outResult.endOfFile        = result.completionData.endOfFile;
            return operationResult;
        });
}

Result AsyncFiberIO::fileReadExactImpl(const FileDescriptor& file, Span<char> buffer,
                                       AsyncFiberFileReadResult& outResult, uint64_t offset, bool useOffset)
{
    SC_TRY(checkFiberContext());
    outResult = {};
    if (buffer.empty())
    {
        outResult.data = buffer;
        return Result(true);
    }

    AsyncFileRead request;
    size_t        numBytesRead = 0;
    while (numBytesRead < buffer.sizeInBytes())
    {
        Span<char> remaining;
        SC_TRY(buffer.sliceStart(numBytesRead, remaining));

        AsyncFiberFileReadResult readResult;
        SC_TRY(fileReadImpl(file, remaining, readResult, offset + numBytesRead, useOffset, request));

        const size_t currentBytesRead = readResult.data.sizeInBytes();
        outResult.endOfFile           = readResult.endOfFile;
        if (currentBytesRead == 0)
        {
            SC_TRY(buffer.sliceStartLength(0, numBytesRead, outResult.data));
            return Result::Error(AsyncFibersResultCategory, AsyncFibersError::UnexpectedEndOfFile);
        }

        numBytesRead += currentBytesRead;
    }

    SC_TRY(buffer.sliceStartLength(0, numBytesRead, outResult.data));
    return Result(true);
}

Result AsyncFiberIO::fileWriteImpl(const FileDescriptor& file, Span<const char> data,
                                   AsyncFiberFileWriteResult* outResult, uint64_t offset, bool useOffset)
{
    SC_TRY(checkFiberContext());
    if (data.empty())
    {
        if (outResult != nullptr)
        {
            outResult->numBytes = 0;
        }
        return Result(true);
    }

    AsyncFileWrite request;

    if (outResult != nullptr)
    {
        outResult->numBytes = 0;
    }

    if (useOffset)
    {
        request.setOffset(offset);
    }

    return runSingleOperation(
        request, [&file, data](AsyncFileWrite& operation, AsyncEventLoop& eventLoop)
        { return operation.start(eventLoop, file, data); },
        [outResult](AsyncFileWrite::Result& result)
        {
            SC::Result operationResult = result.isValid();
            if (outResult != nullptr and operationResult)
            {
                operationResult = result.get(outResult->numBytes);
            }
            return operationResult;
        });
}

Result AsyncFiberIO::checkOwnerThread() const
{
    SC_TRY(scheduler.checkExecutionThread());
    SC_ASYNC_FIBERS_ASSERT_RELEASE(isOwnerThread());
    if (not isOwnerThread())
        return Result::Error(AsyncFibersResultCategory, AsyncFibersError::WrongOwnerThread);
    return Result(true);
}

Result AsyncFiberIO::checkFiberContext() const
{
    return scheduler.currentTask() != nullptr
               ? Result(true)
               : Result::Error(AsyncFibersResultCategory, AsyncFibersError::FiberContextRequired);
}

void AsyncFiberIO::operationStarted() { pendingOperations.fetch_add(1); }

void AsyncFiberIO::operationFinished()
{
    SC_ASYNC_FIBERS_ASSERT_RELEASE(pendingOperations.load() > 0);
    pendingOperations.fetch_sub(1);
}

void AsyncFiberIO::lockCommands() const
{
    int32_t expected = 0;
    while (not commandLock.compare_exchange_weak(expected, 1))
    {
        expected = 0;
    }
}

void AsyncFiberIO::unlockCommands() const { commandLock.store(0); }

Result AsyncFiberIO::enqueueCommand(AsyncFiberCommand& command)
{
    if (commands.empty())
    {
        return Result::Error(AsyncFibersResultCategory, AsyncFibersError::CommandStorageEmpty);
    }
    if (not command.execute.isValid())
    {
        return Result::Error(AsyncFibersResultCategory, AsyncFibersError::InvalidCommand);
    }

    lockCommands();
    if (commandCount == commands.sizeInElements())
    {
        unlockCommands();
        return Result::Error(AsyncFibersResultCategory, AsyncFibersError::CommandQueueFull);
    }

    const size_t index = (commandHead + commandCount) % commands.sizeInElements();
    commands[index]    = command;
    commandCount += 1;
    unlockCommands();

    if (not isOwnerThread())
    {
        // The command is already visible to the owner. Returning a wake-up failure here would let the producer unwind
        // stack state still referenced by the queued command; the owner also drains commands before polling.
        (void)eventLoop.wakeUpFromExternalThread();
    }
    return Result(true);
}

bool AsyncFiberIO::hasPendingCommands() const
{
    lockCommands();
    const bool hasCommands = commandCount != 0;
    unlockCommands();
    return hasCommands;
}

Result AsyncFiberIO::drainCommandQueue()
{
    SC_TRY(checkOwnerThread());

    for (;;)
    {
        AsyncFiberCommand command;
        lockCommands();
        if (commandCount == 0)
        {
            unlockCommands();
            return Result(true);
        }
        command               = commands[commandHead];
        commands[commandHead] = AsyncFiberCommand();
        commandHead           = (commandHead + 1) % commands.sizeInElements();
        commandCount -= 1;
        unlockCommands();

        SC_TRY(command.execute());
    }
}

Result AsyncFiberIO::startOperation(FiberCounter& counter, AsyncRequest& request, Result& operationResult,
                                    Function<Result(AsyncEventLoop&)>& startProcedure)
{
    SC_TRY(checkFiberContext());
    if (scheduler.isCurrentTaskCancellationRequested())
    {
        operationResult = AsyncFiberTaskCancelled();
        return operationResult;
    }

    scheduler.add(counter);
    operationStarted();

    if (isOwnerThread())
    {
        Result startResult = startProcedure(eventLoop);
        if (not startResult)
        {
            operationFinished();
            SC_TRY(scheduler.done(counter));
            return startResult;
        }
        return waitForOperation(counter, request, operationResult);
    }

    AsyncFiberStartState startState;
    startState.asyncFiber      = this;
    startState.counter         = &counter;
    startState.operationResult = &operationResult;
    startState.startProcedure  = startProcedure;
    scheduler.add(startState.startCounter);

    AsyncFiberCommand command;
    command.execute = AsyncFiberCommand::Procedure([this, &startState]() { return executeStartCommand(&startState); });

    Result enqueueResult = enqueueCommand(command);
    if (not enqueueResult)
    {
        SC_TRY(scheduler.done(startState.startCounter));
        operationFinished();
        SC_TRY(scheduler.done(counter));
        return enqueueResult;
    }
    return waitForOperation(counter, request, operationResult, &startState);
}

Result AsyncFiberIO::executeStartCommand(void* startStatePointer)
{
    AsyncFiberStartState& startState = *static_cast<AsyncFiberStartState*>(startStatePointer);
    if (startState.cancelBeforeStart.load() != 0)
    {
        *startState.operationResult = AsyncFiberTaskCancelled();
        operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(startState.startCounter));
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(*startState.counter));
        return Result(true);
    }

    Result startResult = startState.startProcedure(eventLoop);
    if (not startResult)
    {
        *startState.operationResult = startResult;
        operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(startState.startCounter));
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(*startState.counter));
    }
    else
    {
        startState.requestStarted.store(1);
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(startState.startCounter));
    }
    return Result(true);
}

Result AsyncFiberIO::waitForOperation(FiberCounter& counter, AsyncRequest& request, Result& operationResult,
                                      void* startStatePointer)
{
    AsyncFiberStartState* startState = static_cast<AsyncFiberStartState*>(startStatePointer);
    if (scheduler.isCurrentTaskCancellationRequested())
    {
        if (startState != nullptr)
        {
            startState->cancelBeforeStart.store(1);
            SC_TRY(scheduler.waitUninterruptible(startState->startCounter));
            if (startState->requestStarted.load() == 0)
            {
                SC_TRY(scheduler.waitUninterruptible(counter));
                return operationResult;
            }
        }
        SC_TRY(stopOperation(counter, request));
        operationResult = AsyncFiberTaskCancelled();
        return operationResult;
    }

    Result waitResult = scheduler.wait(counter);
    if (waitResult)
    {
        return operationResult;
    }
    // An interruptible counter wait fails only when this fiber has been cancelled. Re-checking the current worker's
    // task is racy with worker migration, so use the wait result itself as the cancellation decision.
    if (startState != nullptr)
    {
        startState->cancelBeforeStart.store(1);
        SC_TRY(scheduler.waitUninterruptible(startState->startCounter));
        if (startState->requestStarted.load() == 0)
        {
            SC_TRY(scheduler.waitUninterruptible(counter));
            return operationResult;
        }
    }
    SC_TRY(stopOperation(counter, request));
    // Cancellation wins after the request and its completion callback have quiesced.
    operationResult = AsyncFiberTaskCancelled();
    return operationResult;
}

Result AsyncFiberIO::stopOperation(FiberCounter& operationCounter, AsyncRequest& request)
{
    if (isOwnerThread() and request.isFree())
    {
        return Result(true);
    }

    FiberCounter        counter;
    AsyncFiberStopState state;

    state.asyncFiber = this;
    state.counter    = &counter;
    state.request    = &request;

    scheduler.add(counter);

    Function<void(AsyncResult&)> stopCallback = [&state](AsyncResult&)
    {
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };
    state.stopCallback = &stopCallback;

    if (isOwnerThread())
    {
        state.stopResult = request.stop(eventLoop, &stopCallback);
        if (not state.stopResult)
        {
            SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(counter));
            return state.stopResult;
        }
    }
    else
    {
        AsyncFiberCommand command;
        command.execute = AsyncFiberCommand::Procedure([this, &state]() { return executeStopCommand(&state); });
        // Keep enqueue errors separate: the owner command exclusively writes state.stopResult.
        const Result enqueueResult = enqueueCommand(command);
        if (not enqueueResult)
        {
            SC_TRY(scheduler.waitUninterruptible(operationCounter));
            return enqueueResult;
        }
    }
    SC_TRY(scheduler.waitUninterruptible(counter));
    return state.stopResult;
}

Result AsyncFiberIO::executeStopCommand(void* stopStatePointer)
{
    AsyncFiberStopState& stopState = *static_cast<AsyncFiberStopState*>(stopStatePointer);
    if (stopState.request->isFree())
    {
        // Completion won the owner-thread race before this stop command was drained.
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(*stopState.counter));
        return Result(true);
    }

    stopState.stopResult = stopState.request->stop(eventLoop, stopState.stopCallback);
    if (not stopState.stopResult)
    {
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(*stopState.counter));
    }
    return Result(true);
}
} // namespace SC
