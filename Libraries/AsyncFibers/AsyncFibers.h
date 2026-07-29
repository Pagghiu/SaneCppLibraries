// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/CompilerMacrosExport.h"
#ifndef SC_EXPORT_LIBRARY_ASYNC_FIBERS
#define SC_EXPORT_LIBRARY_ASYNC_FIBERS 0
#endif
#define SC_ASYNC_FIBERS_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_ASYNC_FIBERS)

#include "../Async/Async.h"
#include "../Common/Assert.h"
#include "../Fibers/Fibers.h"
#include "../Threading/Atomic.h"

//! @defgroup group_async_fibers AsyncFibers
//! Experimental stackful fiber bridge over AsyncEventLoop.

//! @addtogroup group_async_fibers
//! @{
namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(AsyncFibersAssert, SC_ASYNC_FIBERS_EXPORT);

#define SC_ASYNC_FIBERS_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::AsyncFibersAssert, e)
#define SC_ASYNC_FIBERS_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::AsyncFibersAssert, e)
#define SC_ASYNC_FIBERS_TRUST_RESULT(expression) SC_ASYNC_FIBERS_ASSERT_RELEASE(expression)

struct AsyncFiberIO;

struct AsyncFiberCommand
{
    using Procedure = Function<Result()>;

    Procedure execute;
};

struct AsyncFiberSocketSendResult
{
    size_t numBytes = 0;
};

struct AsyncFiberSocketReceiveResult
{
    Span<char> data;
    bool       disconnected = false;
};

struct AsyncFiberSocketReceiveFromResult
{
    Span<char>      data;
    SocketIPAddress sourceAddress;
};

struct AsyncFiberFileReadResult
{
    Span<char> data;
    bool       endOfFile = false;
};

struct AsyncFiberFileWriteResult
{
    size_t numBytes = 0;
};

struct AsyncFiberFileSendOptions
{
    int64_t offset   = 0;
    size_t  length   = 0;
    size_t  pipeSize = 0;
};

struct AsyncFiberFileSendResult
{
    size_t bytesTransferred = 0;
    bool   usedZeroCopy     = false;
};

struct AsyncFiberProcessExitResult
{
    int exitStatus = -1;
};

struct AsyncFiberSignalResult
{
    int      signalNumber  = 0;
    uint32_t deliveryCount = 0;
};

//! Synchronous-looking fiber I/O wrapper around an externally owned AsyncEventLoop.
//! Operation methods require a currently running fiber of the supplied FiberScheduler.
struct SC_ASYNC_FIBERS_EXPORT AsyncFiberIO
{
    AsyncFiberIO(FiberScheduler& fiberScheduler, AsyncEventLoop& asyncEventLoop,
                 Span<AsyncFiberCommand> commandStorage = {});
    ~AsyncFiberIO();

    AsyncFiberIO(const AsyncFiberIO&)            = delete;
    AsyncFiberIO& operator=(const AsyncFiberIO&) = delete;
    AsyncFiberIO(AsyncFiberIO&&)                 = delete;
    AsyncFiberIO& operator=(AsyncFiberIO&&)      = delete;

    [[nodiscard]] FiberScheduler&       fiberScheduler();
    [[nodiscard]] const FiberScheduler& fiberScheduler() const;
    [[nodiscard]] AsyncEventLoop&       asyncEventLoop();
    [[nodiscard]] const AsyncEventLoop& asyncEventLoop() const;
    [[nodiscard]] bool                  isOwnerThread() const;

    Result run();
    Result runOnce();
    Result runNoWait();
    Result runUntilComplete();
    Result runUntilIdle();
    Result runOwner();
    Result runOwnerOnce();
    Result runOwnerNoWait();
    Result runOwnerUntilComplete();
    Result runOwnerUntilIdle();

    Result cancelAll();

