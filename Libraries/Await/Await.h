// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/Compiler.h"
#ifndef SC_EXPORT_LIBRARY_AWAIT
#define SC_EXPORT_LIBRARY_AWAIT 0
#endif
#define SC_AWAIT_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_AWAIT)

#include "../Common/Assert.h"

#define SC_AWAIT_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::AwaitAssert, e)
#define SC_AWAIT_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::AwaitAssert, e)
#define SC_AWAIT_TRUST_RESULT(expression) SC_AWAIT_ASSERT_RELEASE(expression)

#ifndef SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE
#define SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE 0
#endif

#if !SC_INCLUDE_STD_CPP && !SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE
#error "SC::Await requires SC_INCLUDE_STD_CPP=1 or SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE=1."
#endif

#if !SC_LANGUAGE_CPP_AT_LEAST_20
#error "SC::Await requires C++20 or newer."
#endif

#include "../Async/Async.h"
#include "../Common/Result.h"
#include "Internal/AwaitCoroutine.h"

//! @defgroup group_await Await
//! @copybrief library_await (see @ref library_await for more details)
//! C++20 coroutine layer over SC::AsyncEventLoop.
//!
//! Await is an experimental wrapper that lets coroutine bodies express asynchronous operations with `co_await` while
//! still returning plain SC::Result values and using caller-provided output objects.

//! @addtogroup group_await
//! @{
namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(AwaitAssert, SC_AWAIT_EXPORT);

struct AwaitEventLoop;
struct AwaitAllocator;
struct AwaitAllocatorInterface;
struct AwaitTask;
struct AwaitCancellationHandler;
struct AwaitSleepAwaiter;
struct AwaitSocketAcceptAwaiter;
struct AwaitSocketConnectAwaiter;
struct AwaitSocketSendAwaiter;
struct AwaitSocketSendToAwaiter;
struct AwaitSocketSendAllAwaiter;
struct AwaitSocketSendAllBuffersAwaiter;
struct AwaitSocketReceiveAwaiter;
struct AwaitSocketReceiveExactAwaiter;
struct AwaitSocketReceiveLineAwaiter;
struct AwaitSocketReceiveFromAwaiter;
struct AwaitLoopWakeUp;
struct AwaitLoopWakeUpAwaiter;
struct AwaitFileReadAwaiter;
struct AwaitFileReadUntilFullOrEOFAwaiter;
struct AwaitFileWriteAwaiter;
struct AwaitFileSendAwaiter;
struct AwaitFilePollAwaiter;
struct AwaitFileSystemOperationAwaiter;
struct AwaitProcessExitAwaiter;
struct AwaitSignalAwaiter;
struct AwaitTaskGroup;
struct AwaitTaskRegistry;
struct AwaitTaskGroupWaitAllAwaiter;
struct AwaitTaskGroupWaitAnyAwaiter;
struct AwaitTaskRegistryWaitAllAwaiter;
struct AwaitTaskRegistryWaitAnyAwaiter;
struct AwaitTaskSpawnAwaiter;
struct AwaitTaskTimeoutAwaiter;
struct AwaitLoopWorkAwaiter;

SC_AWAIT_EXPORT const char* AwaitCancellationMessage();
SC_AWAIT_EXPORT Result      AwaitCancelledResult();
SC_AWAIT_EXPORT bool        AwaitIsCancelled(Result result);

SC_AWAIT_EXPORT const char* AwaitWrongEventLoopMessage();
SC_AWAIT_EXPORT Result      AwaitWrongEventLoopResult();
SC_AWAIT_EXPORT bool        AwaitIsWrongEventLoop(Result result);

/// @brief Checks the Result of a coroutine await expression and co_return-s the error to the caller.
#define SC_CO_TRY(expression)                                                                                          \
    {                                                                                                                  \
        if (auto _exprResConv = SC::Result(expression))                                                                \
            SC_LANGUAGE_LIKELY { (void)0; }                                                                            \
        else                                                                                                           \
        {                                                                                                              \
            co_return _exprResConv;                                                                                    \
        }                                                                                                              \
    }

struct AwaitSocketSendResult
{
    size_t numBytes = 0;
};

/// @brief Result object populated by AwaitEventLoop::receive.
struct AwaitSocketReceiveResult
{
    Span<char> data;
    bool       disconnected = false;
};

/// @brief Result object populated by AwaitEventLoop::receiveLine.
struct AwaitSocketReceiveLineResult
{
    Span<char> line;
    bool       disconnected = false;
    bool       lineComplete = false;
};

/// @brief Result object populated by AwaitEventLoop::receiveFrom.
struct AwaitSocketReceiveFromResult
{
    Span<char>      data;
    SocketIPAddress sourceAddress;
    bool            disconnected = false;
};

/// @brief Result object populated by AwaitEventLoop::fileRead.
struct AwaitFileReadResult
{
    Span<char> data;
    bool       endOfFile = false;
};

struct AwaitFileWriteResult
{
    size_t numBytes = 0;
};

struct AwaitFileReadOptions
{
    ThreadPool* threadPool = nullptr;
    uint64_t    offset     = 0;
    bool        useOffset  = false;
};

struct AwaitFileWriteOptions
{
    ThreadPool* threadPool = nullptr;
    uint64_t    offset     = 0;
    bool        useOffset  = false;
};

