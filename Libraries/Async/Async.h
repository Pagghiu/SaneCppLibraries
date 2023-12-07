// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Containers/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Function.h"
#include "../Foundation/Span.h"
#include "../System/Time.h"
#include "../Threading/Atomic.h"

// Descriptors
#include "../File/FileDescriptor.h"
#include "../Process/ProcessDescriptor.h"
#include "../Socket/SocketDescriptor.h"

//! @defgroup group_async Async
//! @copybrief library_async (see @ref library_async for more details)
//!
//! An async operation is a request (AsyncRequest) for some I/O to be done made to the OS.
//! Every async operation has an associated callback that is invoked when the request is fullfilled.
//! If the start function returns a valid (non error) Return code, then the user callback will be called both
//! in case of success and in case of any error.
//! If the function returns an invalid Return code, then the user callback will not be called.
//! The memory address of all AsyncRequest derived objects must be stable for the entire duration of a started async
//! request, that means they can be freed / moved after the user callback is executed.
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
    static constexpr int Windows = sizeof(void*) * 7;

    static constexpr size_t Alignment = alignof(void*);

    using Object = AsyncWinOverlapped;
};
using WinOverlappedOpaque = OpaqueObject<AsyncWinOverlappedDefinition>;

struct AsyncWinWaitDefinition
{
    using Handle                    = FileDescriptor::Handle;  // fd
    static constexpr Handle Invalid = FileDescriptor::Invalid; // invalid fd
    static Result           releaseHandle(Handle& waitHandle);
};
struct WinWaitHandle : public UniqueHandle<AsyncWinWaitDefinition>
{
};
} // namespace detail
} // namespace SC

//! @addtogroup group_async
//! @{

/// @brief Base class for all async requests, holding state and type.
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
#if SC_PLATFORM_WINDOWS
        WindowsPoll, ///< Request is an AsyncWindowsPoll object
#endif
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

    void            updateTime();
    AsyncEventLoop* eventLoop = nullptr;

  private:
    [[nodiscard]] static const char* TypeToString(Type type);
    enum class State : uint8_t
    {
        Free,       // not in any queue
        Active,     // when monitored by OS syscall
        Submitting, // when in submission queue
        Cancelling  // when in cancellation queue
    };

    friend struct AsyncEventLoop;

    template <typename Lambda>
    [[nodiscard]] static Result applyOnAsync(AsyncRequest& async, Lambda&& lambda);

#if SC_CONFIGURATION_DEBUG
    const char* debugName = "None";
#endif
    State   state;
    Type    type;
    int32_t eventIndex;
};

/// @brief Base class for all async result objcets
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

/// @brief Create an async result for a given AsyncRequest-derived class
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
struct AsyncLoopTimeout : public AsyncRequest
{
    /// @brief Result for LoopTimeout
    using Result = AsyncResultOf<AsyncLoopTimeout>;
    AsyncLoopTimeout() : AsyncRequest(Type::LoopTimeout) {}

    /// Starts a Timeout that is invoked after expiration (relative) time has passed.
    [[nodiscard]] SC::Result start(AsyncEventLoop& loop, Time::Milliseconds expiration);

    [[nodiscard]] auto getTimeout() const { return timeout; }

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;
    Time::Milliseconds          timeout; // not needed, but keeping just for debugging
    Time::HighResolutionCounter expirationTime;
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a wake-up operation, allowing threads to execute callbacks on loop thread.
/// Callback will be called from the main event loop after an external thread calls wakeUpFromExternalThread.
struct AsyncLoopWakeUp : public AsyncRequest
{
    /// @brief Result for AsyncLoopWakeUp
    using Result = AsyncResultOf<AsyncLoopWakeUp>;
    AsyncLoopWakeUp() : AsyncRequest(Type::LoopWakeUp) {}

    /// Starts a wake up request, that will be fullfilled when an external thread calls wakeUpFromExternalThread.
    /// EventObject is optional and allows the external thread to wait until the user callback has completed execution.
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, EventObject* eventObject = nullptr);

    [[nodiscard]] SC::Result wakeUp();

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;

    EventObject* eventObject = nullptr;
    Atomic<bool> pending     = false;
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a process exit notification request, that will be fullfilled when the given process is exited.
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

    // using Result = AsyncProcessExitResult;

