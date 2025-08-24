// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Async/Internal/IntrusiveDoubleLinkedList.h"
#include "../File/File.h"
#include "../FileSystem/FileSystem.h"
#include "../Foundation/Function.h"
#include "../Foundation/OpaqueObject.h"
#include "../Socket/Socket.h"
#include "../Threading/Atomic.h"
#include "../Threading/ThreadPool.h"
#include "../Time/Time.h"

namespace SC
{
struct ThreadPool;
struct ThreadPoolTask;
struct EventObject;
} // namespace SC
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
/// - `io_uring` on Linux (dynamically loading `liburing`)
///
/// @note If `liburing` is not available on the system, the library will transparently fallback to epoll.
///
/// If an async operation is not supported by the OS, the caller can provide a SC::ThreadPool to run it on a thread.
/// See SC::AsyncFileRead / SC::AsyncFileWrite for an example.

//! @addtogroup group_async
//! @{
namespace SC
{
struct AsyncEventLoop;
struct AsyncResult;
struct AsyncSequence;
struct AsyncTaskSequence;

namespace detail
{
struct AsyncWinOverlapped;
struct AsyncWinOverlappedDefinition
{
    static constexpr int    Windows   = sizeof(void*) * 4 + sizeof(uint64_t);
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

/// @brief Base class for all async requests, holding state and type.
/// An async operation is struct derived from AsyncRequest asking for some I/O to be done made to the OS. @n
/// Every async operation has an associated callback that is invoked when the request is fulfilled.
/// If the `start` function returns a valid (non error) Return code, then the user callback will be called both
/// in case of success and in case of any error. @n
/// If the function returns an invalid Return code or if the operation is manually cancelled with
/// SC::AsyncRequest::stop, then the user callback will not be called.
///
/// @note The memory address of all AsyncRequest derived objects must be stable until user callback is executed.
/// - If request is not re-activated (i.e. `result.reactivateRequest(true)` **is NOT** called) then the async request
/// can be freed as soon as the user callback is called (even inside the callback itself).
/// - If request is re-activated (i.e. `result.reactivateRequest(true)` **is** called) then the async cannot be freed
/// as it's still in use.
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
///     - When this happens, the async is either tracked by the kernel or in one of the linked lists like
///     activeLoopWakeUps
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
struct AsyncRequest
{
    AsyncRequest* next = nullptr;
    AsyncRequest* prev = nullptr;

    void setDebugName(const char* newDebugName);

    /// @brief Adds the request to be executed on a specific AsyncSequence
    void executeOn(AsyncSequence& sequence);

    /// @brief Adds the request to be executed on a specific AsyncTaskSequence
    /// @see AsyncFileRead
    /// @see AsyncFileWrite
    Result executeOn(AsyncTaskSequence& task, ThreadPool& pool);

    /// @brief Disables the thread-pool usage for this request
    void disableThreadPool();

    /// @brief Type of async request
    enum class Type : uint8_t
    {
        LoopTimeout,         ///< Request is an AsyncLoopTimeout object
        LoopWakeUp,          ///< Request is an AsyncLoopWakeUp object
        LoopWork,            ///< Request is an AsyncLoopWork object
        ProcessExit,         ///< Request is an AsyncProcessExit object
        SocketAccept,        ///< Request is an AsyncSocketAccept object
        SocketConnect,       ///< Request is an AsyncSocketConnect object
        SocketSend,          ///< Request is an AsyncSocketSend object
        SocketSendTo,        ///< Request is an SocketSendTo object
        SocketReceive,       ///< Request is an AsyncSocketReceive object
        SocketReceiveFrom,   ///< Request is an SocketReceiveFrom object
        FileRead,            ///< Request is an AsyncFileRead object
        FileWrite,           ///< Request is an AsyncFileWrite object
        FilePoll,            ///< Request is an AsyncFilePoll object
        FileSystemOperation, ///< Request is an AsyncFileSystemOperation object
    };

    /// @brief Constructs a free async request of given type
    /// @param type Type of this specific request
    AsyncRequest(Type type) : state(State::Free), type(type), flags(0), unused(0), userFlags(0) {}

    /// @brief Ask to stop current async operation
    /// @param eventLoop The event Loop associated with this request
    /// @param afterStopped Optional pointer to a callback called after request is fully stopped.
    /// @return `true` if the stop request has been successfully queued
    /// @note When stopping the request must be valid until afterStopped will be called.
    /// This AsyncRequest cannot be re-used before this callback will be called.
    Result stop(AsyncEventLoop& eventLoop, Function<void(AsyncResult&)>* afterStopped = nullptr);

    /// @brief Returns `true` if this request is free
    [[nodiscard]] bool isFree() const;

    /// @brief Returns `true` if this request is being cancelled
    [[nodiscard]] bool isCancelling() const;

    /// @brief Returns `true` if this request is active or being reactivated
    [[nodiscard]] bool isActive() const;

    /// @brief Returns request type
    [[nodiscard]] Type getType() const { return type; }

    /// @brief Shortcut for AsyncEventLoop::start
    Result start(AsyncEventLoop& eventLoop);

    /// @brief Sets user flags, holding some meaningful data for the caller
    void setUserFlags(uint16_t externalFlags) { userFlags = externalFlags; }

    /// @brief Gets user flags, holding some meaningful data for the caller
    uint16_t getUserFlags() const { return userFlags; }

    /// @brief Returns currently set close callback (if any) passed to AsyncRequest::stop
    [[nodiscard]] Function<void(AsyncResult&)>* getCloseCallback() { return closeCallback; }

    [[nodiscard]] const Function<void(AsyncResult&)>* getCloseCallback() const { return closeCallback; }

  protected:
    Result checkState();

    void queueSubmission(AsyncEventLoop& eventLoop);

    AsyncSequence* sequence = nullptr;

    AsyncTaskSequence* getTask();

  private:
    Function<void(AsyncResult&)>* closeCallback = nullptr;

    friend struct AsyncEventLoop;
    friend struct AsyncResult;

    void markAsFree();

    [[nodiscard]] static const char* TypeToString(Type type);
    enum class State : uint8_t
    {
        Free,       // not in any queue, this can be started with an async.start(...)
        Setup,      // when in submission queue waiting to be setup (after an async.start(...))
        Submitting, // when in submission queue waiting to be activated or re-activated
        Active,     // when monitored by OS syscall or in activeLoopWakeUps / activeTimeouts queues
        Reactivate, // when flagged for reactivation inside the callback (after a result.reactivateRequest(true))
        Cancelling, // when in cancellation queue waiting for a cancelAsync (on active async)
    };

#if SC_ASYNC_ENABLE_LOG
    const char* debugName = "None";
#endif
    State   state; // 1 byte
    Type    type;  // 1 byte
    int16_t flags; // 2 bytes