struct AwaitFileSendOptions
{
    int64_t     offset     = 0;
    size_t      length     = 0;
    size_t      pipeSize   = 0;
    ThreadPool* threadPool = nullptr;
};

struct AwaitFileSendResult
{
    size_t bytesTransferred = 0;
    bool   usedZeroCopy     = false;
    bool   complete         = false;
};

struct AwaitTimeoutResult
{
    bool timedOut = false;
};

struct AwaitProcessExitResult
{
    int exitStatus = -1;
};

struct AwaitSignalResult
{
    int      signalNumber  = 0;
    uint32_t deliveryCount = 0;
};

struct AwaitLoopWakeUpResult
{
    uint32_t deliveryCount = 0;
};

struct AwaitTaskGroupWaitAnyResult
{
    size_t     index = size_t(-1);
    AwaitTask* task  = nullptr;
};

struct AwaitTaskGroupResultSummary
{
    size_t     numTasks          = 0;
    size_t     numCompleted      = 0;
    size_t     numSucceeded      = 0;
    size_t     numFailed         = 0;
    size_t     firstFailureIndex = size_t(-1);
    AwaitTask* firstFailureTask  = nullptr;
    Result     firstFailure      = Result(true);
};

struct AwaitTaskRegistrySpawnResult
{
    size_t     index = size_t(-1);
    AwaitTask* task  = nullptr;
};

struct AwaitTaskRegistryWaitAnyResult
{
    size_t     index = size_t(-1);
    AwaitTask* task  = nullptr;
};

enum class AwaitTaskGroupCancelPolicy : uint8_t
{
    CancelChildren,
    LeaveChildrenRunning,
};

enum class AwaitTaskGroupWaitAnyPolicy : uint8_t
{
    CancelRemaining,
    LeaveRemainingRunning,
};

enum class AwaitTaskRegistryWaitAnyPolicy : uint8_t
{
    CancelRemaining,
    LeaveRemainingRunning,
};

enum class AwaitAllocatorMode : uint8_t
{
    None,
    Fixed,
    Virtual,
    Malloc,
    Polymorphic,
};

struct AwaitAllocatorStatistics
{
    size_t numAllocations = 0;
    size_t numReleases    = 0;

    size_t requestedBytesAllocated = 0;
    size_t requestedBytesReleased  = 0;

    size_t bytesInUse                     = 0;
    size_t peakBytesInUse                 = 0;
    size_t largestRequestedAllocationSize = 0;

    size_t numAllocationFailures       = 0;
    size_t lastFailedAllocationSize    = 0;
    size_t largestFailedAllocationSize = 0;
};

struct AwaitAllocatorVirtualOptions
{
    size_t reserveBytes       = 0;
    size_t initialCommitBytes = 0;
};

struct AwaitAllocatorInterface
{
    virtual void* allocateImpl(const void* owner, size_t numBytes, size_t alignment) = 0;
    virtual void  releaseImpl(void* memory)                                          = 0;
    virtual ~AwaitAllocatorInterface() {}
};

enum class AwaitFileSystemOperationType : uint8_t
{
    Open,
    Close,
    Read,
    Write,
    CopyFile,
    CopyDirectory,
    Rename,
    RemoveEmptyDirectory,
    RemoveFile,
};

/// @brief Cancellation hook installed by the awaiter currently suspending an AwaitTask.
struct AwaitCancellationHandler
{
    void* object = nullptr;

    Result (*cancel)(void* object, AwaitEventLoop& eventLoop) = nullptr;
};

/// @brief Explicit allocator used by AwaitTask coroutine frame allocation.
struct SC_AWAIT_EXPORT AwaitAllocator
{
    AwaitAllocator()                                 = default;
    AwaitAllocator(const AwaitAllocator&)            = delete;
    AwaitAllocator& operator=(const AwaitAllocator&) = delete;
    AwaitAllocator(AwaitAllocator&&)                 = delete;
    AwaitAllocator& operator=(AwaitAllocator&&)      = delete;
    ~AwaitAllocator();

    [[nodiscard]] Result createFixed(Span<char> storage);
    [[nodiscard]] Result createVirtual(AwaitAllocatorVirtualOptions options);
    [[nodiscard]] Result createMalloc();
    [[nodiscard]] Result createPolymorphic(AwaitAllocatorInterface& customAllocatorInterface);
    [[nodiscard]] Result close();

    [[nodiscard]] void* allocate(const void* owner, size_t numBytes, size_t alignment);
    void                release(void* memory);
    static void         releaseFromAnyAllocator(void* memory);

    [[nodiscard]] AwaitAllocatorMode       mode() const;
    [[nodiscard]] AwaitAllocatorStatistics statistics() const;
    [[nodiscard]] bool                     isOpen() const;

    [[nodiscard]] size_t used() const;
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t peakUsed() const;
    [[nodiscard]] size_t largestAllocationSize() const;
    [[nodiscard]] size_t failedAllocationSize() const;
    [[nodiscard]] size_t reservedBytes() const;
    [[nodiscard]] size_t committedBytes() const;

  private:
    struct BlockHeader;

    Result initializeFixedStorage(Span<char> storage);
    void*  allocateFromBlocks(const void* owner, size_t numBytes, size_t alignment);
    void   releaseBlock(BlockHeader& header);
    bool   ensureCommitted(size_t sizeInBytes);
    void   releaseVirtualMemory();
    void   recordAllocationFailure(size_t numBytes);
    void   resetState();

