// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/CompilerMacrosExport.h"
#ifndef SC_EXPORT_LIBRARY_FIBERS_ASYNC
#define SC_EXPORT_LIBRARY_FIBERS_ASYNC 0
#endif
#define SC_FIBER_ASYNC_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_FIBERS_ASYNC)

#include "../Async/Async.h"
#include "../Common/Assert.h"
#include "../Fibers/Fibers.h"
#include "../Threading/Atomic.h"

//! @defgroup group_fibers_async FibersAsync
//! Experimental stackful fiber bridge over AsyncEventLoop.

//! @addtogroup group_fibers_async
//! @{
namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(FiberAsyncAssert, SC_FIBER_ASYNC_EXPORT);

#define SC_FIBER_ASYNC_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::FiberAsyncAssert, e)
#define SC_FIBER_ASYNC_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::FiberAsyncAssert, e)
#define SC_FIBER_ASYNC_TRUST_RESULT(expression) SC_FIBER_ASYNC_ASSERT_RELEASE(expression)

struct FiberAsyncIO;

struct FiberAsyncCommand
{
    using Procedure = Function<Result()>;

    Procedure execute;
};

struct FiberAsyncSocketSendResult
{
    size_t numBytes = 0;
};

struct FiberAsyncSocketReceiveResult
{
    Span<char> data;
    bool       disconnected = false;
};

struct FiberAsyncSocketReceiveFromResult
{
    Span<char>      data;
    SocketIPAddress sourceAddress;
};

struct FiberAsyncFileReadResult
{
    Span<char> data;
    bool       endOfFile = false;
};

struct FiberAsyncFileWriteResult
{
    size_t numBytes = 0;
};

struct FiberAsyncFileSendOptions
{
    int64_t offset   = 0;
    size_t  length   = 0;
    size_t  pipeSize = 0;
};

struct FiberAsyncFileSendResult
{
    size_t bytesTransferred = 0;
    bool   usedZeroCopy     = false;
};

struct FiberAsyncProcessExitResult
{
    int exitStatus = -1;
};

struct FiberAsyncSignalResult
{
    int      signalNumber  = 0;
    uint32_t deliveryCount = 0;
};

//! Synchronous-looking fiber I/O wrapper around an externally owned AsyncEventLoop.
struct SC_FIBER_ASYNC_EXPORT FiberAsyncIO
{
    FiberAsyncIO(FiberScheduler& fiberScheduler, AsyncEventLoop& asyncEventLoop,
                 Span<FiberAsyncCommand> commandStorage = {});
    ~FiberAsyncIO();

    FiberAsyncIO(const FiberAsyncIO&)            = delete;
    FiberAsyncIO& operator=(const FiberAsyncIO&) = delete;
    FiberAsyncIO(FiberAsyncIO&&)                 = delete;
    FiberAsyncIO& operator=(FiberAsyncIO&&)      = delete;

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
    Result send(const SocketDescriptor& socket, Span<const char> data, FiberAsyncSocketSendResult* outResult = nullptr);
    Result receive(const SocketDescriptor& socket, Span<char> buffer, FiberAsyncSocketReceiveResult& outResult);
    Result sendAll(const SocketDescriptor& socket, Span<const char> data,
                   FiberAsyncSocketSendResult* outResult = nullptr);
    Result sendTo(const SocketDescriptor& socket, SocketIPAddress address, Span<const char> data,
                  FiberAsyncSocketSendResult* outResult = nullptr);
    Result receiveFrom(const SocketDescriptor& socket, Span<char> buffer, FiberAsyncSocketReceiveFromResult& outResult);
    Result fileRead(const FileDescriptor& file, Span<char> buffer, FiberAsyncFileReadResult& outResult);
    Result fileReadAt(const FileDescriptor& file, uint64_t offset, Span<char> buffer,
                      FiberAsyncFileReadResult& outResult);
    Result fileReadExact(const FileDescriptor& file, Span<char> buffer, FiberAsyncFileReadResult& outResult);
    Result fileReadExactAt(const FileDescriptor& file, uint64_t offset, Span<char> buffer,
                           FiberAsyncFileReadResult& outResult);
    Result filePoll(const FileDescriptor& file);
    Result fileWrite(const FileDescriptor& file, Span<const char> data, FiberAsyncFileWriteResult* outResult = nullptr);
    Result fileWriteAt(const FileDescriptor& file, uint64_t offset, Span<const char> data,
                       FiberAsyncFileWriteResult* outResult = nullptr);
    Result fileWriteAll(const FileDescriptor& file, Span<const char> data,
                        FiberAsyncFileWriteResult* outResult = nullptr);
    Result fileWriteAllAt(const FileDescriptor& file, uint64_t offset, Span<const char> data,
                          FiberAsyncFileWriteResult* outResult = nullptr);
    Result fileSend(const FileDescriptor& file, const SocketDescriptor& socket, FiberAsyncFileSendOptions options = {},
                    FiberAsyncFileSendResult* outResult = nullptr);
    Result processExit(FileDescriptor::Handle process, FiberAsyncProcessExitResult& outResult);
    Result signal(int signalNumber, FiberAsyncSignalResult& outResult);

  private:
    FiberScheduler& scheduler;
    AsyncEventLoop& eventLoop;

    Span<FiberAsyncCommand> commands;
    size_t                  commandHead  = 0;
    size_t                  commandCount = 0;
    mutable Atomic<int32_t> commandLock  = 0;

    Atomic<int32_t> pendingOperations = 0;
    uint64_t        ownerThreadID     = 0;

    Result checkOwnerThread() const;
    void   operationStarted();
    void   operationFinished();
    void   lockCommands() const;
    void   unlockCommands() const;
    Result enqueueCommand(FiberAsyncCommand& command);
    Result drainCommandQueue();
    bool   hasPendingCommands() const;
    Result fileReadImpl(const FileDescriptor& file, Span<char> buffer, FiberAsyncFileReadResult& outResult,
                        uint64_t offset, bool useOffset);
    Result fileReadImpl(const FileDescriptor& file, Span<char> buffer, FiberAsyncFileReadResult& outResult,
                        uint64_t offset, bool useOffset, AsyncFileRead& request);
    Result fileReadExactImpl(const FileDescriptor& file, Span<char> buffer, FiberAsyncFileReadResult& outResult,
                             uint64_t offset, bool useOffset);
    Result fileWriteImpl(const FileDescriptor& file, Span<const char> data, FiberAsyncFileWriteResult* outResult,
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