    uint16_t unused;    // 2 bytes
    uint16_t userFlags; // 2 bytes
};

/// @brief Execute AsyncRequests serially, by submitting the next one after the previous one is completed.
/// Requests are being queued on a sequence using AsyncRequest::executeOn.
/// AsyncTaskSequence can be used to force running asyncs on a thread (useful for buffered files)
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncFileWriteSnippet
struct AsyncSequence
{
    AsyncSequence* next = nullptr;
    AsyncSequence* prev = nullptr;

    bool clearSequenceOnCancel = true; ///< Do not queue next requests in the sequence when current one is cancelled
    bool clearSequenceOnError  = true; ///< Do not queue next requests in the sequence when current one returns error
  private:
    friend struct AsyncEventLoop;
    bool runningAsync = false; // true if an async from this sequence is being run
    bool tracked      = false;

    IntrusiveDoubleLinkedList<AsyncRequest> submissions;
};

/// @brief Empty base struct for all AsyncRequest-derived CompletionData (internal) structs.
struct AsyncCompletionData
{
};

/// @brief Base class for all async results (argument of completion callbacks).
/// It holds Result (returnCode) and re-activation flag.
struct AsyncResult
{
    /// @brief Constructs an async result from a request and a result
    AsyncResult(AsyncEventLoop& eventLoop, AsyncRequest& request, SC::Result& res, bool* hasBeenReactivated = nullptr)
        : eventLoop(eventLoop), async(request), hasBeenReactivated(hasBeenReactivated), returnCode(res)
    {}

    /// @brief Ask the event loop to re-activate this request after it was already completed
    /// @param shouldBeReactivated `true` will reactivate the request
    void reactivateRequest(bool shouldBeReactivated);

    /// @brief Check if the returnCode of this result is valid
    [[nodiscard]] const SC::Result& isValid() const { return returnCode; }

    AsyncEventLoop& eventLoop;
    AsyncRequest&   async;

  protected:
    friend struct AsyncEventLoop;

    bool  shouldCallCallback = true;
    bool* hasBeenReactivated = nullptr;

    SC::Result& returnCode;
};

/// @brief Helper holding CompletionData for a specific AsyncRequest-derived class
/// @tparam T Type of the request class associated to this result
/// @tparam C Type of the CompletionData derived class associated to this result
template <typename T, typename C>
struct AsyncResultOf : public AsyncResult
{
    T&       getAsync() { return static_cast<T&>(AsyncResult::async); }
    const T& getAsync() const { return static_cast<const T&>(AsyncResult::async); }

    using AsyncResult::AsyncResult;

    C       completionData;
    int32_t eventIndex = 0;
};

/// @brief Starts a Timeout that is invoked only once after expiration (relative) time has passed.
/// @note For a periodic timeout, call AsyncLoopTimeout::Result::reactivateRequest(true) in the completion callback
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncLoopTimeoutSnippet
struct AsyncLoopTimeout : public AsyncRequest
{
    AsyncLoopTimeout() : AsyncRequest(Type::LoopTimeout) {}

    using CompletionData = AsyncCompletionData;
    using Result         = AsyncResultOf<AsyncLoopTimeout, CompletionData>;
    using AsyncRequest::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, Time::Milliseconds relativeTimeout);

    Function<void(Result&)> callback; ///< Called after given expiration time since AsyncLoopTimeout::start has passed

    Time::Milliseconds relativeTimeout; ///< First timer expiration (relative) time in milliseconds

    /// @brief Gets computed absolute expiration time that determines when this timeout get executed
    Time::Absolute getExpirationTime() const { return expirationTime; }

  private:
    SC::Result validate(AsyncEventLoop&);
    friend struct AsyncEventLoop;
    Time::Absolute expirationTime;
};

/// @brief Starts a wake-up operation, allowing threads to execute callbacks on loop thread. @n
/// SC::AsyncLoopWakeUp::callback will be invoked on the thread running SC::AsyncEventLoop::run (or its variations)
/// after SC::AsyncLoopWakeUp::wakeUp has been called.
/// @note There is no guarantee that after calling AsyncLoopWakeUp::start the callback has actually finished execution.
/// An optional SC::EventObject passed to SC::AsyncLoopWakeUp::start can be used for synchronization
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncLoopWakeUpSnippet1
///
/// An EventObject can be wait-ed to synchronize further actions from the thread invoking the wake up request, ensuring
/// that the callback has finished its execution.
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncLoopWakeUpSnippet2
struct AsyncLoopWakeUp : public AsyncRequest
{
    AsyncLoopWakeUp() : AsyncRequest(Type::LoopWakeUp) {}

    using CompletionData = AsyncCompletionData;
    using Result         = AsyncResultOf<AsyncLoopWakeUp, CompletionData>;
    using AsyncRequest::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, EventObject& eventObject);

    /// Wakes up event loop, scheduling AsyncLoopWakeUp::callback on next AsyncEventLoop::run (or its variations)
    SC::Result wakeUp(AsyncEventLoop& eventLoop);

    Function<void(Result&)> callback; ///< Callback called by SC::AsyncEventLoop::run after SC::AsyncLoopWakeUp::wakeUp
    EventObject* eventObject = nullptr; ///< Optional EventObject to let external threads wait for the callback to end

  private:
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);

    Atomic<bool> pending = false;
};

/// @brief Starts monitoring a process, notifying about its termination.
/// @ref library_process library can be used to start a process and obtain the native process handle.
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncProcessSnippet
struct AsyncProcessExit : public AsyncRequest
{
    AsyncProcessExit() : AsyncRequest(Type::ProcessExit) {}

    struct CompletionData : public AsyncCompletionData
    {
        int exitStatus;
    };

    struct Result : public AsyncResultOf<AsyncProcessExit, CompletionData>
    {
        using AsyncResultOf<AsyncProcessExit, CompletionData>::AsyncResultOf;

        SC::Result get(int& status)
        {
            status = completionData.exitStatus;
            return returnCode;
        }
    };
    using AsyncRequest::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    /// @note Using FileDescriptor::Handle instead of ProcessDescriptor::Handle not to depend on
    /// Process library because their low-level native representation are the same.
    SC::Result start(AsyncEventLoop& eventLoop, FileDescriptor::Handle process);