    AwaitAllocatorMode       currentMode = AwaitAllocatorMode::None;
    AwaitAllocatorStatistics currentStatistics;
    Span<char>               fixedStorage;
    BlockHeader*             firstBlock         = nullptr;
    AwaitAllocatorInterface* allocatorInterface = nullptr;

    void*  virtualMemory         = nullptr;
    size_t virtualReservedBytes  = 0;
    size_t virtualCommittedBytes = 0;
};

/// @brief Caller-owned coroutine task returning a plain SC::Result.
struct SC_AWAIT_EXPORT AwaitTask
{
    struct Promise;
    using promise_type = Promise;
    using Handle       = AwaitCoroutineTypedHandle<Promise>;

    AwaitTask() = default;
    explicit AwaitTask(Handle newHandle);

    AwaitTask(const AwaitTask&)            = delete;
    AwaitTask& operator=(const AwaitTask&) = delete;

    AwaitTask(AwaitTask&& other) noexcept;
    AwaitTask& operator=(AwaitTask&& other) noexcept;
    ~AwaitTask();

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isStarted() const;
    [[nodiscard]] bool isCompleted() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] bool isCancellationRequested() const;

    [[nodiscard]] Result result() const;

    Result cancel(AwaitEventLoop& await);

    bool   await_ready() const;
    bool   await_suspend(Handle continuation);
    Result await_resume() const;

  private:
    friend struct AwaitEventLoop;
    friend struct AwaitTaskGroup;
    friend struct AwaitTaskGroupWaitAllAwaiter;
    friend struct AwaitTaskGroupWaitAnyAwaiter;
    friend struct AwaitTaskRegistryWaitAllAwaiter;
    friend struct AwaitTaskRegistryWaitAnyAwaiter;
    friend struct AwaitTaskSpawnAwaiter;
    friend struct AwaitTaskTimeoutAwaiter;

    Result start();
    void   resume();
    void   destroy();

    Handle handle = {};
};

/// @brief Coroutine promise implementation used by AwaitTask.
struct SC_AWAIT_EXPORT AwaitTask::Promise
{
    Promise();

    template <typename First, typename... Rest>
    Promise(First& first, Rest&... rest) : Promise()
    {
        eventLoop = findEventLoop(first, rest...);
    }

    template <typename... Rest>
    Promise(AwaitEventLoop& await, Rest&...) : Promise()
    {
        eventLoop = &await;
    }

    static AwaitEventLoop* findEventLoop();
    static AwaitEventLoop* findEventLoop(AwaitEventLoop& await);

    template <typename First, typename... Rest>
    static AwaitEventLoop* findEventLoop(First&, Rest&... rest)
    {
        return findEventLoop(rest...);
    }

    static void* allocateFrame(size_t size, AwaitEventLoop* eventLoop) noexcept;
    static void  deallocateFrame(void* frame) noexcept;

    static void* operator new(size_t size) noexcept;

    template <typename First, typename... Rest>
    static void* operator new(size_t size, First& first, Rest&... rest) noexcept
    {
        return allocateFrame(size, findEventLoop(first, rest...));
    }

    template <typename... Rest>
    static void* operator new(size_t size, AwaitEventLoop& await, Rest&...) noexcept
    {
        return allocateFrame(size, &await);
    }

    static void operator delete(void* frame, size_t) noexcept;

    static AwaitTask get_return_object_on_allocation_failure();

    AwaitTask get_return_object();

    AwaitSuspendAlways initial_suspend() noexcept;

    struct FinalSuspend
    {
        bool await_ready() noexcept;
        void await_suspend(AwaitTask::Handle handle) noexcept;
        void await_resume() noexcept;
    };

    FinalSuspend final_suspend() noexcept;

    void return_value(Result newResult) noexcept;

    void unhandled_exception() noexcept;

    Result taskResult;

    AwaitTask::Handle        continuation;
    AwaitTask::Handle        deferredDestroyNext;
    AwaitCancellationHandler cancellation;
    AwaitEventLoop*          eventLoop;
    void*                    completionObject;
    void (*completionCallback)(void* object);

    bool started;
    bool completed;
    bool cancellationRequested;
    bool inCompletionCallback;
    bool destroyDeferred;
};

/// @brief Coroutine-friendly wrapper around an existing AsyncEventLoop.
struct SC_AWAIT_EXPORT AwaitEventLoop
{
    explicit AwaitEventLoop(AsyncEventLoop& asyncEventLoop, AwaitAllocator& allocator);

    [[nodiscard]] AsyncEventLoop&       asyncEventLoop();
    [[nodiscard]] const AsyncEventLoop& asyncEventLoop() const;
    [[nodiscard]] AwaitAllocator&       allocator();

    Result spawn(AwaitTask& task);

    Result run();
    Result runOnce();
    Result runNoWait();