    AsyncProcessExit() : AsyncRequest(Type::ProcessExit) {}

    /// Starts a process exit notification request, that will be fullfilled when the given process is exited.
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, ProcessDescriptor::Handle process);

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;
    ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
    detail::WinWaitHandle       waitHandle;
#endif
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a socket accept operation, obtaining a new socket from a listening socket.
/// The callback is called with a new socket connected to the given listening endpoint will be returned.
struct AsyncSocketAccept : public AsyncRequest
{
    /// @brief Result for AsyncSocketAccept
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

    /// Starts a socket accept operation, that will return a new socket connected to the given listening endpoint.
    /// SocketDescriptor must be created with async flags (createAsyncTCPSocket) and already bound and listening.
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;
    SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;

    SocketDescriptor clientSocket;
    uint8_t          acceptBuffer[288];
#endif
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a socket connect operation, connecting to a remote endpoint.
/// Callback will be called when the given socket is connected to ipAddress.
struct AsyncSocketConnect : public AsyncRequest
{
    /// @brief Result for AsyncSocketConnect
    using Result = AsyncResultOf<AsyncSocketConnect>;

    AsyncSocketConnect() : AsyncRequest(Type::SocketConnect) {}

    /// Starts a socket connect operation.
    /// Callback will be called when the given socket is connected to ipAddress.
    [[nodiscard]] SC::Result start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                   SocketIPAddress ipAddress);

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;
    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    SocketIPAddress          ipAddress;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a socket send operation, sending bytes to a remote endpoint.
/// Callback will be called when the given socket is ready to send more data.
struct AsyncSocketSend : public AsyncRequest
{
    /// @brief Result for AsyncSocketSend
    using Result = AsyncResultOf<AsyncSocketSend>;
    AsyncSocketSend() : AsyncRequest(Type::SocketSend) {}

    /// Starts a socket send operation.
    /// Callback will be called when the given socket is ready to send more data.
    [[nodiscard]] SC::Result start(AsyncEventLoop& loop, const SocketDescriptor& socketDescriptor,
                                   Span<const char> data);

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<const char>         data;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketReceive;

/// @brief Starts a socket receive operation, receiving bytes from a remote endpoint.
/// Callback will be called when some data is read from socket.
struct AsyncSocketReceive : public AsyncRequest
{
    /// @brief Result for AsyncSocketReceive
    struct Result : public AsyncResultOf<AsyncSocketReceive>
    {
        Result(AsyncSocketReceive& async, SC::Result&& res) : AsyncResultOf(async, move(res)) {}

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

    /// Starts a socket receive operation.
    /// Callback will be called when some data is read from socket.
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor,
                                   Span<char> data);

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<char>               data;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a socket close operation.
/// Callback will be called when the socket has been fully closed.
struct AsyncSocketClose : public AsyncRequest
{
    /// @brief Result for AsyncSocketClose
    using Result = AsyncResultOf<AsyncSocketClose>;

    AsyncSocketClose() : AsyncRequest(Type::SocketClose) {}