    Function<void(Result&)> callback; ///< Called when process has exited

  private:
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
    detail::WinWaitHandle       waitHandle;
    AsyncEventLoop*             eventLoop = nullptr;
#elif SC_PLATFORM_LINUX
    FileDescriptor pidFd;
#endif
};

struct AsyncSocketAccept;
namespace detail
{
/// @brief Support object to Socket Accept.
/// This has been split out of AsyncSocketAccept because on Windows it's quite big to allow heap allocating it
struct AsyncSocketAcceptData
{
#if SC_PLATFORM_WINDOWS
    void (*pAcceptEx)() = nullptr;
    detail::WinOverlappedOpaque overlapped;
    SocketDescriptor            clientSocket;
    uint8_t                     acceptBuffer[288] = {0};
#elif SC_PLATFORM_LINUX
    AlignedStorage<28> sockAddrHandle;
    uint32_t           sockAddrLen;
#endif
};

/// @brief Base class for AsyncSocketAccept to allow dynamically allocating AsyncSocketAcceptData
struct AsyncSocketAcceptBase : public AsyncRequest
{
    AsyncSocketAcceptBase() : AsyncRequest(Type::SocketAccept) {}

    struct CompletionData : public AsyncCompletionData
    {
        SocketDescriptor acceptedClient;
    };

    struct Result : public AsyncResultOf<AsyncSocketAccept, CompletionData>
    {
        using AsyncResultOf<AsyncSocketAccept, CompletionData>::AsyncResultOf;

        SC::Result moveTo(SocketDescriptor& client)
        {
            SC_TRY(returnCode);
            return client.assign(move(completionData.acceptedClient));
        }
    };
    using AsyncRequest::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor, AsyncSocketAcceptData& data);
    SC::Result validate(AsyncEventLoop&);

    Function<void(Result&)>    callback; ///< Called when a new socket has been accepted
    SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
    AsyncSocketAcceptData*     acceptData    = nullptr;
};

} // namespace detail

/// @brief Starts a socket accept operation, obtaining a new socket from a listening socket. @n
/// The callback is called with a new socket connected to the given listening endpoint will be returned. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedSocket. @n
/// Alternatively SC::AsyncEventLoop::createAsyncTCPSocket creates and associates the socket to the loop.
/// @note To continue accepting new socket SC::AsyncResult::reactivateRequest must be called.
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncSocketAcceptSnippet
struct AsyncSocketAccept : public detail::AsyncSocketAcceptBase
{
    AsyncSocketAccept() { AsyncSocketAcceptBase::acceptData = &data; }
    using AsyncSocketAcceptBase::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

  private:
    detail::AsyncSocketAcceptData data;
};

/// @brief Starts a socket connect operation, connecting to a remote endpoint. @n
/// Callback will be called when the given socket is connected to ipAddress. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedSocket. @n
/// Alternatively SC::AsyncEventLoop::createAsyncTCPSocket creates and associates the socket to the loop.
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncSocketConnectSnippet
struct AsyncSocketConnect : public AsyncRequest
{
    AsyncSocketConnect() : AsyncRequest(Type::SocketConnect) {}

    using CompletionData = AsyncCompletionData;
    using Result         = AsyncResultOf<AsyncSocketConnect, CompletionData>;
    using AsyncRequest::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor, SocketIPAddress address);

    Function<void(Result&)> callback; ///< Called after socket is finally connected to endpoint

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    SocketIPAddress          ipAddress;

  private:
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);

#if SC_PLATFORM_WINDOWS
    void (*pConnectEx)() = nullptr;
    detail::WinOverlappedOpaque overlapped;
#endif
};

/// @brief Starts a socket send operation, sending bytes to a remote endpoint.
/// Callback will be called when the given socket is ready to send more data. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedSocket or though AsyncSocketAccept. @n
/// Alternatively SC::AsyncEventLoop::createAsyncTCPSocket creates and associates the socket to the loop.
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncSocketSendSnippet
struct AsyncSocketSend : public AsyncRequest
{
    AsyncSocketSend() : AsyncRequest(Type::SocketSend) {}
    struct CompletionData : public AsyncCompletionData
    {
        size_t numBytes = 0;
    };
    using Result = AsyncResultOf<AsyncSocketSend, CompletionData>;
    using AsyncRequest::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor, Span<const char> data);

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor, Span<Span<const char>> data);

    Function<void(Result&)> callback; ///< Called when socket is ready to send more data.

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid; ///< The socket to send data to

    Span<const char>       buffer;              ///< Span of bytes to send (singleBuffer == true)
    Span<Span<const char>> buffers;             ///< Spans of bytes to send (singleBuffer == false)
    bool                   singleBuffer = true; ///< Controls if buffer or buffers will be used

  protected:
    AsyncSocketSend(Type type) : AsyncRequest(type) {}
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);

    size_t totalBytesWritten = 0;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};

/// @brief Starts an unconnected socket send to operation, sending bytes to a remote endpoint.
/// Callback will be called when the given socket is ready to send more data. @n
/// Typical use case is to send data to an unconnected UDP socket. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedSocket or though AsyncSocketAccept. @n
/// Alternatively SC::AsyncEventLoop::createAsyncUDPSocket creates and associates the socket to the loop.
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncSocketSendToSnippet
struct AsyncSocketSendTo : public AsyncSocketSend
{
    AsyncSocketSendTo() : AsyncSocketSend(Type::SocketSendTo) {}

    SocketIPAddress address;

    SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor, SocketIPAddress ipAddress,
                     Span<const char> data);

    SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor, SocketIPAddress ipAddress,
                     Span<Span<const char>> data);

  private:
    using AsyncSocketSend::start;
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);
#if SC_PLATFORM_LINUX
    AlignedStorage<56> typeErasedMsgHdr;
#endif
};

/// @brief Starts a socket receive operation, receiving bytes from a remote endpoint.
/// Callback will be called when some data is read from socket. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedSocket or though AsyncSocketAccept. @n
/// Alternatively SC::AsyncEventLoop::createAsyncTCPSocket creates and associates the socket to the loop.
///
/// Additional notes:
/// - SC::AsyncSocketReceive::CompletionData::disconnected will be set to true when client disconnects
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncSocketReceiveSnippet
struct AsyncSocketReceive : public AsyncRequest
{
    AsyncSocketReceive() : AsyncRequest(Type::SocketReceive) {}

    struct CompletionData : public AsyncCompletionData
    {
        size_t numBytes     = 0;
        bool   disconnected = false;
    };