    AwaitSleepAwaiter         sleep(TimeMs duration);
    AwaitSocketAcceptAwaiter  accept(const SocketDescriptor& serverSocket, SocketDescriptor& outClient);
    AwaitSocketConnectAwaiter connect(const SocketDescriptor& socket, SocketIPAddress address);
    AwaitSocketSendAwaiter    send(const SocketDescriptor& socket, Span<const char> data,
                                   AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketSendAwaiter    send(const SocketDescriptor& socket, Span<Span<const char>> data,
                                   AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketSendToAwaiter  sendTo(const SocketDescriptor& socket, SocketIPAddress address, Span<const char> data,
                                     AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketSendToAwaiter  sendTo(const SocketDescriptor& socket, SocketIPAddress address,
                                     Span<Span<const char>> data, AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketSendAllAwaiter sendAll(const SocketDescriptor& socket, Span<const char> data,
                                      AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketSendAllBuffersAwaiter sendAll(const SocketDescriptor& socket, Span<Span<const char>> data,
                                             AwaitSocketSendResult* outResult = nullptr);
    AwaitSocketReceiveAwaiter        receive(const SocketDescriptor& socket, Span<char> buffer,
                                             AwaitSocketReceiveResult& outResult);
    AwaitSocketReceiveExactAwaiter   receiveExact(const SocketDescriptor& socket, Span<char> buffer,
                                                  AwaitSocketReceiveResult* outResult = nullptr);
    AwaitSocketReceiveLineAwaiter    receiveLine(const SocketDescriptor& socket, Span<char> buffer,
                                                 AwaitSocketReceiveLineResult& outResult);
    AwaitSocketReceiveFromAwaiter    receiveFrom(const SocketDescriptor& socket, Span<char> buffer,
                                                 AwaitSocketReceiveFromResult& outResult);

    AwaitFileReadAwaiter fileRead(const FileDescriptor& file, Span<char> buffer, AwaitFileReadResult& outResult,
                                  AwaitFileReadOptions options = {});
    AwaitFileReadUntilFullOrEOFAwaiter fileReadUntilFullOrEOF(const FileDescriptor& file, Span<char> buffer,
                                                              AwaitFileReadResult& outResult,
                                                              AwaitFileReadOptions options = {});

    AwaitFileWriteAwaiter fileWrite(const FileDescriptor& file, Span<const char> data,
                                    AwaitFileWriteResult* outResult = nullptr, AwaitFileWriteOptions options = {});
    AwaitFileWriteAwaiter fileWrite(const FileDescriptor& file, Span<Span<const char>> data,
                                    AwaitFileWriteResult* outResult = nullptr, AwaitFileWriteOptions options = {});
    AwaitFileSendAwaiter  fileSend(const FileDescriptor& file, const SocketDescriptor& socket,
                                   AwaitFileSendResult& outResult, AwaitFileSendOptions options = {});
    AwaitFilePollAwaiter  filePoll(const FileDescriptor& file);

    AwaitLoopWakeUpAwaiter wakeUp(AwaitLoopWakeUp& wakeUp, AwaitLoopWakeUpResult& outResult,
                                  AsyncLoopWakeUpOptions options = {});

    AwaitFileSystemOperationAwaiter fsOpen(ThreadPool& threadPool, StringSpan path, FileOpen mode,
                                           FileDescriptor& outFile);
    AwaitFileSystemOperationAwaiter fsClose(ThreadPool& threadPool, FileDescriptor& file);
    AwaitFileSystemOperationAwaiter fsRead(ThreadPool& threadPool, FileDescriptor& file, Span<char> buffer,
                                           AwaitFileReadResult& outResult, uint64_t offset = 0);
    AwaitFileSystemOperationAwaiter fsWrite(ThreadPool& threadPool, FileDescriptor& file, Span<const char> data,
                                            AwaitFileWriteResult* outResult = nullptr, uint64_t offset = 0);
    AwaitFileSystemOperationAwaiter fsCopyFile(ThreadPool& threadPool, StringSpan path, StringSpan destinationPath,
                                               FileSystemCopyFlags copyFlags = FileSystemCopyFlags());
    AwaitFileSystemOperationAwaiter fsCopyDirectory(ThreadPool& threadPool, StringSpan path, StringSpan destinationPath,
                                                    FileSystemCopyFlags copyFlags = FileSystemCopyFlags());
    AwaitFileSystemOperationAwaiter fsRename(ThreadPool& threadPool, StringSpan path, StringSpan newPath);
    AwaitFileSystemOperationAwaiter fsRemoveEmptyDirectory(ThreadPool& threadPool, StringSpan path);
    AwaitFileSystemOperationAwaiter fsRemoveFile(ThreadPool& threadPool, StringSpan path);
    AwaitProcessExitAwaiter         processExit(FileDescriptor::Handle process, AwaitProcessExitResult& outResult);
    AwaitSignalAwaiter              signal(int signalNumber, AwaitSignalResult& outResult);
    AwaitTaskSpawnAwaiter           spawnAndWait(AwaitTask& task);
    AwaitTaskTimeoutAwaiter         waitFor(AwaitTask& task, TimeMs timeout, AwaitTimeoutResult* outResult = nullptr);
    AwaitLoopWorkAwaiter            loopWork(ThreadPool& threadPool, Function<Result()> work);

  private:
    friend struct AwaitTask;

    void deferDestroy(AwaitTask::Handle handle);
    void drainDeferredDestroys();

    AsyncEventLoop&   eventLoop;
    AwaitAllocator*   frameAllocator;
    AwaitTask::Handle deferredDestroyList;
};

/// @brief Stable wake-up object that can resume an AwaitLoopWakeUpAwaiter from another thread.
struct SC_AWAIT_EXPORT AwaitLoopWakeUp
{
    Result wakeUp(AwaitEventLoop& await);
    Result wakeUp(AsyncEventLoop& eventLoop);

    [[nodiscard]] bool isActive() const;

  private:
    friend struct AwaitLoopWakeUpAwaiter;

    AsyncLoopWakeUp request;
};

/// @brief Awaiter for a single AsyncLoopTimeout operation.
struct SC_AWAIT_EXPORT AwaitSleepAwaiter
{
    AwaitSleepAwaiter(AwaitEventLoop& await, TimeMs duration);

    AwaitEventLoop&  await;
    TimeMs           duration;
    AsyncLoopTimeout request;
    Result           operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncLoopWakeUp delivery.
struct SC_AWAIT_EXPORT AwaitLoopWakeUpAwaiter
{
    AwaitLoopWakeUpAwaiter(AwaitEventLoop& await, AwaitLoopWakeUp& wakeUp, AwaitLoopWakeUpResult& outResult,
                           AsyncLoopWakeUpOptions options);

    AwaitEventLoop&        await;
    AwaitLoopWakeUp&       wakeUp;
    AwaitLoopWakeUpResult& outResult;
    AsyncLoopWakeUpOptions options;
    Result                 operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketAccept operation.
struct SC_AWAIT_EXPORT AwaitSocketAcceptAwaiter
{
    AwaitSocketAcceptAwaiter(AwaitEventLoop& await, const SocketDescriptor& serverSocket, SocketDescriptor& outClient);

    AwaitEventLoop&         await;
    const SocketDescriptor& serverSocket;
    SocketDescriptor&       outClient;
    AsyncSocketAccept       request;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketConnect operation.
struct SC_AWAIT_EXPORT AwaitSocketConnectAwaiter
{
    AwaitSocketConnectAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, SocketIPAddress address);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    SocketIPAddress         address;
    AsyncSocketConnect      request;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketSend operation.
struct SC_AWAIT_EXPORT AwaitSocketSendAwaiter
{
    AwaitSocketSendAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<const char> data,
                           AwaitSocketSendResult* outResult);
    AwaitSocketSendAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<Span<const char>> data,
                           AwaitSocketSendResult* outResult);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    Span<const char>        data;
    Span<Span<const char>>  buffers;
    AwaitSocketSendResult*  outResult = nullptr;
    AsyncSocketSend         request;
    Result                  operationResult = Result(true);
    bool                    singleBuffer    = true;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketSendTo operation.
struct SC_AWAIT_EXPORT AwaitSocketSendToAwaiter
{
    AwaitSocketSendToAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, SocketIPAddress address,
                             Span<const char> data, AwaitSocketSendResult* outResult);
    AwaitSocketSendToAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, SocketIPAddress address,
                             Span<Span<const char>> data, AwaitSocketSendResult* outResult);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    SocketIPAddress         address;
    Span<const char>        data;
    Span<Span<const char>>  buffers;
    AwaitSocketSendResult*  outResult = nullptr;
    AsyncSocketSendTo       request;
    Result                  operationResult = Result(true);
    bool                    singleBuffer    = true;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter that reactivates AsyncSocketSend until the whole buffer is sent.
struct SC_AWAIT_EXPORT AwaitSocketSendAllAwaiter
{
    AwaitSocketSendAllAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<const char> data,
                              AwaitSocketSendResult* outResult);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    Span<const char>        data;
    AwaitSocketSendResult*  outResult = nullptr;
    AsyncSocketSend         request;
    Result                  operationResult = Result(true);
    size_t                  numBytesSent    = 0;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter that sends every buffer in a scatter/gather list.
struct SC_AWAIT_EXPORT AwaitSocketSendAllBuffersAwaiter
{
    AwaitSocketSendAllBuffersAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<Span<const char>> data,
                                     AwaitSocketSendResult* outResult);

    AwaitEventLoop&         await;
    const SocketDescriptor& socket;
    Span<Span<const char>>  data;
    AwaitSocketSendResult*  outResult = nullptr;
    AsyncSocketSend         request;
    AsyncLoopTimeout        deferredStart;
    Result                  operationResult = Result(true);
    size_t                  numBytesSent    = 0;
    size_t                  bufferIndex     = 0;
    size_t                  bufferOffset    = 0;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    bool   findNextBuffer();
    Result startCurrentBuffer();
    Result updateRequestBuffer();

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketReceive operation.
struct SC_AWAIT_EXPORT AwaitSocketReceiveAwaiter
{
    AwaitSocketReceiveAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<char> buffer,
                              AwaitSocketReceiveResult& outResult);

    AwaitEventLoop&           await;
    const SocketDescriptor&   socket;
    Span<char>                buffer;
    AwaitSocketReceiveResult& outResult;
    AsyncSocketReceive        request;
    Result                    operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter that reactivates AsyncSocketReceive until the whole caller buffer is filled.
struct SC_AWAIT_EXPORT AwaitSocketReceiveExactAwaiter
{
    AwaitSocketReceiveExactAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<char> buffer,
                                   AwaitSocketReceiveResult* outResult);

    AwaitEventLoop&           await;
    const SocketDescriptor&   socket;
    Span<char>                buffer;
    AwaitSocketReceiveResult* outResult = nullptr;
    AsyncSocketReceive        request;
    Result                    operationResult  = Result(true);
    size_t                    numBytesReceived = 0;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    Result startRemainingReceive();
    Result updateRequestBuffer();
    Result updateOutResult(bool disconnected);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter that reads a '\n'-terminated line into caller-provided storage.
struct SC_AWAIT_EXPORT AwaitSocketReceiveLineAwaiter
{
    AwaitSocketReceiveLineAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<char> buffer,
                                  AwaitSocketReceiveLineResult& outResult);

    AwaitEventLoop&               await;
    const SocketDescriptor&       socket;
    Span<char>                    buffer;
    AwaitSocketReceiveLineResult& outResult;
    AsyncSocketReceive            request;
    Result                        operationResult  = Result(true);
    size_t                        numBytesReceived = 0;
    char                          currentByte      = 0;
    bool                          lineComplete     = false;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    Result startNextByte();
    Result updateOutResult(bool disconnected);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncSocketReceiveFrom operation.
struct SC_AWAIT_EXPORT AwaitSocketReceiveFromAwaiter
{
    AwaitSocketReceiveFromAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket, Span<char> buffer,
                                  AwaitSocketReceiveFromResult& outResult);

    AwaitEventLoop&               await;
    const SocketDescriptor&       socket;
    Span<char>                    buffer;
    AwaitSocketReceiveFromResult& outResult;
    AsyncSocketReceiveFrom        request;
    Result                        operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncFileRead operation.
struct SC_AWAIT_EXPORT AwaitFileReadAwaiter
{
    AwaitFileReadAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<char> buffer,
                         AwaitFileReadResult& outResult, AwaitFileReadOptions options);

    AwaitEventLoop&       await;
    const FileDescriptor& file;
    Span<char>            buffer;
    AwaitFileReadResult&  outResult;
    AsyncFileRead         request;
    AsyncTaskSequence     taskSequence;
    AwaitFileReadOptions  options;
    Result                operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter that reads until the caller buffer is full or EOF is reached.
struct SC_AWAIT_EXPORT AwaitFileReadUntilFullOrEOFAwaiter
{
    AwaitFileReadUntilFullOrEOFAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<char> buffer,
                                       AwaitFileReadResult& outResult, AwaitFileReadOptions options);

    AwaitEventLoop&       await;
    const FileDescriptor& file;
    Span<char>            buffer;
    AwaitFileReadResult&  outResult;
    AsyncFileRead         request;
    AsyncTaskSequence     taskSequence;
    AwaitFileReadOptions  options;
    Result                operationResult = Result(true);
    size_t                numBytesRead    = 0;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);
    Result startRemainingRead();
    Result updateRequestBufferAndOffset();
    Result updateOutResult(bool endOfFile);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncFileWrite operation.
struct SC_AWAIT_EXPORT AwaitFileWriteAwaiter
{
    AwaitFileWriteAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<const char> data,
                          AwaitFileWriteResult* outResult, AwaitFileWriteOptions options);
    AwaitFileWriteAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<Span<const char>> data,
                          AwaitFileWriteResult* outResult, AwaitFileWriteOptions options);

    AwaitEventLoop&        await;
    const FileDescriptor&  file;
    Span<const char>       data;
    Span<Span<const char>> buffers;
    AwaitFileWriteResult*  outResult = nullptr;
    AsyncFileWrite         request;
    AsyncTaskSequence      taskSequence;
    AwaitFileWriteOptions  options;
    Result                 operationResult = Result(true);
    bool                   singleBuffer    = true;

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncFileSend operation.
struct SC_AWAIT_EXPORT AwaitFileSendAwaiter
{
    AwaitFileSendAwaiter(AwaitEventLoop& await, const FileDescriptor& file, const SocketDescriptor& socket,
                         AwaitFileSendResult& outResult, AwaitFileSendOptions options);

    AwaitEventLoop&         await;
    const FileDescriptor&   file;
    const SocketDescriptor& socket;
    AwaitFileSendResult&    outResult;
    AwaitFileSendOptions    options;
    AsyncFileSend           request;
    AsyncTaskSequence       taskSequence;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single AsyncFilePoll operation.
struct SC_AWAIT_EXPORT AwaitFilePollAwaiter
{
    AwaitFilePollAwaiter(AwaitEventLoop& await, const FileDescriptor& file);

    AwaitEventLoop&       await;
    const FileDescriptor& file;
    AsyncFilePoll         request;
    Result                operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for selected AsyncFileSystemOperation path operations.
struct SC_AWAIT_EXPORT AwaitFileSystemOperationAwaiter
{
    AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                    AwaitFileSystemOperationType operation, StringSpan path,
                                    StringSpan otherPath = StringSpan(), FileOpen mode = FileOpen(),
                                    FileDescriptor*     outFile   = nullptr,
                                    FileSystemCopyFlags copyFlags = FileSystemCopyFlags());
    AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                    AwaitFileSystemOperationType operation, FileDescriptor& file);
    AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                    AwaitFileSystemOperationType operation, FileDescriptor& file, Span<char> buffer,
                                    AwaitFileReadResult& outResult, uint64_t offset);
    AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                    AwaitFileSystemOperationType operation, FileDescriptor& file, Span<const char> data,
                                    AwaitFileWriteResult* outResult, uint64_t offset);

    AwaitEventLoop&              await;
    ThreadPool&                  threadPool;
    AwaitFileSystemOperationType operation;
    StringSpan                   path;
    StringSpan                   otherPath;
    FileOpen                     mode;
    FileDescriptor*              outFile     = nullptr;
    FileDescriptor*              fileToClose = nullptr;
    FileDescriptor*              fileToUse   = nullptr;
    Span<char>                   readBuffer;
    Span<const char>             writeBuffer;
    AwaitFileReadResult*         outReadResult  = nullptr;
    AwaitFileWriteResult*        outWriteResult = nullptr;
    uint64_t                     offset         = 0;
    FileSystemCopyFlags          copyFlags;
    AsyncFileSystemOperation     request;
    Result                       operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Caller-storage structured group of child tasks owned by the current scope.
struct SC_AWAIT_EXPORT AwaitTaskGroup
{
    AwaitTaskGroup(AwaitEventLoop& await, Span<AwaitTask*> taskStorage,
                   AwaitTaskGroupCancelPolicy cancelPolicy = AwaitTaskGroupCancelPolicy::CancelChildren);

    Result                       spawn(AwaitTask& task);
    Result                       spawnAll(Span<AwaitTask*> taskList);
    AwaitTaskGroupWaitAllAwaiter waitAll();
    AwaitTaskGroupWaitAnyAwaiter waitAny(
        AwaitTaskGroupWaitAnyResult& outResult,
        AwaitTaskGroupWaitAnyPolicy  waitAnyPolicy = AwaitTaskGroupWaitAnyPolicy::CancelRemaining);
    Result collectResults(Span<Result> outResults, AwaitTaskGroupResultSummary* outSummary = nullptr) const;
    Result summarizeResults(AwaitTaskGroupResultSummary& outSummary) const;

    [[nodiscard]] size_t size() const;
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t remainingCapacity() const;
    [[nodiscard]] bool   isEmpty() const;
    [[nodiscard]] bool   isFull() const;

  private:
    friend struct AwaitTaskGroupWaitAllAwaiter;
    friend struct AwaitTaskGroupWaitAnyAwaiter;

    AwaitEventLoop&            await;
    Span<AwaitTask*>           tasks;
    AwaitTaskGroupCancelPolicy cancelPolicy;
    size_t                     numTasks = 0;
};

/// @brief Caller-owned fixed-slot registry for detached/background tasks.
struct SC_AWAIT_EXPORT AwaitTaskRegistry
{
    AwaitTaskRegistry(AwaitEventLoop& await, Span<AwaitTask> taskStorage);

    Result                          spawn(AwaitTask&& task, AwaitTaskRegistrySpawnResult* outResult = nullptr);
    Result                          cancelAll();
    AwaitTaskRegistryWaitAllAwaiter waitAll();
    AwaitTaskRegistryWaitAnyAwaiter waitAny(
        AwaitTaskRegistryWaitAnyResult& outResult,
        AwaitTaskRegistryWaitAnyPolicy  waitAnyPolicy = AwaitTaskRegistryWaitAnyPolicy::CancelRemaining);

    size_t clearCompleted(AwaitTaskGroupResultSummary* outSummary = nullptr);

    [[nodiscard]] AwaitTask*       taskAt(size_t index);
    [[nodiscard]] const AwaitTask* taskAt(size_t index) const;
    [[nodiscard]] size_t           size() const;
    [[nodiscard]] size_t           activeCount() const;
    [[nodiscard]] size_t           completedCount() const;
    [[nodiscard]] size_t           capacity() const;
    [[nodiscard]] size_t           remainingCapacity() const;
    [[nodiscard]] bool             isEmpty() const;
    [[nodiscard]] bool             isFull() const;
    [[nodiscard]] bool             hasActiveTasks() const;
    [[nodiscard]] bool             hasCompletedTasks() const;

  private:
    friend struct AwaitTaskRegistryWaitAllAwaiter;
    friend struct AwaitTaskRegistryWaitAnyAwaiter;

    AwaitEventLoop& await;
    Span<AwaitTask> tasks;
};

/// @brief Awaiter that waits for every valid task in an AwaitTaskRegistry.
struct SC_AWAIT_EXPORT AwaitTaskRegistryWaitAllAwaiter
{
    explicit AwaitTaskRegistryWaitAllAwaiter(AwaitTaskRegistry& registry);

    AwaitTaskRegistry& registry;
    Result             operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);
    static void   onTaskCompleted(void* object);

    Result cancel(AwaitEventLoop& eventLoop);
    void   onTaskCompleted();
    void   finish(Result result);
    void   clearTaskCallbacks();
    Result collectResult() const;

    AwaitTask::Handle continuation;
    size_t            totalTasks     = 0;
    size_t            completedTasks = 0;
    bool              finished       = false;
};

/// @brief Awaiter that waits for the first valid task in an AwaitTaskRegistry to complete.
struct SC_AWAIT_EXPORT AwaitTaskRegistryWaitAnyAwaiter
{
    AwaitTaskRegistryWaitAnyAwaiter(AwaitTaskRegistry& registry, AwaitTaskRegistryWaitAnyResult& outResult,
                                    AwaitTaskRegistryWaitAnyPolicy waitAnyPolicy);

    AwaitTaskRegistry&              registry;
    AwaitTaskRegistryWaitAnyResult& outResult;
    AwaitTaskRegistryWaitAnyPolicy  waitAnyPolicy;
    Result                          operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);
    static void   onTaskCompleted(void* object);

    Result cancel(AwaitEventLoop& eventLoop);
    void   onTaskCompleted();
    void   finish(Result result);
    void   clearTaskCallbacks();
    Result setWinner(size_t index);
    Result cancelRemaining(AwaitEventLoop& eventLoop);

    AwaitTask::Handle continuation;
    size_t            totalTasks     = 0;
    size_t            completedTasks = 0;
    size_t            winnerIndex    = size_t(-1);
    bool              cancelling     = false;
    bool              finished       = false;
};

/// @brief Awaiter that waits for every active task in an AwaitTaskGroup.
struct SC_AWAIT_EXPORT AwaitTaskGroupWaitAllAwaiter
{
    explicit AwaitTaskGroupWaitAllAwaiter(AwaitTaskGroup& group);

