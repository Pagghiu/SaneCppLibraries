// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Containers/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Function.h"
#include "../Foundation/OpaqueObject.h"
#include "../Foundation/Span.h"
#include "../Threading/Atomic.h"
#include "../Time/Time.h"

// Descriptors
#include "../File/FileDescriptor.h"
#include "../Process/ProcessDescriptor.h"
#include "../Socket/SocketDescriptor.h"

//! @defgroup group_async Async
//! @copybrief library_async (see @ref library_async for more details)
//! Async is a multi-platform / event-driven asynchronous I/O library.
//!
/// It exposes async programming model for common IO operations like reading / writing to / from a file or tcp socket.
///
/// Synchronous I/O operations could block the current thread of execution for an undefined amount of time, making it
/// difficult to scale an application to a large number of concurrent operations, or to coexist with other even loop,
/// like for example a GUI event loop. Such async programming model uses a common pattern, where the call fills an
/// AsyncRequest with the required data. The AsyncRequest is added to an AsyncEventLoop that will queue the request to
/// some low level OS IO queue. The event loop can then monitor all the requests in a single call to
/// SC::AsyncEventLoop::run, SC::AsyncEventLoop::runOnce or SC::AsyncEventLoop::runNoWait. These three different run
/// methods cover different integration use cases of the event loop inside of an applications.
///
/// The kernel Async API used on each operating systems are the following:
/// - `IOCP` on Windows
/// - `kqueue` on macOS
/// - `epoll` on Linux
/// - `io_uring` on Linux (experimental).
///
/// Note: To enable `io_uring` backend set `SC_ASYNC_USE_IO_URING=1` and link `liburing` (`-luring`).
///
namespace SC
{
struct EventObject;
struct AsyncEventLoop;

struct AsyncRequest;
struct AsyncResult;
template <typename T>
struct AsyncResultOf;
} // namespace SC

namespace SC
{
namespace detail
{
struct AsyncWinOverlapped;
struct AsyncWinOverlappedDefinition
{
    static constexpr int    Windows   = sizeof(void*) * 7;
    static constexpr size_t Alignment = alignof(void*);

    using Object = AsyncWinOverlapped;
};
using WinOverlappedOpaque = OpaqueObject<AsyncWinOverlappedDefinition>;

struct AsyncWinWaitDefinition
{
    using Handle                    = FileDescriptor::Handle;  // fd
    static constexpr Handle Invalid = FileDescriptor::Invalid; // invalid fd

    static Result releaseHandle(Handle& waitHandle);
};
struct WinWaitHandle : public UniqueHandle<AsyncWinWaitDefinition>
{
};
} // namespace detail
} // namespace SC

//! @addtogroup group_async
//! @{

/// @brief Base class for all async requests, holding state and type.
/// An async operation is struct derived from AsyncRequest asking for some I/O to be done made to the OS. @n
/// Every async operation has an associated callback that is invoked when the request is fulfilled.
/// If the `start` function returns a valid (non error) Return code, then the user callback will be called both
/// in case of success and in case of any error. @n
/// If the function returns an invalid Return code or if the operation is manually cancelled with
/// SC::AsyncRequest::stop, then the user callback will not be called.
/// @note The memory address of all AsyncRequest derived objects must be stable for the entire duration of a started
/// async request. This means that they can be freed / moved after the user callback is executed.
///
/// Some implementation details:
/// SC::AsyncRequest::state dictates the lifetime of the async request according to a state machine.
///
/// Regular Lifetime of an Async request (called just async in the paragraph below):
///
/// 1. An async that has been started, will be pushed in the submission queue with state == State::Setup.
/// 2. Inside stageSubmission a started async will be do the one time setup (with setupAsync)
/// 3. Inside stageSubmission a Setup or Submitting async will be activated (with activateAsync)
/// 4. If activateAsync is successful, the async becomes state == State::Active.
///     - When this happens, the async is either tracked by the kernel or in one of the linked lists like activeWakeUps
///
/// 5. The Active async can become completed, when the kernel signals its completion (or readiness...):
///      - [default] -> Async is complete and it will be teardown and freed (state == State::Free)
///      - result.reactivateRequest(true) -> Async gets submitted again (state == State::Submitting) (3.)
///
/// Cancellation of an async:
/// An async can be cancelled at any time:
///
/// 1. Async not yet submitted in State::Setup --> it just gets removed from the submission queue
/// 2. Async in submission queue but already setup --> it will receive a teardownAsync
/// 3. Async in Active state (so after setupAsync and activateAsync) --> will receive cancelAsync and teardownAsync
///
/// Any other case is considered an error (trying to cancel an async already being cancelled or being teardown).
struct SC::AsyncRequest
{
    AsyncRequest* next = nullptr;
    AsyncRequest* prev = nullptr;

#if SC_CONFIGURATION_DEBUG
    void setDebugName(const char* newDebugName) { debugName = newDebugName; }
#else
    void setDebugName(const char* newDebugName) { SC_COMPILER_UNUSED(newDebugName); }
#endif