    struct Result : public AsyncResultOf<AsyncSocketReceive, CompletionData>
    {
        using AsyncResultOf<AsyncSocketReceive, CompletionData>::AsyncResultOf;

        /// @brief Get a Span of the actually read data
        /// @param outData The span of data actually read from socket
        /// @return Valid Result if the data was read without errors
        SC::Result get(Span<char>& outData)
        {
            SC_TRY(getAsync().buffer.sliceStartLength(0, completionData.numBytes, outData));
            return returnCode;
        }

        SocketIPAddress getSourceAddress() const;
    };
    using AsyncRequest::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& descriptor, Span<char> data);

    Function<void(Result&)> callback; ///< Called after data has been received

    Span<char>               buffer; ///< The writeable span of memory where to data will be written
    SocketDescriptor::Handle handle = SocketDescriptor::Invalid; /// The Socket Descriptor handle to read data from.

  protected:
    AsyncSocketReceive(Type type) : AsyncRequest(type) {}
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};

/// @brief Starts an unconnected socket receive from operation, receiving bytes from a remote endpoint.
/// Callback will be called when some data is read from socket. @n
/// Typical use case is to receive data from an unconnected UDP socket. @n
/// @ref library_socket library can be used to create a Socket but the socket should be created with
/// SC::SocketFlags::NonBlocking and associated to the event loop with
/// SC::AsyncEventLoop::associateExternallyCreatedSocket or though AsyncSocketAccept. @n
/// Alternatively SC::AsyncEventLoop::createAsyncUDPSocket creates and associates the socket to the loop.
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncSocketReceiveFromSnippet
struct AsyncSocketReceiveFrom : public AsyncSocketReceive
{
    AsyncSocketReceiveFrom() : AsyncSocketReceive(Type::SocketReceiveFrom) {}
    using AsyncSocketReceive::start;

  private:
    SocketIPAddress address;
    friend struct AsyncSocketReceive;
    friend struct AsyncEventLoop;
#if SC_PLATFORM_LINUX
    AlignedStorage<56> typeErasedMsgHdr;
#endif
};

/// @brief Starts a file read operation, reading bytes from a file (or pipe).
/// Callback will be called when the data read from the file (or pipe) is available. @n
///
/// Call AsyncRequest::executeOn to set a thread pool if this is a buffered file and not a pipe.
/// This is important on APIs with blocking behaviour on buffered file I/O (all apis with the exception of `io_uring`).
///
/// @ref library_file library can be used to open the file and obtain a file (or pipe) descriptor handle.
///
/// @note Pipes or files opened using Posix `O_DIRECT` or Windows `FILE_FLAG_WRITE_THROUGH` & `FILE_FLAG_NO_BUFFERING`
/// should instead avoid using the `Task` parameter for best performance.
///
/// When not using the `Task` remember to:
/// - Open the file descriptor for non-blocking IO (SC::File::OpenOptions::blocking == `false`)
/// - Call SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor on the file descriptor
///
/// Additional notes:
/// - When reactivating the AsyncRequest, remember to increment the offset (SC::AsyncFileRead::offset)
/// - SC::AsyncFileRead::CompletionData::endOfFile signals end of file reached
/// - `io_uring` backend will not use thread pool because that API allows proper async file read/writes
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncFileReadSnippet
struct AsyncFileRead : public AsyncRequest
{
    AsyncFileRead() : AsyncRequest(Type::FileRead) { handle = FileDescriptor::Invalid; }

    struct CompletionData : public AsyncCompletionData
    {
        size_t numBytes  = 0;
        bool   endOfFile = false;
    };

    struct Result : public AsyncResultOf<AsyncFileRead, CompletionData>
    {
        using AsyncResultOf<AsyncFileRead, CompletionData>::AsyncResultOf;

        SC::Result get(Span<char>& data)
        {
            SC_TRY(getAsync().buffer.sliceStartLength(0, completionData.numBytes, data));
            return returnCode;
        }
    };
    using AsyncRequest::start;

    Function<void(Result&)> callback; /// Callback called when some data has been read from the file into the buffer
    Span<char>              buffer;   /// The writeable span of memory where to data will be written
    FileDescriptor::Handle  handle;   /// The file/pipe descriptor handle to read data from.
    /// Use SC::FileDescriptor or SC::PipeDescriptor to open it.

    /// @brief Returns the last offset set with AsyncFileRead::setOffset
    uint64_t getOffset() const { return offset; }

    /// @brief Sets the offset in bytes at which start reading.
    /// @note Setting file offset when reading is only possible on seekable files
    void setOffset(uint64_t fileOffset)
    {
        useOffset = true;
        offset    = fileOffset;
    }

  private:
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);

    bool useOffset = false;
    bool endedSync = false;

    uint64_t offset = 0; /// Offset from file start where to start reading. Not supported on pipes.
#if SC_PLATFORM_WINDOWS
    uint64_t                    readCursor = 0;
    detail::WinOverlappedOpaque overlapped;
#endif
};

/// @brief Starts a file write operation, writing bytes to a file (or pipe).
/// Callback will be called when the file is ready to receive more bytes to write. @n
///
/// Call AsyncRequest::executeOn to set a thread pool if this is a buffered file and not a pipe.
/// This is important on APIs with blocking behaviour on buffered file I/O (all apis with the exception of `io_uring`).
///
/// @ref library_file library can be used to open the file and obtain a blocking or non-blocking file descriptor handle.
/// @n
///
/// @note Pipes or files opened using Posix `O_DIRECT` or Windows `FILE_FLAG_WRITE_THROUGH` & `FILE_FLAG_NO_BUFFERING`
/// should instead avoid using the `Task` parameter for best performance.
///
/// When not using the `Task` remember to:
/// - Open the file descriptor for non-blocking IO (SC::File::OpenOptions::blocking == `false`)
/// - Call SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor on the file descriptor
///
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncFileWriteSnippet
struct AsyncFileWrite : public AsyncRequest
{
    AsyncFileWrite() : AsyncRequest(Type::FileWrite) { handle = FileDescriptor::Invalid; }

    struct CompletionData : public AsyncCompletionData
    {
        size_t numBytes = 0;
    };

    struct Result : public AsyncResultOf<AsyncFileWrite, CompletionData>
    {
        using AsyncResultOf<AsyncFileWrite, CompletionData>::AsyncResultOf;

        SC::Result get(size_t& writtenSizeInBytes)
        {
            writtenSizeInBytes = completionData.numBytes;
            return returnCode;
        }
    };