    AwaitTaskGroup& group;
    Result          operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);
    static void   onTaskCompleted(void* object);

    Result cancel(AwaitEventLoop& eventLoop);
    void   onTaskCompleted();
    void   finish(Result result);
    void   clearChildCallbacks();
    Result collectResult() const;

    AwaitTask::Handle continuation;
    size_t            completedTasks = 0;
    bool              finished       = false;
};

/// @brief Awaiter that waits for the first active task in an AwaitTaskGroup to complete.
struct SC_AWAIT_EXPORT AwaitTaskGroupWaitAnyAwaiter
{
    AwaitTaskGroupWaitAnyAwaiter(AwaitTaskGroup& group, AwaitTaskGroupWaitAnyResult& outResult,
                                 AwaitTaskGroupWaitAnyPolicy waitAnyPolicy);

    AwaitTaskGroup&              group;
    AwaitTaskGroupWaitAnyResult& outResult;
    AwaitTaskGroupWaitAnyPolicy  waitAnyPolicy;
    Result                       operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);
    static void   onTaskCompleted(void* object);

    Result cancel(AwaitEventLoop& eventLoop);
    void   onTaskCompleted();
    void   finish(Result result);
    void   clearChildCallbacks();
    Result setWinner(size_t index);
    Result cancelRemaining(AwaitEventLoop& eventLoop);

    AwaitTask::Handle continuation;
    size_t            completedTasks = 0;
    size_t            winnerIndex    = size_t(-1);
    bool              cancelling     = false;
    bool              finished       = false;
};