    /// @brief Get the event loop associated with this AsyncRequest
    [[nodiscard]] AsyncEventLoop* getEventLoop() const { return eventLoop; }

    /// @brief Type of async request
    enum class Type : uint8_t
    {
        LoopTimeout,   ///< Request is an AsyncLoopTimeout object
        LoopWakeUp,    ///< Request is an AsyncLoopWakeUp object
        ProcessExit,   ///< Request is an AsyncProcessExit object
        SocketAccept,  ///< Request is an AsyncSocketAccept object
        SocketConnect, ///< Request is an AsyncSocketConnect object
        SocketSend,    ///< Request is an AsyncSocketSend object
        SocketReceive, ///< Request is an AsyncSocketReceive object
        SocketClose,   ///< Request is an AsyncSocketClose object
        FileRead,      ///< Request is an AsyncFileRead object
        FileWrite,     ///< Request is an AsyncFileWrite object
        FileClose,     ///< Request is an AsyncFileClose object
        FilePoll,      ///< Request is an AsyncFilePoll object
    };

    /// @brief Constructs a free async request of given type
    /// @param type Type of this specific request
    AsyncRequest(Type type) : state(State::Free), type(type), eventIndex(-1) {}

    /// Stops the async operation

    /// @brief Ask to stop current async operation
    /// @return `true` if the stop request has been successfully queued
    [[nodiscard]] Result stop();

  protected:
    [[nodiscard]] Result validateAsync();
    [[nodiscard]] Result queueSubmission(AsyncEventLoop& eventLoop);

    static void     updateTime(AsyncEventLoop& loop);
    AsyncEventLoop* eventLoop = nullptr;

  private:
    [[nodiscard]] static const char* TypeToString(Type type);
    enum class State : uint8_t
    {
        Free,       // not in any queue, this can be started with an async.start(...)
        Setup,      // when in submission queue waiting to be setup (after an async.start(...))
        Submitting, // when in submission queue waiting to be activated (after a result.reactivateRequest(true))
        Active,     // when monitored by OS syscall or in activeWakeUps / activeTimeouts queues
        Cancelling, // when in cancellation queue waiting for a cancelAsync (on active async)
        Teardown    // when in cancellation queue waiting for a teardownAsync (on non-active, already setup async)
    };

    friend struct AsyncEventLoop;

    template <typename Lambda>
    [[nodiscard]] static Result applyOnAsync(AsyncRequest& async, Lambda&& lambda);

#if SC_CONFIGURATION_DEBUG
    const char* debugName = "None";
#endif
    State   state;     // 1 byte
    Type    type;      // 1 byte
    int16_t flags = 0; // 2 bytes
    int32_t eventIndex;

    static constexpr int16_t Flag_RegularFile = 1 << 1;
};

/// @brief Base class for all async results
struct SC::AsyncResult
{
    using Type = AsyncRequest::Type;

    /// @brief Constructs an async result from a result
    /// @param res The result of async operation
    AsyncResult(SC::Result&& res) : returnCode(move(res)) {}

    /// @brief Ask the event loop to re-activate this request after it was already completed
    /// @param value  `true` will reactivate the request
    void reactivateRequest(bool value) { shouldBeReactivated = value; }

    /// @brief Check if the returnCode of this result is valid
    [[nodiscard]] const SC::Result& isValid() const { return returnCode; }

