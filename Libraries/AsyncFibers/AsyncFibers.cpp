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
static constexpr Result AsyncFiberTaskCancelled() { return Result::Error("FiberTask cancelled"); }

struct AsyncFiberOperationState
{
    AsyncFiberIO* asyncFiber = nullptr;
    FiberCounter* counter    = nullptr;
    Result        result     = Result(true);
};

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
    AsyncFiberIO*                 asyncFiber   = nullptr;
    FiberCounter*                 counter      = nullptr;
    AsyncRequest*                 request      = nullptr;
    Function<void(AsyncResult&)>* stopCallback = nullptr;
    Result*                       stopResult   = nullptr;
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

Result AsyncFiberIO::sleep(TimeMs duration)
{
    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncLoopTimeout         request;

    state.asyncFiber = this;
    state.counter    = &counter;

    request.callback = [&state](AsyncLoopTimeout::Result& result)
    {
        state.result = result.isValid();
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncLoopTimeout* request = nullptr;
        TimeMs            duration;
    };
    StartContext                      startContext{&request, duration};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, startContext.duration); };
    return startOperation(counter, request, state.result, startProcedure);
}

Result AsyncFiberIO::accept(const SocketDescriptor& serverSocket, SocketDescriptor& outClient)
{
    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncSocketAccept        request;

    state.asyncFiber = this;
    state.counter    = &counter;

    request.callback = [&state, &outClient](AsyncSocketAccept::Result& result)
    {
        state.result = result.moveTo(outClient);
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncSocketAccept*      request = nullptr;
        const SocketDescriptor* socket  = nullptr;
    };
    StartContext                      startContext{&request, &serverSocket};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, *startContext.socket); };
    return startOperation(counter, request, state.result, startProcedure);
}

Result AsyncFiberIO::connect(const SocketDescriptor& socket, SocketIPAddress address)
{
    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncSocketConnect       request;

    state.asyncFiber = this;
    state.counter    = &counter;

    request.callback = [&state](AsyncSocketConnect::Result& result)
    {
        state.result = result.isValid();
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncSocketConnect*     request = nullptr;
        const SocketDescriptor* socket  = nullptr;
        SocketIPAddress         address;
    };
    StartContext                      startContext{&request, &socket, address};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, *startContext.socket, startContext.address); };
    return startOperation(counter, request, state.result, startProcedure);
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

    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncSocketSend          request;

    state.asyncFiber = this;
    state.counter    = &counter;
    if (outResult != nullptr)
    {
        outResult->numBytes = 0;
    }

    request.callback = [&state, outResult](AsyncSocketSend::Result& result)
    {
        state.result = result.isValid();
        if (outResult != nullptr and state.result)
        {
            outResult->numBytes = result.completionData.numBytes;
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

Result AsyncFiberIO::receive(const SocketDescriptor& socket, Span<char> buffer,
                             AsyncFiberSocketReceiveResult& outResult)
{
    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncSocketReceive       request;

    state.asyncFiber = this;
    state.counter    = &counter;
    outResult        = {};

    request.callback = [&state, &outResult](AsyncSocketReceive::Result& result)
    {
        state.result           = result.get(outResult.data);
        outResult.disconnected = result.completionData.disconnected;
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncSocketReceive*     request = nullptr;
        const SocketDescriptor* socket  = nullptr;
        Span<char>              buffer;
    };
    StartContext                      startContext{&request, &socket, buffer};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, *startContext.socket, startContext.buffer); };
    return startOperation(counter, request, state.result, startProcedure);
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
                state.result = Result::Error("AsyncFiberIO::sendAll made no progress");
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
    SC_TRY(file.get(handle, Result::Error("AsyncFiberIO::filePoll invalid file")));

    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncFileReadiness       request;

    state.asyncFiber = this;
    state.counter    = &counter;

    request.callback = [&state](AsyncFileReadiness::Result& result)
    {
        state.result = result.isValid();
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncFileReadiness*    request = nullptr;
        FileDescriptor::Handle handle  = FileDescriptor::Invalid;
    };
    StartContext                      startContext{&request, handle};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, startContext.handle); };
    return startOperation(counter, request, state.result, startProcedure);
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
    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncFileSend            request;

    state.asyncFiber = this;
    state.counter    = &counter;
    if (outResult != nullptr)
    {
        *outResult = {};
    }

    request.callback = [&state, outResult](AsyncFileSend::Result& result)
    {
        state.result = result.isValid();
        if (outResult != nullptr and state.result)
        {
            outResult->bytesTransferred = result.getBytesTransferred();
            outResult->usedZeroCopy     = result.usedZeroCopy();
        }
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncFileSend*            request = nullptr;
        const FileDescriptor*     file    = nullptr;
        const SocketDescriptor*   socket  = nullptr;
        AsyncFiberFileSendOptions options;
    };
    StartContext startContext;
    startContext.request = &request;
    startContext.file    = &file;
    startContext.socket  = &socket;
    startContext.options = options;

    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    {
        return startContext.request->start(eventLoop, *startContext.file, *startContext.socket,
                                           startContext.options.offset, startContext.options.length,
                                           startContext.options.pipeSize);
    };
    return startOperation(counter, request, state.result, startProcedure);
}