    Result sleep(TimeMs duration);
    Result accept(const SocketDescriptor& serverSocket, SocketDescriptor& outClient);
    Result connect(const SocketDescriptor& socket, SocketIPAddress address);
    Result send(const SocketDescriptor& socket, Span<const char> data, AsyncFiberSocketSendResult* outResult = nullptr);
    Result receive(const SocketDescriptor& socket, Span<char> buffer, AsyncFiberSocketReceiveResult& outResult);
    Result sendAll(const SocketDescriptor& socket, Span<const char> data,
                   AsyncFiberSocketSendResult* outResult = nullptr);
    Result sendTo(const SocketDescriptor& socket, SocketIPAddress address, Span<const char> data,
                  AsyncFiberSocketSendResult* outResult = nullptr);
    Result receiveFrom(const SocketDescriptor& socket, Span<char> buffer, AsyncFiberSocketReceiveFromResult& outResult);
    Result fileRead(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult);
    Result fileReadAt(const FileDescriptor& file, uint64_t offset, Span<char> buffer,
                      AsyncFiberFileReadResult& outResult);
    Result fileReadExact(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult);
    Result fileReadExactAt(const FileDescriptor& file, uint64_t offset, Span<char> buffer,
                           AsyncFiberFileReadResult& outResult);
    Result filePoll(const FileDescriptor& file);
    Result fileWrite(const FileDescriptor& file, Span<const char> data, AsyncFiberFileWriteResult* outResult = nullptr);
    Result fileWriteAt(const FileDescriptor& file, uint64_t offset, Span<const char> data,
                       AsyncFiberFileWriteResult* outResult = nullptr);
    Result fileWriteAll(const FileDescriptor& file, Span<const char> data,
                        AsyncFiberFileWriteResult* outResult = nullptr);
    Result fileWriteAllAt(const FileDescriptor& file, uint64_t offset, Span<const char> data,
                          AsyncFiberFileWriteResult* outResult = nullptr);
    Result fileSend(const FileDescriptor& file, const SocketDescriptor& socket, AsyncFiberFileSendOptions options = {},
                    AsyncFiberFileSendResult* outResult = nullptr);
    Result processExit(FileDescriptor::Handle process, AsyncFiberProcessExitResult& outResult);
    Result signal(int signalNumber, AsyncFiberSignalResult& outResult);

  private:
    FiberScheduler& scheduler;
    AsyncEventLoop& eventLoop;

    Span<AsyncFiberCommand> commands;
    size_t                  commandHead  = 0;
    size_t                  commandCount = 0;
    mutable Atomic<int32_t> commandLock  = 0;

    Atomic<int32_t> pendingOperations = 0;
    uint64_t        ownerThreadID     = 0;

    Result checkOwnerThread() const;
    Result checkFiberContext() const;
    void   operationStarted();
    void   operationFinished();
    void   lockCommands() const;
    void   unlockCommands() const;
    Result enqueueCommand(AsyncFiberCommand& command);
    Result drainCommandQueue();
    bool   hasPendingCommands() const;
    Result fileReadImpl(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult,
                        uint64_t offset, bool useOffset);
    Result fileReadImpl(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult,
                        uint64_t offset, bool useOffset, AsyncFileRead& request);
    Result fileReadExactImpl(const FileDescriptor& file, Span<char> buffer, AsyncFiberFileReadResult& outResult,
                             uint64_t offset, bool useOffset);
    Result fileWriteImpl(const FileDescriptor& file, Span<const char> data, AsyncFiberFileWriteResult* outResult,
                         uint64_t offset, bool useOffset);
    Result startOperation(FiberCounter& counter, AsyncRequest& request, Result& operationResult,
                          Function<Result(AsyncEventLoop&)>& startProcedure);
    Result executeStartCommand(void* startState);
    Result waitForOperation(FiberCounter& counter, AsyncRequest& request, Result& operationResult,
                            void* startState = nullptr);
    Result stopOperation(FiberCounter& operationCounter, AsyncRequest& request);
    Result executeStopCommand(void* stopState);
};
} // namespace SC
//! @}