  protected:
    friend struct AsyncEventLoop;

    bool       shouldBeReactivated = false;
    SC::Result returnCode;
};

/// @brief Create an async Callback result for a given AsyncRequest-derived class
/// @tparam T Type of the request class associated to this result
template <typename T>
struct SC::AsyncResultOf : public AsyncResult
{
    T& async;
    AsyncResultOf(T& async, AsyncResult&& res) : AsyncResult(move(res)), async(async) {}
};

namespace SC
{
//! @addtogroup group_async
//! @{

/// @brief Starts a Timeout that is invoked after expiration (relative) time has passed.
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncLoopTimeoutSnippet
struct AsyncLoopTimeout : public AsyncRequest
{
    /// @brief Callback result for LoopTimeout
    using Result = AsyncResultOf<AsyncLoopTimeout>;
    AsyncLoopTimeout() : AsyncRequest(Type::LoopTimeout) {}

    /// @brief Starts a Timeout that is invoked after expiration (relative) time has passed.
    /// @param eventLoop The event loop where queuing this async request
    /// @param expiration Relative time in milliseconds from when start is called after which callback will be called
    /// @return Valid Result if the request has been successfully queued
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, Time::Milliseconds expiration);

    /// @brief Relative time since AsyncLoopTimeout::start after which callback will be called
    [[nodiscard]] auto getTimeout() const { return timeout; }

    Function<void(Result&)> callback; ///< Called after given expiration time since AsyncLoopTimeout::start has passed

  private:
    friend struct AsyncEventLoop;
    Time::Milliseconds          timeout; // not needed, but keeping just for debugging
    Time::HighResolutionCounter expirationTime;
};

/// @brief Starts a wake-up operation, allowing threads to execute callbacks on loop thread. @n
/// SC::AsyncLoopWakeUp::callback will be invoked on the thread running SC::AsyncEventLoop::run (or its variations)
/// after SC::AsyncLoopWakeUp::wakeUp has been called.
/// @note There is no guarantee that after calling AsyncLoopWakeUp::start the callback has actually finished execution.
/// An optional SC::EventObject passed to SC::AsyncLoopWakeUp::start can be used for synchronization
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncLoopWakeUpSnippet1
///
/// An EventObject can be wait-ed to synchronize further actions from the thread invoking the wake up request, ensuring
/// that the callback has finished its execution.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncLoopWakeUpSnippet2
struct AsyncLoopWakeUp : public AsyncRequest
{
    /// @brief Callback result for AsyncLoopWakeUp
    using Result = AsyncResultOf<AsyncLoopWakeUp>;
    AsyncLoopWakeUp() : AsyncRequest(Type::LoopWakeUp) {}

    /// @brief Starts a wake up request, that will be fulfilled when an external thread calls AsyncLoopWakeUp::wakeUp.
    /// @param eventLoop The event loop where queuing this async request
    /// @param eventObject Optional EventObject to synchronize external threads waiting until the callback is finished.
    /// @return Valid Result if the request has been successfully queued
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, EventObject* eventObject = nullptr);

    /// Wakes up event loop, scheduling AsyncLoopWakeUp::callback on next AsyncEventLoop::run (or its variations)
    [[nodiscard]] SC::Result wakeUp();

    Function<void(Result&)> callback; ///< Callback called by SC::AsyncEventLoop::run after SC::AsyncLoopWakeUp::wakeUp

  private:
    friend struct AsyncEventLoop;

    EventObject* eventObject = nullptr;
    Atomic<bool> pending     = false;
};

/// @brief Starts a process exit notification request, so that when process is exited the callback will be called. @n
/// @ref library_process library can be used to start a process and obtain the native process handle.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncProcessSnippet
struct AsyncProcessExit : public AsyncRequest
{
    struct Result : public AsyncResultOf<AsyncProcessExit>
    {
        Result(AsyncProcessExit& async, SC::Result&& res) : AsyncResultOf(async, move(res)) {}

        [[nodiscard]] SC::Result moveTo(ProcessDescriptor::ExitStatus& status)
        {
            status = exitStatus;
            return returnCode;
        }

      private:
        friend struct AsyncEventLoop;
        ProcessDescriptor::ExitStatus exitStatus;
    };