    using AsyncRequest::start;

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, Span<Span<const char>> data);

    /// @brief Sets async request members and calls AsyncEventLoop::start
    SC::Result start(AsyncEventLoop& eventLoop, Span<const char> data);

    Function<void(Result&)> callback; ///< Callback called when descriptor is ready to be written with more data

    FileDescriptor::Handle handle; ///< The file/pipe descriptor to write data to.
    ///< Use SC::FileDescriptor or SC::PipeDescriptor to open it.

    Span<const char>       buffer;              ///< The read-only span of memory where to read the data from
    Span<Span<const char>> buffers;             ///< The read-only spans of memory where to read the data from
    bool                   singleBuffer = true; ///< Controls if buffer or buffers will be used

    /// @brief Returns the last offset set with AsyncFileWrite::setOffset
    uint64_t getOffset() const { return offset; }

    /// @brief Sets the offset in bytes at which start writing.
    /// @note Setting write file offset when reading is only possible on seekable files
    void setOffset(uint64_t fileOffset)
    {
        useOffset = true;
        offset    = fileOffset;
    }

  private:
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);

#if SC_PLATFORM_WINDOWS
    bool endedSync = false;
#else
    bool isWatchable = false;
#endif
    bool     useOffset = false;
    uint64_t offset    = 0xffffffffffffffff; /// Offset to start writing from. Not supported on pipes.

    size_t totalBytesWritten = 0;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};

/// @brief Starts an handle polling operation.
/// Uses `GetOverlappedResult` (windows), `kevent` (macOS), `epoll` (Linux) and `io_uring` (Linux).
/// Callback will be called when any of the three API signals readiness events on the given file descriptor.
/// Check @ref library_file_system_watcher for an example usage of this notification.
struct AsyncFilePoll : public AsyncRequest
{
    AsyncFilePoll() : AsyncRequest(Type::FilePoll) {}

    using CompletionData = AsyncCompletionData;
    using Result         = AsyncResultOf<AsyncFilePoll, CompletionData>;

    /// Starts a file descriptor poll operation, monitoring its readiness with appropriate OS API
    SC::Result start(AsyncEventLoop& eventLoop, FileDescriptor::Handle fileDescriptor);

#if SC_PLATFORM_WINDOWS
    [[nodiscard]] void* getOverlappedPtr();
#endif

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;
    SC::Result validate(AsyncEventLoop&);

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};

// forward declared because it must be defined after AsyncTaskSequence
struct AsyncLoopWork;
struct AsyncFileSystemOperation;

struct AsyncFileSystemOperationCompletionData : public AsyncCompletionData
{
    FileDescriptor::Handle handle = FileDescriptor::Invalid; // for open

    int    code     = 0; // for open/close
    size_t numBytes = 0; // for read
};

namespace detail
{
// A simple hand-made variant of all completion types
struct AsyncCompletionVariant
{
    AsyncCompletionVariant() {}
    ~AsyncCompletionVariant() { destroy(); }

    AsyncCompletionVariant(const AsyncCompletionVariant&)            = delete;
    AsyncCompletionVariant(AsyncCompletionVariant&&)                 = delete;
    AsyncCompletionVariant& operator=(const AsyncCompletionVariant&) = delete;
    AsyncCompletionVariant& operator=(AsyncCompletionVariant&&)      = delete;

    bool inited = false;

    AsyncRequest::Type type;
    union
    {
        AsyncCompletionData completionDataLoopWork; // Defined after AsyncCompletionVariant / AsyncTaskSequence
        AsyncLoopTimeout::CompletionData       completionDataLoopTimeout;
        AsyncLoopWakeUp::CompletionData        completionDataLoopWakeUp;
        AsyncProcessExit::CompletionData       completionDataProcessExit;
        AsyncSocketAccept::CompletionData      completionDataSocketAccept;
        AsyncSocketConnect::CompletionData     completionDataSocketConnect;
        AsyncSocketSend::CompletionData        completionDataSocketSend;
        AsyncSocketSendTo::CompletionData      completionDataSocketSendTo;
        AsyncSocketReceive::CompletionData     completionDataSocketReceive;
        AsyncSocketReceiveFrom::CompletionData completionDataSocketReceiveFrom;
        AsyncFileRead::CompletionData          completionDataFileRead;
        AsyncFileWrite::CompletionData         completionDataFileWrite;
        AsyncFilePoll::CompletionData          completionDataFilePoll;

        AsyncFileSystemOperationCompletionData completionDataFileSystemOperation;
    };

    auto& getCompletion(AsyncLoopWork&) { return completionDataLoopWork; }
    auto& getCompletion(AsyncLoopTimeout&) { return completionDataLoopTimeout; }
    auto& getCompletion(AsyncLoopWakeUp&) { return completionDataLoopWakeUp; }
    auto& getCompletion(AsyncProcessExit&) { return completionDataProcessExit; }
    auto& getCompletion(AsyncSocketAccept&) { return completionDataSocketAccept; }
    auto& getCompletion(AsyncSocketConnect&) { return completionDataSocketConnect; }
    auto& getCompletion(AsyncSocketSend&) { return completionDataSocketSend; }
    auto& getCompletion(AsyncSocketReceive&) { return completionDataSocketReceive; }
    auto& getCompletion(AsyncFileRead&) { return completionDataFileRead; }
    auto& getCompletion(AsyncFileWrite&) { return completionDataFileWrite; }
    auto& getCompletion(AsyncFilePoll&) { return completionDataFilePoll; }
    auto& getCompletion(AsyncFileSystemOperation&) { return completionDataFileSystemOperation; }

    template <typename T>
    auto& construct(T& t)
    {
        destroy();
        placementNew(getCompletion(t));
        inited = true;
        type   = t.getType();
        return getCompletion(t);
    }
    void destroy();
};
} // namespace detail

/// @brief An AsyncSequence using a SC::ThreadPool to execute one or more SC::AsyncRequest in a background thread.
/// Calling SC::AsyncRequest::executeOn on multiple requests with the same SC::AsyncTaskSequence queues them to be
/// serially executed on the same thread.
struct AsyncTaskSequence : public AsyncSequence
{
  protected:
    ThreadPoolTask task;
    ThreadPool*    threadPool = nullptr;

    friend struct AsyncEventLoop;
    friend struct AsyncRequest;

    detail::AsyncCompletionVariant completion;

    SC::Result returnCode = SC::Result(true);
};