Result AsyncFiberIO::processExit(FileDescriptor::Handle process, AsyncFiberProcessExitResult& outResult)
{
    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncProcessExit         request;

    state.asyncFiber = this;
    state.counter    = &counter;
    outResult        = {};

    request.callback = [&state, &outResult](AsyncProcessExit::Result& result)
    {
        state.result = result.get(outResult.exitStatus);
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncProcessExit*      request = nullptr;
        FileDescriptor::Handle process = FileDescriptor::Invalid;
    };
    StartContext                      startContext{&request, process};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, startContext.process); };
    return startOperation(counter, request, state.result, startProcedure);
}

Result AsyncFiberIO::signal(int signalNumber, AsyncFiberSignalResult& outResult)
{
    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncSignal              request;

    state.asyncFiber = this;
    state.counter    = &counter;
    outResult        = {};

    request.callback = [&state, &outResult](AsyncSignal::Result& result)
    {
        state.result            = result.isValid();
        outResult.signalNumber  = result.completionData.signalNumber;
        outResult.deliveryCount = result.completionData.deliveryCount;
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncSignal*       request = nullptr;
        int                signal  = 0;
        AsyncSignalOptions options;
    };
    StartContext startContext;
    startContext.request      = &request;
    startContext.signal       = signalNumber;
    startContext.options.mode = AsyncSignalOptions::Mode::OneShot;

    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, startContext.signal, startContext.options); };
    return startOperation(counter, request, state.result, startProcedure);
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

    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncSocketSendTo        request;

    state.asyncFiber = this;
    state.counter    = &counter;
    if (outResult != nullptr)
    {
        outResult->numBytes = 0;
    }

    request.callback = [&state, outResult](AsyncSocketSendTo::Result& result)
    {
        state.result = result.isValid();
        if (outResult != nullptr and state.result)
        {
            outResult->numBytes = result.completionData.numBytes;
        }
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncSocketSendTo*      request = nullptr;
        const SocketDescriptor* socket  = nullptr;
        SocketIPAddress         address;
        Span<const char>        data;
    };
    StartContext                      startContext{&request, &socket, address, data};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, *startContext.socket, startContext.address, startContext.data); };
    return startOperation(counter, request, state.result, startProcedure);
}