    AsyncProcessExit() : AsyncRequest(Type::ProcessExit) {}

    /// @brief Starts a process exit notification request, so that when process is exited the callback will be called.
    /// @param eventLoop The event loop where queuing this async request
    /// @param process Native handle of the process that is being monitored
    /// @return Valid Result if the request has been successfully queued
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, ProcessDescriptor::Handle process);

    Function<void(Result&)> callback; ///< Called when process has exited

  private:
    friend struct AsyncEventLoop;
    ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
    detail::WinWaitHandle       waitHandle;
#elif SC_PLATFORM_LINUX
    FileDescriptor pidFd;
#endif
};

/// @brief Starts a socket accept operation, obtaining a new socket from a listening socket. @n
/// The callback is called with a new socket connected to the given listening endpoint will be returned. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedTCPSocket. @n
/// Alternatively SC::AsyncEventLoop::createAsyncTCPSocket creates and associates the socket to the loop.
/// @note To continue accepting new socket SC::AsyncResult::reactivateRequest must be called.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncSocketAcceptSnippet
struct AsyncSocketAccept : public AsyncRequest
{
    /// @brief Callback result for AsyncSocketAccept
    struct Result : public AsyncResultOf<AsyncSocketAccept>
    {
        Result(AsyncSocketAccept& async, SC::Result&& res) : AsyncResultOf(async, move(res)) {}

        [[nodiscard]] SC::Result moveTo(SocketDescriptor& client)
        {
            SC_TRY(returnCode);
            return client.assign(move(acceptedClient));
        }

      private:
        friend struct AsyncEventLoop;
        SocketDescriptor acceptedClient;
    };
    AsyncSocketAccept() : AsyncRequest(Type::SocketAccept) {}

    /// @brief Starts a socket accept operation, that returns a new socket connected to the given listening endpoint.
    /// @note SocketDescriptor must be created with async flags and already bound and listening.
    /// @param eventLoop The event loop where queuing this async request
    /// @param socketDescriptor The socket that will receive the accepted client.
    /// @return Valid Result if the request has been successfully queued
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    Function<void(Result&)> callback; ///< Called when a new socket has been accepted

  private:
    friend struct AsyncEventLoop;
    SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;

    SocketDescriptor clientSocket;
    uint8_t          acceptBuffer[288];
#elif SC_PLATFORM_LINUX
    AlignedStorage<28> sockAddrHandle;
    uint32_t           sockAddrLen;
#endif
};

/// @brief Starts a socket connect operation, connecting to a remote endpoint. @n
/// Callback will be called when the given socket is connected to ipAddress. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedTCPSocket. @n
/// Alternatively SC::AsyncEventLoop::createAsyncTCPSocket creates and associates the socket to the loop.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncSocketConnectSnippet
struct AsyncSocketConnect : public AsyncRequest
{
    /// @brief Callback result for AsyncSocketConnect
    using Result = AsyncResultOf<AsyncSocketConnect>;

    AsyncSocketConnect() : AsyncRequest(Type::SocketConnect) {}

    /// @brief Starts a socket connect operation.
    /// Callback will be called when the given socket is connected to ipAddress.
    /// @param eventLoop The event loop where queuing this async request
    /// @param socketDescriptor The socket needing to connect to the ip address
    /// @param ipAddress A valid ip address to connect to
    /// @return Valid Result if the request has been successfully queued
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor,
                                   SocketIPAddress ipAddress);

    Function<void(Result&)> callback; ///< Called after socket is finally connected to endpoint

  private:
    friend struct AsyncEventLoop;
    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    SocketIPAddress          ipAddress;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};

/// @brief Starts a socket send operation, sending bytes to a remote endpoint.
/// Callback will be called when the given socket is ready to send more data. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedTCPSocket or though AsyncSocketAccept. @n
/// Alternatively SC::AsyncEventLoop::createAsyncTCPSocket creates and associates the socket to the loop.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncSocketSendSnippet
struct AsyncSocketSend : public AsyncRequest
{
    /// @brief Callback result for AsyncSocketSend
    using Result = AsyncResultOf<AsyncSocketSend>;
    AsyncSocketSend() : AsyncRequest(Type::SocketSend) {}