/// @brief Executes work in a thread pool and then invokes a callback on the event loop thread. @n
/// AsyncLoopWork::work is invoked on one of the thread supplied by the ThreadPool passed during AsyncLoopWork::start.
/// AsyncLoopWork::callback will be called as a completion, on the event loop thread AFTER work callback is finished.
///
/// \snippet Tests/Libraries/Async/AsyncTestLoopWork.inl AsyncLoopWorkSnippet
struct AsyncLoopWork : public AsyncRequest
{
    AsyncLoopWork() : AsyncRequest(Type::LoopWork) {}

    using CompletionData = AsyncCompletionData;
    using Result         = AsyncResultOf<AsyncLoopWork, CompletionData>;

    /// @brief Sets the ThreadPool that will supply the thread to run the async work on
    /// @note Always call this method at least once before AsyncLoopWork::start
    SC::Result setThreadPool(ThreadPool& threadPool);

    Function<SC::Result()>  work;     /// Called to execute the work in a background threadpool thread
    Function<void(Result&)> callback; /// Called after work is done, on the thread calling EventLoop::run()

  private:
    friend struct AsyncEventLoop;
    SC::Result        validate(AsyncEventLoop&);
    AsyncTaskSequence task;
};

/// @brief Starts an asynchronous file system operation (open, close, read, write, sendFile, stat, lstat, fstat, etc.)
/// Some operations need a file path and others need a file descriptor.
/// @note Operations will run on the thread pool set with AsyncFileSystemOperation::setThreadPool on all backends except
/// when the event loop is using io_uring on Linux.
/// @warning File paths must be encoded in the native encoding of the OS, that is UTF-8 on Posix and UTF-16 on Windows.
///
/// Example of async open operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationOpenSnippet
///
/// Example of async close operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationCloseSnippet
///
/// Example of async read operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationReadSnippet
///
/// Example of async write operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationWriteSnippet
///
/// Example of async copy operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationCopySnippet
///
/// Example of async copy directory operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationCopyDirectorySnippet
///
/// Example of async rename operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationRenameSnippet
///
/// Example of async remove empty directory operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationRemoveEmptyFolderSnippet
///
/// Example of async remove file operation:
/// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationRemoveFileSnippet
///
struct AsyncFileSystemOperation : public AsyncRequest
{
    AsyncFileSystemOperation() : AsyncRequest(Type::FileSystemOperation) {}
    ~AsyncFileSystemOperation() { destroy(); }
#ifdef CopyFile
#undef CopyFile
#endif
#ifdef RemoveDirectory
#undef RemoveDirectory
#endif
    enum class Operation
    {
        None = 0,
        Open,
        Close,
        Read,
        Write,
        CopyFile,
        CopyDirectory,
        Rename,
        RemoveDirectory,
        RemoveFile,
    };

    using CompletionData = AsyncFileSystemOperationCompletionData;
    using Result         = AsyncResultOf<AsyncFileSystemOperation, CompletionData>;

    /// @brief Sets the thread pool to use for the operation
    SC::Result setThreadPool(ThreadPool& threadPool);

    Function<void(Result&)> callback; ///< Called after the operation is completed, on the event loop thread

    /// @brief Opens a file asynchronously and returns its corresponding file descriptor
    /// @param eventLoop The event loop to use
    /// @param path The path to the file to open (encoded in UTF-8 on Posix and UTF-16 on Windows)
    /// @param mode The mode to open the file in (read, write, read-write, etc.)
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationOpenSnippet
    SC::Result open(AsyncEventLoop& eventLoop, StringSpan path, FileOpen mode);

    /// @brief Closes a file descriptor asynchronously
    /// @param eventLoop The event loop to use
    /// @param handle The file descriptor to close
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationCloseSnippet
    SC::Result close(AsyncEventLoop& eventLoop, FileDescriptor::Handle handle);

    /// @brief Reads data from a file descriptor at a given offset
    /// @param eventLoop The event loop to use
    /// @param handle The file descriptor to read from
    /// @param buffer The buffer to read into
    /// @param offset The offset in the file to read from
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationReadSnippet
    SC::Result read(AsyncEventLoop& eventLoop, FileDescriptor::Handle handle, Span<char> buffer, uint64_t offset);

    /// @brief Writes data to a file descriptor at a given offset
    /// @param eventLoop The event loop to use
    /// @param handle The file descriptor to write to
    /// @param buffer The buffer containing data to write
    /// @param offset The offset in the file to write to
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationWriteSnippet
    SC::Result write(AsyncEventLoop& eventLoop, FileDescriptor::Handle handle, Span<const char> buffer,
                     uint64_t offset);

    /// @brief Copies a file from one location to another
    /// @param eventLoop The event loop to use
    /// @param path The path to the source file
    /// @param destinationPath The path to the destination file
    /// @param copyFlags Flags to control the copy operation
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationCopySnippet
    SC::Result copyFile(AsyncEventLoop& eventLoop, StringSpan path, StringSpan destinationPath,
                        FileSystemCopyFlags copyFlags = FileSystemCopyFlags());

    /// @brief Copies a directory from one location to another
    /// @param eventLoop The event loop to use
    /// @param path The path to the source directory
    /// @param destinationPath The path to the destination directory
    /// @param copyFlags Flags to control the copy operation
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationCopyDirectorySnippet
    SC::Result copyDirectory(AsyncEventLoop& eventLoop, StringSpan path, StringSpan destinationPath,
                             FileSystemCopyFlags copyFlags = FileSystemCopyFlags());

    /// @brief Renames a file
    /// @param eventLoop The event loop to use
    /// @param path The path to the source file
    /// @param newPath The new path to the file
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationRenameSnippet
    SC::Result rename(AsyncEventLoop& eventLoop, StringSpan path, StringSpan newPath);

    /// @brief Removes a directory asynchronously
    /// @param eventLoop The event loop to use
    /// @param path The path to the directory to remove
    /// @note The directory must be empty for this operation to succeed
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationRemoveEmptyFolderSnippet
    SC::Result removeEmptyDirectory(AsyncEventLoop& eventLoop, StringSpan path);

    /// @brief Removes a file asynchronously
    /// @param eventLoop The event loop to use
    /// @param path The path to the file to remove
    /// \snippet Tests/Libraries/Async/AsyncTestFileSystemOperation.inl AsyncFileSystemOperationRemoveFileSnippet
    SC::Result removeFile(AsyncEventLoop& eventLoop, StringSpan path);

  private:
    friend struct AsyncEventLoop;
    Operation      operation = Operation::None;
    AsyncLoopWork  loopWork;
    CompletionData completionData;

    void onOperationCompleted(AsyncLoopWork::Result& res);