/// @brief Awaiter for a single AsyncProcessExit operation.
struct SC_AWAIT_EXPORT AwaitProcessExitAwaiter
{
    AwaitProcessExitAwaiter(AwaitEventLoop& await, FileDescriptor::Handle process, AwaitProcessExitResult& outResult);

    AwaitEventLoop&         await;
    FileDescriptor::Handle  process = FileDescriptor::Invalid;
    AwaitProcessExitResult& outResult;
    AsyncProcessExit        request;
    Result                  operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter for a single one-shot AsyncSignal operation.
struct SC_AWAIT_EXPORT AwaitSignalAwaiter
{
    AwaitSignalAwaiter(AwaitEventLoop& await, int signalNumber, AwaitSignalResult& outResult);

    AwaitEventLoop&    await;
    int                signalNumber = 0;
    AwaitSignalResult& outResult;
    AsyncSignal        request;
    Result             operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};

/// @brief Awaiter that starts a child task if needed, then waits for it to complete.
struct SC_AWAIT_EXPORT AwaitTaskSpawnAwaiter
{
    AwaitTaskSpawnAwaiter(AwaitEventLoop& await, AwaitTask& task);

    AwaitEventLoop& await;
    AwaitTask&      task;
    Result          operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle continuation;
};

/// @brief Awaiter that waits for a child task, cancelling it if a timeout expires first.
struct SC_AWAIT_EXPORT AwaitTaskTimeoutAwaiter
{
    AwaitTaskTimeoutAwaiter(AwaitEventLoop& await, AwaitTask& task, TimeMs timeout, AwaitTimeoutResult* outResult);