    /// @brief Starts a socket send operation.
    /// Callback will be called when the given socket is ready to send more data.
    /// @param eventLoop The event loop where queuing this async request
    /// @param socketDescriptor The socket to send data to
    /// @param data The data to be sent
    /// @return Valid Result if the request has been successfully queued
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor,
                                   Span<const char> data);

    Function<void(Result&)> callback; ///< Called when socket is ready to send more data.

  private:
    friend struct AsyncEventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<const char>         data;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
struct AsyncSocketReceive;

/// @brief Starts a socket receive operation, receiving bytes from a remote endpoint.
/// Callback will be called when some data is read from socket. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedTCPSocket or though AsyncSocketAccept. @n
/// Alternatively SC::AsyncEventLoop::createAsyncTCPSocket creates and associates the socket to the loop.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncSocketReceiveSnippet
struct AsyncSocketReceive : public AsyncRequest
{
    /// @brief Callback result for AsyncSocketReceive
    struct Result : public AsyncResultOf<AsyncSocketReceive>
    {
        Result(AsyncSocketReceive& async, SC::Result&& res) : AsyncResultOf(async, move(res)) {}

        /// @brief Get a Span of the actually read data
        /// @param outData The span of data actually read from socket
        /// @return Valid Result if the data was read without errors
        [[nodiscard]] SC::Result moveTo(Span<char>& outData)
        {
            outData = readData;
            return returnCode;
        }

      private:
        friend struct AsyncEventLoop;
        Span<char> readData;
    };

    AsyncSocketReceive() : AsyncRequest(Type::SocketReceive) {}

    /// @brief Starts a socket receive operation.
    /// Callback will be called when some data is read from socket.
    /// @param eventLoop The event loop where queuing this async request
    /// @param socketDescriptor The socket from which to receive data
    /// @param data Span of memory where to write received bytes
    /// @return Valid Result if the request has been successfully queued
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor,
                                   Span<char> data);

    Function<void(Result&)> callback; ///< Called after data has been received

  private:
    friend struct AsyncEventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<char>               data;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};

/// @brief Starts a socket close operation.
/// Callback will be called when the socket has been fully closed.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncSocketCloseSnippet
struct AsyncSocketClose : public AsyncRequest
{
    /// @brief Callback result for AsyncSocketClose
    using Result = AsyncResultOf<AsyncSocketClose>;

    AsyncSocketClose() : AsyncRequest(Type::SocketClose) {}

    /// @brief Starts a socket close operation.
    /// Callback will be called when the socket has been fully closed.
    /// @param eventLoop The event loop where queuing this async request
    /// @param socketDescriptor The socket to be closed
    /// @return Valid Result if the request has been successfully queued
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    int                     code = 0; ///< Return code of close socket operation
    Function<void(Result&)> callback; ///< Callback called after fully closing the socket

  private:
    friend struct AsyncEventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
};

/// @brief Starts a file read operation, reading bytes from a file.
/// Callback will be called after some data is read from file. @n
/// @ref library_file library can be used to open the file and obtain a file descriptor handle.
/// Make sure to associate the file descriptor with SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncFileReadSnippet
struct AsyncFileRead : public AsyncRequest
{
    /// @brief Callback result for AsyncFileRead
    struct Result : public AsyncResultOf<AsyncFileRead>
    {
        Result(AsyncFileRead& async, SC::Result&& res) : AsyncResultOf(async, move(res)) {}

        [[nodiscard]] SC::Result moveTo(Span<char>& data)
        {
            data = readData;
            return returnCode;
        }

      private:
        friend struct AsyncEventLoop;
        Span<char> readData;
    };

    AsyncFileRead() : AsyncRequest(Type::FileRead) {}
    /// Starts a file receive operation, that will return when some data is read from file.
    [[nodiscard]] SC::Result start(AsyncEventLoop& loop, FileDescriptor::Handle fileDescriptor, Span<char> readBuffer);

    uint64_t offset = 0;

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;

    FileDescriptor::Handle fileDescriptor;
    Span<char>             readBuffer;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#elif SC_PLATFORM_LINUX
    size_t syncReadBytes = 0;
#endif
};