    struct FileDescriptorData
    {
        FileDescriptor::Handle handle;
    };

    struct OpenData
    {
        StringSpan path;
        FileOpen   mode;
    };

    struct ReadData
    {
        FileDescriptor::Handle handle;
        Span<char>             buffer;
        uint64_t               offset;
    };

    struct WriteData
    {
        FileDescriptor::Handle handle;
        Span<const char>       buffer;
        uint64_t               offset;
    };

    struct CopyFileData
    {
        StringSpan          path;
        StringSpan          destinationPath;
        FileSystemCopyFlags copyFlags;
    };

    using CopyDirectoryData = CopyFileData;

    using CloseData = FileDescriptorData;

    struct RenameData
    {
        StringSpan path;
        StringSpan newPath;
    };

    struct RemoveData
    {
        StringSpan path;
    };

    union
    {
        OpenData          openData;
        CloseData         closeData;
        ReadData          readData;
        WriteData         writeData;
        CopyFileData      copyFileData;
        CopyDirectoryData copyDirectoryData;
        RenameData        renameData;
        RemoveData        removeData;
    };

    void destroy();

    SC::Result start(AsyncEventLoop& eventLoop, FileDescriptor::Handle fileDescriptor);
    SC::Result validate(AsyncEventLoop&);
};

/// @brief Allows user to supply a block of memory that will store kernel I/O events retrieved from
/// AsyncEventLoop::runOnce. Such events can then be later passed to AsyncEventLoop::dispatchCompletions.
/// @see AsyncEventLoop::runOnce
struct AsyncKernelEvents
{
    Span<uint8_t> eventsMemory; ///< User supplied block of memory used to store kernel I/O events

  private:
    int numberOfEvents = 0;
    friend struct AsyncEventLoop;
};

/// @brief Allow library user to provide callbacks signaling different phases of async event loop cycle
struct AsyncEventLoopListeners
{
    Function<void(AsyncEventLoop&)> beforeBlockingPoll;
    Function<void(AsyncEventLoop&)> afterBlockingPoll;
};

/// @brief Asynchronous I/O (files, sockets, timers, processes, fs events, threads wake-up) (see @ref library_async)
/// AsyncEventLoop pushes all AsyncRequest derived classes to I/O queues in the OS.
/// @see AsyncEventLoopMonitor can be used to integrate AsyncEventLoop with a GUI event loop
///
/// Basic lifetime for an event loop is:
/// \snippet Tests/Libraries/Async/AsyncTest.cpp AsyncEventLoopSnippet
struct AsyncEventLoop
{
    /// @brief Options given to AsyncEventLoop::create
    struct Options
    {
        enum class ApiType : uint8_t
        {
            Automatic = 0,   ///< Platform specific backend chooses the best API.
            ForceUseIOURing, ///< (Linux only) Tries to use `io_uring` (failing if it's not found on the system)
            ForceUseEpoll,   ///< (Linux only) Tries to use `epoll`
        };
        ApiType apiType; ///< Criteria to choose Async IO API

        Options() { apiType = ApiType::Automatic; }
    };

    AsyncEventLoop();

    /// Creates the event loop kernel object
    Result create(Options options = Options());

    /// Closes the event loop kernel object
    Result close();

    /// @brief Queues an async request request that has been correctly setup
    /// @note The request will be validated immediately and activated during next event loop cycle
    Result start(AsyncRequest& async);

    /// Interrupts the event loop even if it has active request on it
    void interrupt();

    /// @brief Returns `true` if create has been already called (successfully)
    [[nodiscard]] bool isInitialized() const;

    /// Blocks until there are no more active queued requests, dispatching all completions.
    /// It's useful for applications where the eventLoop is the only (or the main) loop.
    /// One example could be a console based app doing socket IO or a web server.
    /// Waiting on kernel events blocks the current thread with 0% CPU utilization.
    /// @see AsyncEventLoop::blockingPoll to integrate the loop with a GUI event loop
    Result run();

    /// Blocks until at least one request proceeds, ensuring forward progress, dispatching all completions.
    /// It's useful for application where it's needed to run some idle work after every IO event.
    /// Waiting on requests blocks the current thread with 0% CPU utilization.
    ///
    /// This function is a shortcut invoking async event loop building blocks:
    /// - AsyncEventLoop::submitRequests
    /// - AsyncEventLoop::blockingPoll
    /// - AsyncEventLoop::dispatchCompletions
    /// @see AsyncEventLoop::blockingPoll for a description on how to integrate AsyncEventLoop with another event loop
    Result runOnce();

    /// Process active requests if any, dispatching their completions, or returns immediately without blocking.
    /// It's useful for game-like applications where the event loop runs every frame and one would like to check
    /// and dispatch its I/O callbacks in-between frames.
    /// This call allows poll-checking I/O without blocking.
    /// @see AsyncEventLoop::blockingPoll to integrate the loop with a GUI event loop
    Result runNoWait();

    /// Submits all queued async requests.
    /// An AsyncRequest becomes queued after user calls its specific AsyncRequest::start method.
    ///
    /// @see AsyncEventLoop::blockingPoll for a description on how to integrate AsyncEventLoop with another event loop
    Result submitRequests(AsyncKernelEvents& kernelEvents);

    /// Blocks until at least one event happens, ensuring forward progress, without executing completions.
    /// It's one of the three building blocks of AsyncEventLoop::runOnce allowing co-operation of AsyncEventLoop
    /// within another event loop (for example a GUI event loop or another IO event loop).
    ///
    /// One possible example of such integration with a GUI event loop could:
    ///
    /// - Call AsyncEventLoop::submitRequests on the GUI thread to queue some requests
    /// - Call AsyncEventLoop::blockingPoll on a secondary thread, storying AsyncKernelEvents
    /// - Wake up the GUI event loop from the secondary thread after AsyncEventLoop::blockingPoll returns
    /// - Call AsyncEventLoop:dispatchCompletions on the GUI event loop to dispatch callbacks on GUI thread
    /// - Repeat all steps
    ///
    /// Waiting on requests blocks the current thread with 0% CPU utilization.
    /// @param kernelEvents Mandatory parameter to store kernel IO events WITHOUT running their completions.
    /// In that case user is expected to run completions passing it to AsyncEventLoop::dispatchCompletions.
    /// @see AsyncEventLoop::submitRequests sends async requests to kernel before calling blockingPoll
    /// @see AsyncEventLoop::dispatchCompletions invokes callbacks associated with kernel events after blockingPoll
    /// @see AsyncEventLoop::setListeners sets function called before and after entering kernel poll
    Result blockingPoll(AsyncKernelEvents& kernelEvents);