    /// Starts a socket close operation.
    /// Callback will be called when the socket has been fully closed.
    [[nodiscard]] SC::Result start(AsyncEventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    int                     code = 0;
    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a file receive operation, reading bytes from a file.
/// Callback will be called when some data is read from file.
struct AsyncFileRead : public AsyncRequest
{
    /// @brief Result for AsyncFileRead
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
#endif
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a file write operation, writing bytes to a file.
/// Callback will be called when the file is ready to receive more bytes to write.
struct AsyncFileWrite : public AsyncRequest
{
    /// @brief Result for AsyncFileWrite
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
#endif
};
//------------------------------------------------------------------------------------------------------

/// @brief Starts a file close operation, closing the OS file descriptor.
/// Callback will be called when the file is actually closed.
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
//------------------------------------------------------------------------------------------------------
#if SC_PLATFORM_WINDOWS || DOXYGEN

/// @brief Starts a windows poll operation, to be signaled by `GetOverlappedResult`.
/// Callback will be called when `GetOverlappedResult` signals events on the given file descriptor.
struct AsyncWindowsPoll : public AsyncRequest
{
    using Result = AsyncResultOf<AsyncWindowsPoll>;
    AsyncWindowsPoll() : AsyncRequest(Type::WindowsPoll) {}

    /// Starts a windows poll operation, monitoring the given file descriptor with GetOverlappedResult
    [[nodiscard]] SC::Result start(AsyncEventLoop& loop, FileDescriptor::Handle fileDescriptor);

    [[nodiscard]] auto& getOverlappedOpaque() { return overlapped; }

    Function<void(Result&)> callback;

  private:
    friend struct AsyncEventLoop;

    FileDescriptor::Handle fileDescriptor;

    detail::WinOverlappedOpaque overlapped;
};
#endif
//! @}

} // namespace SC
//------------------------------------------------------------------------------------------------------

/// @brief Asynchronous I/O (files, sockets, timers, processes, fs events, threads wake-up) (see @ref library_async)
struct SC::AsyncEventLoop
{
    /// Creates the event loop kernel object
    [[nodiscard]] Result create();

    /// Closes the event loop kernel object
    [[nodiscard]] Result close();

    /// Runs until there are no more active handles left.
    [[nodiscard]] Result run();

    /// Runs just a single step. Ensures forward progress. If there are no events it blocks.
    [[nodiscard]] Result runOnce();

    /// Runs just a single step without forward progress. If there are no events returns immediately.
    [[nodiscard]] Result runNoWait();

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked).
    /// The parameter is an AsyncLoopWakeUp that must have been previously started (with AsyncLoopWakeUp::start).
    [[nodiscard]] Result wakeUpFromExternalThread(AsyncLoopWakeUp& wakeUp);

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked)
    [[nodiscard]] Result wakeUpFromExternalThread();

    /// Helper to creates a TCP socket with AsyncRequest flags of the given family (IPV4 / IPV6).
    /// It also automatically registers the socket with the eventLoop (associateExternallyCreatedTCPSocket)
    [[nodiscard]] Result createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor);

    /// Associates a TCP Socket created externally (without using createAsyncTCPSocket) with the eventloop.
    [[nodiscard]] Result associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor);

    /// Associates a File descriptor created externally with the eventloop.
    [[nodiscard]] Result associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor);

    /// Returns handle to the kernel level IO queue object.
    /// Used by external systems calling OS async function themselves (FileSystemWatcher on windows for example)
    [[nodiscard]] Result getLoopFileDescriptor(FileDescriptor::Handle& fileDescriptor) const;

    /// Get Loop time
    [[nodiscard]] Time::HighResolutionCounter getLoopTime() const { return loopTime; }

  private:
    int numberOfActiveHandles = 0;
    int numberOfTimers        = 0;
    int numberOfWakeups       = 0;
    int numberOfExternals     = 0;

    IntrusiveDoubleLinkedList<AsyncRequest> submissions;
    IntrusiveDoubleLinkedList<AsyncRequest> activeTimers;
    IntrusiveDoubleLinkedList<AsyncRequest> activeWakeUps;
    IntrusiveDoubleLinkedList<AsyncRequest> manualCompletions;

    Time::HighResolutionCounter loopTime;

    struct KernelQueue;

    struct Internal;
    struct InternalDefinition
    {
        static constexpr int Windows = 224;
        static constexpr int Apple   = 144;
        static constexpr int Default = sizeof(void*);

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

    [[nodiscard]] Result stopAsync(AsyncRequest& async);

    // LoopWakeUp
    void executeWakeUps(AsyncResult& result);

    // Setup
    [[nodiscard]] Result queueSubmission(AsyncRequest& async);

    // Phases
    [[nodiscard]] Result stageSubmission(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result setupAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result activateAsync(KernelQueue& queue, AsyncRequest& async);
    [[nodiscard]] Result cancelAsync(KernelQueue& queue, AsyncRequest& async);

    void reportError(KernelQueue& queue, AsyncRequest& async, Result&& returnCode);
    void completeAsync(KernelQueue& queue, AsyncRequest& async, Result&& returnCode, bool& reactivate);
    void completeAndEventuallyReactivate(KernelQueue& queue, AsyncRequest& async, Result&& returnCode);

    enum class PollMode
    {
        NoWait,
        ForcedForwardProgress
    };

    [[nodiscard]] Result runStep(PollMode pollMode);

    friend struct AsyncRequest;
};

//! @}