/// @brief Starts a file write operation, writing bytes to a file.
/// Callback will be called when the file is ready to receive more bytes to write. @n
/// @ref library_file library can be used to open the file and obtain a file descriptor handle. @n
/// Make sure to associate the file descriptor with SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncFileWriteSnippet
struct AsyncFileWrite : public AsyncRequest
{
    /// @brief Callback result for AsyncFileWrite
    struct Result : public AsyncResultOf<AsyncFileWrite>
    {
        Result(AsyncFileWrite& async, SC::Result&& res) : AsyncResultOf(async, move(res)) {}

        [[nodiscard]] SC::Result moveTo(size_t& writtenSizeInBytes)
        {
            writtenSizeInBytes = writtenBytes;
            return returnCode;
        }

      private:
        friend struct AsyncEventLoop;
        size_t writtenBytes = 0;
    };

    AsyncFileWrite() : AsyncRequest(Type::FileWrite) {}

    /// Starts a file receive operation, that will return when the file is ready to receive more bytes to write.
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, FileDescriptor::Handle fileDescriptor,
                                   Span<const char> writeBuffer);

    uint64_t offset = 0;

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;
    FileDescriptor::Handle fileDescriptor;
    Span<const char>       writeBuffer;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#elif SC_PLATFORM_LINUX
    size_t syncWrittenBytes = 0;
#endif
};

/// @brief Starts a file close operation, closing the OS file descriptor.
/// Callback will be called when the file is actually closed. @n
/// @ref library_file library can be used to open the file and obtain a file descriptor handle.
///
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncFileCloseSnippet
struct AsyncFileClose : public AsyncRequest
{
    using Result = AsyncResultOf<AsyncFileClose>;
    AsyncFileClose() : AsyncRequest(Type::FileClose) {}

    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, FileDescriptor::Handle fileDescriptor);

    int code = 0;

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;
    FileDescriptor::Handle fileDescriptor;
};

/// @brief Starts an handle polling operation.
/// Uses `GetOverlappedResult` (windows), `kevent` (macOS), `epoll` (Linux) and `io_uring` (Linux).  
/// Callback will be called when any of the three API signals readiness events on the given file descriptor.  
/// Check @ref library_file_system_watcher for an example usage of this notification.
struct AsyncFilePoll : public AsyncRequest
{
    using Result = AsyncResultOf<AsyncFilePoll>;
    AsyncFilePoll() : AsyncRequest(Type::FilePoll) {}

    /// Starts a file descriptor poll operation, monitoring its readiness with appropriate OS API
    [[nodiscard]] SC::Result start(AsyncEventLoop& loop, FileDescriptor::Handle fileDescriptor);

#if SC_PLATFORM_WINDOWS
    [[nodiscard]] auto& getOverlappedOpaque() { return overlapped; }
#endif

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;

    FileDescriptor::Handle fileDescriptor;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};

//! @}

} // namespace SC

/// @brief Asynchronous I/O (files, sockets, timers, processes, fs events, threads wake-up) (see @ref library_async)
/// AsyncEventLoop pushes all AsyncRequest derived classes to I/O queues in the OS.
/// Basic lifetime for an event loop is:
/// \snippet Libraries/Async/Tests/AsyncTest.cpp AsyncEventLoopSnippet
struct SC::AsyncEventLoop
{
    /// Creates the event loop kernel object
    [[nodiscard]] Result create();

    /// Closes the event loop kernel object
    [[nodiscard]] Result close();

    /// Blocks until there are no more active queued requests.
    /// It's useful for applications where the eventLoop is the only (or the main) loop.
    /// One example could be a console based app doing socket IO or a web server.
    /// Waiting on requests blocks the current thread with 0% CPU utilization.
    [[nodiscard]] Result run();

    /// Blocks until at least one request proceeds, ensuring forward progress.
    /// It's useful for applications where the eventLoop events needs to be interleaved with other work.
    /// For example one possible way of integrating with a UI event loop could be to schedule a recurrent timeout
    /// timer every 1/60 seconds where calling GUI event loop  updates every 60 seconds, blocking for I/O for
    /// the remaining time. Waiting on requests blocks the current thread with 0% CPU utilization.
    [[nodiscard]] Result runOnce();