Result AsyncFiberIO::receiveFrom(const SocketDescriptor& socket, Span<char> buffer,
                                 AsyncFiberSocketReceiveFromResult& outResult)
{
    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncSocketReceiveFrom   request;

    state.asyncFiber = this;
    state.counter    = &counter;
    outResult        = {};

    request.callback = [&state, &outResult](AsyncSocketReceiveFrom::Result& result)
    {
        state.result            = result.get(outResult.data);
        outResult.sourceAddress = result.getSourceAddress();
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    struct StartContext
    {
        AsyncSocketReceiveFrom* request = nullptr;
        const SocketDescriptor* socket  = nullptr;
        Span<char>              buffer;
    };
    StartContext                      startContext{&request, &socket, buffer};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, *startContext.socket, startContext.buffer); };
    return startOperation(counter, request, state.result, startProcedure);
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
    FiberCounter             counter;
    AsyncFiberOperationState state;

    state.asyncFiber = this;
    state.counter    = &counter;
    outResult        = {};

    request.callback = [&state, &outResult](AsyncFileRead::Result& result)
    {
        state.result        = result.get(outResult.data);
        outResult.endOfFile = result.completionData.endOfFile;
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    if (useOffset)
    {
        request.setOffset(offset);
    }

    struct StartContext
    {
        AsyncFileRead*        request = nullptr;
        const FileDescriptor* file    = nullptr;
        Span<char>            buffer;
    };
    StartContext                      startContext{&request, &file, buffer};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, *startContext.file, startContext.buffer); };
    return startOperation(counter, request, state.result, startProcedure);
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
            return Result::Error("AsyncFiberIO::fileReadExact reached end of file");
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

    FiberCounter             counter;
    AsyncFiberOperationState state;
    AsyncFileWrite           request;

    state.asyncFiber = this;
    state.counter    = &counter;
    if (outResult != nullptr)
    {
        outResult->numBytes = 0;
    }

    request.callback = [&state, outResult](AsyncFileWrite::Result& result)
    {
        state.result = result.isValid();
        if (outResult != nullptr and state.result)
        {
            state.result = result.get(outResult->numBytes);
        }
        state.asyncFiber->operationFinished();
        SC_ASYNC_FIBERS_TRUST_RESULT(state.asyncFiber->fiberScheduler().done(*state.counter));
    };

    if (useOffset)
    {
        request.setOffset(offset);
    }

    struct StartContext
    {
        AsyncFileWrite*       request = nullptr;
        const FileDescriptor* file    = nullptr;
        Span<const char>      data;
    };
    StartContext                      startContext{&request, &file, data};
    Function<Result(AsyncEventLoop&)> startProcedure = [&startContext](AsyncEventLoop& eventLoop)
    { return startContext.request->start(eventLoop, *startContext.file, startContext.data); };
    return startOperation(counter, request, state.result, startProcedure);
}

Result AsyncFiberIO::checkOwnerThread() const
{
    SC_ASYNC_FIBERS_ASSERT_RELEASE(isOwnerThread());
    SC_TRY_MSG(isOwnerThread(), "AsyncFiberIO used from a thread different than its owner thread");
    return Result(true);
}

Result AsyncFiberIO::checkFiberContext() const
{
    return scheduler.currentTask() != nullptr ? Result(true)
                                              : Result::Error("AsyncFiberIO operation must be called from a fiber");
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
        return Result::Error("AsyncFiberIO command queue storage is empty");
    }
    if (not command.execute.isValid())
    {
        return Result::Error("AsyncFiberIO command is invalid");
    }

    lockCommands();
    if (commandCount == commands.sizeInElements())
    {
        unlockCommands();
        return Result::Error("AsyncFiberIO command queue is full");
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
        operationResult = AsyncFiberTaskCancelled();
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
        return operationResult;
    }

    Result waitResult = scheduler.wait(counter);
    if (waitResult)
    {
        return operationResult;
    }
    if (scheduler.isCurrentTaskCancellationRequested())
    {
        operationResult = AsyncFiberTaskCancelled();
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
        return operationResult;
    }
    return waitResult;
}

Result AsyncFiberIO::stopOperation(FiberCounter& operationCounter, AsyncRequest& request)
{
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

    Result stopResult = Result(true);
    state.stopResult  = &stopResult;

    if (isOwnerThread())
    {
        stopResult = request.stop(eventLoop, &stopCallback);
    }
    else
    {
        AsyncFiberCommand command;
        command.execute = AsyncFiberCommand::Procedure([this, &state]() { return executeStopCommand(&state); });
        stopResult      = enqueueCommand(command);
        if (not stopResult)
        {
            SC_TRY(scheduler.waitUninterruptible(operationCounter));
            return stopResult;
        }
    }
    SC_TRY(stopResult);
    return scheduler.waitUninterruptible(counter);
}

Result AsyncFiberIO::executeStopCommand(void* stopStatePointer)
{
    AsyncFiberStopState& stopState = *static_cast<AsyncFiberStopState*>(stopStatePointer);
    *stopState.stopResult          = stopState.request->stop(eventLoop, stopState.stopCallback);
    if (not *stopState.stopResult)
    {
        SC_ASYNC_FIBERS_TRUST_RESULT(scheduler.done(*stopState.counter));
    }
    return Result(true);
}
} // namespace SC