    /// Invokes completions for the AsyncKernelEvents collected by a call to AsyncEventLoop::blockingPoll.
    /// This is typically done when user wants to pool for events on a thread (calling AsyncEventLoop::blockingPoll)
    /// and dispatch the callbacks on another thread (calling AsyncEventLoop::dispatchCompletions).
    /// The typical example would be integrating AsyncEventLoop with a GUI event loop.
    /// @see AsyncEventLoop::blockingPoll for a description on how to integrate AsyncEventLoop with another event loop
    Result dispatchCompletions(AsyncKernelEvents& kernelEvents);

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked).
    /// The parameter is an AsyncLoopWakeUp that must have been previously started (with AsyncLoopWakeUp::start).
    Result wakeUpFromExternalThread(AsyncLoopWakeUp& wakeUp);

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked)
    Result wakeUpFromExternalThread();

    /// @brief Creates an async TCP (IPV4 / IPV6) socket registered with the eventLoop
    Result createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor);

    /// @brief Creates an async UCP (IPV4 / IPV6) socket registered with the eventLoop
    Result createAsyncUDPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor);

    /// @brief Associates a previously created TCP / UDP socket with the eventLoop
    Result associateExternallyCreatedSocket(SocketDescriptor& outDescriptor);

    /// @brief Associates a previously created File Descriptor with the eventLoop.
    Result associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor);

    /// @brief Removes association of a TCP Socket with any event loop
    static Result removeAllAssociationsFor(SocketDescriptor& outDescriptor);

    /// @brief Removes association of a File Descriptor with any event loop
    static Result removeAllAssociationsFor(FileDescriptor& outDescriptor);

    /// Updates loop time to "now"
    void updateTime();

    /// Get Loop time
    [[nodiscard]] Time::Monotonic getLoopTime() const;

    /// Obtain the total number of active requests
    [[nodiscard]] int getNumberOfActiveRequests() const;

    /// Obtain the total number of submitted requests
    [[nodiscard]] int getNumberOfSubmittedRequests() const;

    /// @brief Returns the next AsyncLoopTimeout that will be executed (shortest relativeTimeout)
    /// @returns `nullptr` if no AsyncLoopTimeout has been started or scheduled
    [[nodiscard]] AsyncLoopTimeout* findEarliestLoopTimeout() const;

    /// @brief Excludes the request from active handles count (to avoid it keeping event loop alive)
    void excludeFromActiveCount(AsyncRequest& async);

    /// @brief Reverses the effect of excludeFromActiveCount for the request
    void includeInActiveCount(AsyncRequest& async);

    /// @brief Enumerates all requests objects associated with this loop
    void enumerateRequests(Function<void(AsyncRequest&)> enumerationCallback);

    /// @brief Sets reference to listeners that will signal different events in loop lifetime
    /// @note The structure pointed by this pointer must be valid throughout loop lifetime
    void setListeners(AsyncEventLoopListeners* listeners);

    /// @brief Checks if excludeFromActiveCount() has been called on the given request
    [[nodiscard]] static bool isExcludedFromActiveCount(const AsyncRequest& async);

    /// Check if liburing is loadable (only on Linux)
    /// @return true if liburing has been loaded, false otherwise (and on any non-Linux os)
    [[nodiscard]] static bool tryLoadingLiburing();

    /// @brief Clears the sequence
    void clearSequence(AsyncSequence& sequence);

    struct Internal;

  private:
    struct InternalDefinition
    {
        static constexpr int Windows = 520;
        static constexpr int Apple   = 512;
        static constexpr int Linux   = 720;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = 8;

        using Object = Internal;
    };

  public:
    using InternalOpaque = OpaqueObject<InternalDefinition>;

  private:
    InternalOpaque internalOpaque;
    Internal&      internal;

    friend struct AsyncRequest;
    friend struct AsyncFileWrite;
    friend struct AsyncFileRead;
    friend struct AsyncFileSystemOperation;
    friend struct AsyncResult;
};

/// @brief Monitors Async I/O events from a background thread using a blocking kernel function (no CPU usage on idle).
/// AsyncEventLoopMonitor makes it easy to integrate AsyncEventLoop within a GUI event loop or another I/O event loop.
/// This pattern avoids constantly polling the kernel, using virtually 0% of CPU time when waiting for events.
struct AsyncEventLoopMonitor
{
    Function<void(void)> onNewEventsAvailable; ///< Informs to call dispatchCompletions on GUI Event Loop

    /// @brief Create the monitoring thread for an AsyncEventLoop.
    /// To start monitoring events call AsyncEventLoopMonitor::startMonitoring.
    Result create(AsyncEventLoop& eventLoop);

    /// @brief Stop monitoring the AsyncEventLoop, disposing all resources
    Result close();

    /// @brief Queue all async requests submissions and start monitoring loop events on a background thread.
    /// On the background thread AsyncEventLoop::blockingPoll will block (with 0% CPU usage) and return only when
    /// it will be informed by the kernel of some new events.
    /// Immediately after AsyncEventLoopMonitor::onNewEventsAvailable will be called (on the background thread).
    /// In the code handler associated with this event, the user/caller should inform its main thread to call
    /// AsyncEventLoopMonitor::stopMonitoringAndDispatchCompletions.
    Result startMonitoring();

    /// @brief Stops monitoring events on the background thread and dispatches callbacks for completed requests.
    /// This is typically called by the user of this class on the _main thread_ or in general on the thread where
    /// the event loop that coordinates the application lives (GUI thread typically or another I/O Event Loop thread).
    /// @note In some cases this method will also immediately submit new requests that have been queued by callbacks.
    Result stopMonitoringAndDispatchCompletions();

  private:
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324) // useless warning on 32 bit... (structure was padded due to __declspec(align()))
#endif
    alignas(uint64_t) uint8_t eventsMemory[8 * 1024]; // 8 Kb of kernel events
#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif

    AsyncKernelEvents asyncKernelEvents;
    AsyncEventLoop*   eventLoop = nullptr;
    AsyncLoopWakeUp   eventLoopWakeUp;

    Thread      eventLoopThread;
    EventObject eventObjectEnterBlockingMode;
    EventObject eventObjectExitBlockingMode;

    Atomic<bool> finished    = false;
    Atomic<bool> needsWakeUp = true;

    bool wakeUpHasBeenCalled = false;

    Result monitoringLoopThread(Thread& thread);
};

} // namespace SC
//! @}