    /// Process active requests if they exist or returns immediately without blocking.
    /// It's useful for game-like applications where the event loop runs every frame and one would like to check
    /// and dispatch its I/O callbacks in-between frames.
    /// This call allows poll-checking I/O without blocking.
    [[nodiscard]] Result runNoWait();

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked).
    /// The parameter is an AsyncLoopWakeUp that must have been previously started (with AsyncLoopWakeUp::start).
    [[nodiscard]] Result wakeUpFromExternalThread(AsyncLoopWakeUp& wakeUp);

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked)
    [[nodiscard]] Result wakeUpFromExternalThread();

    /// Helper to creates a TCP socket with AsyncRequest flags of the given family (IPV4 / IPV6).
    /// It also automatically registers the socket with the eventLoop (associateExternallyCreatedTCPSocket)
    [[nodiscard]] Result createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor);

    /// Associates a TCP Socket created externally (without using createAsyncTCPSocket) with the eventLoop.
    [[nodiscard]] Result associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor);

    /// Associates a File descriptor created externally with the eventLoop.
    [[nodiscard]] Result associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor);

#if SC_PLATFORM_WINDOWS
    /// Returns handle to the kernel level IO queue object.
    /// Used by external systems calling OS async function themselves (FileSystemWatcher on windows for example)
    [[nodiscard]] Result getLoopFileDescriptor(FileDescriptor::Handle& fileDescriptor) const;
#endif
    /// Get Loop time
    [[nodiscard]] Time::HighResolutionCounter getLoopTime() const { return loopTime; }

  private:
    int numberOfActiveHandles = 0;
    int numberOfExternals     = 0;

    IntrusiveDoubleLinkedList<AsyncRequest> submissions;
    IntrusiveDoubleLinkedList<AsyncRequest> activeTimers;
    IntrusiveDoubleLinkedList<AsyncRequest> activeWakeUps;
    IntrusiveDoubleLinkedList<AsyncRequest> manualCompletions;

#if SC_PLATFORM_LINUX
    IntrusiveDoubleLinkedList<AsyncProcessExit> activeProcessChild;
#endif

    Time::HighResolutionCounter loopTime;

    struct KernelQueue;

    struct Internal;
    struct InternalDefinition
    {
        static constexpr int Windows = 160;
        static constexpr int Apple   = 88;
        static constexpr int Default = 304;

        static constexpr size_t Alignment = alignof(void*);

        using Object = Internal;
    };

  public:
    using InternalOpaque = OpaqueObject<InternalDefinition>;

  private:
    InternalOpaque internal;

    [[nodiscard]] int getTotalNumberOfActiveHandle() const;

    void removeActiveHandle(AsyncRequest& async);
    void addActiveHandle(AsyncRequest& async);
    void scheduleManualCompletion(AsyncRequest& async);
    void increaseActiveCount();
    void decreaseActiveCount();

    // Timers
    [[nodiscard]] const Time::HighResolutionCounter* findEarliestTimer() const;

    void invokeExpiredTimers();
    void updateTime();
    void executeTimers(KernelQueue& queue, const Time::HighResolutionCounter& nextTimer);

    [[nodiscard]] Result cancelAsync(AsyncRequest& async);

    // LoopWakeUp
    void executeWakeUps(AsyncResult& result);

    // Setup
    [[nodiscard]] Result queueSubmission(AsyncRequest& async);

    // Phases
    [[nodiscard]] Result stageSubmission(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result setupAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result teardownAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result activateAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result cancelAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result completeAsync(KernelQueue& queue, AsyncRequest& async, Result&& returnCode, bool& reactivate);

    [[nodiscard]] Result completeAndEventuallyReactivate(KernelQueue& queue, AsyncRequest& async, Result&& returnCode);

    void reportError(KernelQueue& queue, AsyncRequest& async, Result&& returnCode);

    enum class SyncMode
    {
        NoWait,
        ForcedForwardProgress
    };

    [[nodiscard]] Result runStep(SyncMode syncMode);

    void runStepExecuteCompletions(KernelQueue& queue);
    void runStepExecuteManualCompletions(KernelQueue& queue);

    friend struct AsyncRequest;
};

//! @}