    AwaitEventLoop&     await;
    AwaitTask&          task;
    TimeMs              timeout;
    AwaitTimeoutResult* outResult = nullptr;
    AsyncLoopTimeout    timeoutRequest;
    Result              operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);
    static void   onTaskCompleted(void* object);

    Result cancel(AwaitEventLoop& eventLoop);
    void   onTaskCompleted();
    void   finish(Result result);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
    bool                         cancelling     = false;
    bool                         childCompleted = false;
    bool                         timeoutStopped = false;
    bool                         timeoutFired   = false;
    bool                         finished       = false;
};

/// @brief Awaiter for a single AsyncLoopWork operation.
struct SC_AWAIT_EXPORT AwaitLoopWorkAwaiter
{
    AwaitLoopWorkAwaiter(AwaitEventLoop& await, ThreadPool& threadPool, Function<Result()> work);

    AwaitEventLoop&    await;
    ThreadPool&        threadPool;
    Function<Result()> work;
    AsyncLoopWork      request;
    Result             operationResult = Result(true);

    bool   await_ready() const;
    bool   await_suspend(AwaitTask::Handle continuation);
    Result await_resume();

  private:
    static Result cancel(void* object, AwaitEventLoop& eventLoop);

    Result cancel(AwaitEventLoop& eventLoop);

    AwaitTask::Handle            continuation;
    Function<void(AsyncResult&)> stopCallback;
};
} // namespace SC
//! @}
