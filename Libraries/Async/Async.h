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
/// @brief Asynchronous I/O (files, sockets, timers, processes, fs events, threads wake-up) (see @ref library_async)
namespace Async
{
struct EventLoop;

struct AsyncRequest;
struct AsyncResult;
template <typename T>
struct AsyncResultOf;
} // namespace Async
} // namespace SC

namespace SC
{
namespace Async
{
namespace detail
{
struct WinOverlapped;
struct WinOverlappedDefinition
{
    static constexpr int Windows = sizeof(void*) * 7;

    static constexpr size_t Alignment = alignof(void*);

    using Object = WinOverlapped;
};
using WinOverlappedOpaque = OpaqueObject<WinOverlappedDefinition>;

struct WinWaitDefinition
{
    using Handle                    = FileDescriptor::Handle;  // fd
    static constexpr Handle Invalid = FileDescriptor::Invalid; // invalid fd
    static Result           releaseHandle(Handle& waitHandle);
};
struct WinWaitHandle : public UniqueHandle<WinWaitDefinition>
{
};
} // namespace detail
} // namespace Async
} // namespace SC

//! @addtogroup group_async
//! @{

/// @brief Base class for all async requests, holding state and type.
struct SC::Async::AsyncRequest
{
    Async::AsyncRequest* next = nullptr;
    Async::AsyncRequest* prev = nullptr;

#if SC_CONFIGURATION_DEBUG
    void setDebugName(const char* newDebugName) { debugName = newDebugName; }
#else
    void setDebugName(const char* newDebugName) { SC_COMPILER_UNUSED(newDebugName); }
#endif

    /// @brief Get the event loop associated with this AsyncRequest
    [[nodiscard]] EventLoop* getEventLoop() const { return eventLoop; }

    /// @brief Type of async request
    enum class Type : uint8_t
    {
        LoopTimeout,   ///< Request is an Async::LoopTimeout object
        LoopWakeUp,    ///< Request is an Async::LoopWakeUp object
        ProcessExit,   ///< Request is an Async::ProcessExit object
        SocketAccept,  ///< Request is an Async::SocketAccept object
        SocketConnect, ///< Request is an Async::SocketConnect object
        SocketSend,    ///< Request is an Async::SocketSend object
        SocketReceive, ///< Request is an Async::SocketReceive object
        SocketClose,   ///< Request is an Async::SocketClose object
        FileRead,      ///< Request is an Async::FileRead object
        FileWrite,     ///< Request is an Async::FileWrite object
        FileClose,     ///< Request is an Async::FileClose object
#if SC_PLATFORM_WINDOWS
        WindowsPoll, ///< Request is an Async::WindowsPoll object
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
    [[nodiscard]] Result queueSubmission(EventLoop& eventLoop);

    void       updateTime();
    EventLoop* eventLoop = nullptr;

  private:
    [[nodiscard]] static const char* TypeToString(Type type);
    enum class State : uint8_t
    {
        Free,       // not in any queue
        Active,     // when monitored by OS syscall
        Submitting, // when in submission queue
        Cancelling  // when in cancellation queue
    };

    friend struct EventLoop;

    template <typename Lambda>
    [[nodiscard]] static Result applyOnAsync(Async::AsyncRequest& async, Lambda&& lambda);

#if SC_CONFIGURATION_DEBUG
    const char* debugName = "None";
#endif
    State   state;
    Type    type;
    int32_t eventIndex;
};

/// @brief Base class for all async result objcets
struct SC::Async::AsyncResult
{
    using Type = Async::AsyncRequest::Type;

    /// @brief Constructs an async result from a result
    /// @param res The result of async operation
    AsyncResult(SC::Result&& res) : returnCode(move(res)) {}

    /// @brief Ask the event loop to re-activate this request after it was already completed
    /// @param value  `true` will reactivate the request
    void reactivateRequest(bool value) { shouldBeReactivated = value; }

    /// @brief Check if the returnCode of this result is valid
    [[nodiscard]] const SC::Result& isValid() const { return returnCode; }

  protected:
    friend struct EventLoop;

    bool       shouldBeReactivated = false;
    SC::Result returnCode;
};

/// @brief Helper to create an async result class
/// @tparam T Type of the request class associated to this result
template <typename T>
struct SC::Async::AsyncResultOf : public Async::AsyncResult
{
    T& async;
    AsyncResultOf(T& async, AsyncResult&& res) : AsyncResult(move(res)), async(async) {}
};

namespace SC
{
namespace Async
{
//! @addtogroup group_async
//! @{

struct LoopTimeout;
/// @brief Result for LoopTimeout
using LoopTimeoutResult = AsyncResultOf<Async::LoopTimeout>;

/// @brief Starts a Timeout that is invoked after expiration (relative) time has passed.
struct LoopTimeout : public AsyncRequest
{
    using Result = LoopTimeoutResult;
    LoopTimeout() : AsyncRequest(Type::LoopTimeout) {}

    /// Starts a Timeout that is invoked after expiration (relative) time has passed.
    [[nodiscard]] SC::Result start(EventLoop& loop, Time::Milliseconds expiration);

    [[nodiscard]] auto getTimeout() const { return timeout; }

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    Time::Milliseconds          timeout; // not needed, but keeping just for debugging
    Time::HighResolutionCounter expirationTime;
};
//------------------------------------------------------------------------------------------------------
struct LoopWakeUp;
/// @brief Result for LoopWakeUp
using LoopWakeUpResult = AsyncResultOf<LoopWakeUp>;

/// @brief Starts a wake-up operation, allowing threads to execute callbacks on loop thread.
/// Callback will be called from the main event loop after an external thread calls wakeUpFromExternalThread.
struct LoopWakeUp : public AsyncRequest
{
    using Result = LoopWakeUpResult;
    LoopWakeUp() : AsyncRequest(Type::LoopWakeUp) {}

    /// Starts a wake up request, that will be fullfilled when an external thread calls wakeUpFromExternalThread.
    /// EventObject is optional and allows the external thread to wait until the user callback has completed execution.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, EventObject* eventObject = nullptr);

    [[nodiscard]] SC::Result wakeUp();

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    EventObject* eventObject = nullptr;
    Atomic<bool> pending     = false;
};
//------------------------------------------------------------------------------------------------------
struct ProcessExit;
/// @brief Result for ProcessExit
struct ProcessExitResult : public AsyncResultOf<ProcessExit>
{
    ProcessExitResult(ProcessExit& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] SC::Result moveTo(ProcessDescriptor::ExitStatus& status)
    {
        status = exitStatus;
        return returnCode;
    }

  private:
    friend struct EventLoop;
    ProcessDescriptor::ExitStatus exitStatus;
};

/// @brief Starts a process exit notification request, that will be fullfilled when the given process is exited.
struct ProcessExit : public AsyncRequest
{
    using Result = ProcessExitResult;

    ProcessExit() : AsyncRequest(Type::ProcessExit) {}

    /// Starts a process exit notification request, that will be fullfilled when the given process is exited.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, ProcessDescriptor::Handle process);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
    detail::WinWaitHandle       waitHandle;
#endif
};
//------------------------------------------------------------------------------------------------------
struct SocketAccept;
/// @brief Result for SocketAccept
struct SocketAcceptResult : public AsyncResultOf<SocketAccept>
{
    SocketAcceptResult(SocketAccept& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] SC::Result moveTo(SocketDescriptor& client)
    {
        SC_TRY(returnCode);
        return client.assign(move(acceptedClient));
    }

  private:
    friend struct EventLoop;
    SocketDescriptor acceptedClient;
};

/// @brief Starts a socket accept operation, obtaining a new socket from a listening socket.
/// The callback is called with a new socket connected to the given listening endpoint will be returned.
struct SocketAccept : public AsyncRequest
{
    using Result = SocketAcceptResult;
    SocketAccept() : AsyncRequest(Type::SocketAccept) {}

    /// Starts a socket accept operation, that will return a new socket connected to the given listening endpoint.
    /// SocketDescriptor must be created with async flags (createAsyncTCPSocket) and already bound and listening.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;

    SocketDescriptor clientSocket;
    uint8_t          acceptBuffer[288];
#endif
};
//------------------------------------------------------------------------------------------------------
struct SocketConnect;
/// @brief Result for SocketConnect
using SocketConnectResult = AsyncResultOf<SocketConnect>;
/// @brief Starts a socket connect operation, connecting to a remote endpoint.
/// Callback will be called when the given socket is connected to ipAddress.
struct SocketConnect : public AsyncRequest
{
    using Result = SocketConnectResult;
    SocketConnect() : AsyncRequest(Type::SocketConnect) {}

    /// Starts a socket connect operation.
    /// Callback will be called when the given socket is connected to ipAddress.
    [[nodiscard]] SC::Result start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                   SocketIPAddress ipAddress);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    SocketIPAddress          ipAddress;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct SocketSend;
/// @brief Result for SocketSend
using SocketSendResult = AsyncResultOf<SocketSend>;
/// @brief Starts a socket send operation, sending bytes to a remote endpoint.
/// Callback will be called when the given socket is ready to send more data.
struct SocketSend : public AsyncRequest
{
    using Result = SocketSendResult;
    SocketSend() : AsyncRequest(Type::SocketSend) {}

    /// Starts a socket send operation.
    /// Callback will be called when the given socket is ready to send more data.
    [[nodiscard]] SC::Result start(EventLoop& loop, const SocketDescriptor& socketDescriptor, Span<const char> data);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<const char>         data;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct SocketReceive;
/// @brief Result for SocketReceive
struct SocketReceiveResult : public AsyncResultOf<SocketReceive>
{
    SocketReceiveResult(SocketReceive& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] SC::Result moveTo(Span<char>& outData)
    {
        outData = readData;
        return returnCode;
    }

  private:
    friend struct EventLoop;
    Span<char> readData;
};
/// @brief Starts a socket receive operation, receiving bytes from a remote endpoint.
/// Callback will be called when some data is read from socket.
struct SocketReceive : public AsyncRequest
{
    using Result = SocketReceiveResult;

    SocketReceive() : AsyncRequest(Type::SocketReceive) {}

    /// Starts a socket receive operation.
    /// Callback will be called when some data is read from socket.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor, Span<char> data);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<char>               data;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct SocketClose;
/// @brief Result for SocketClose
using SocketCloseResult = AsyncResultOf<SocketClose>;
/// @brief Starts a socket close operation.
/// Callback will be called when the socket has been fully closed.
struct SocketClose : public AsyncRequest
{
    using Result = SocketCloseResult;
    SocketClose() : AsyncRequest(Type::SocketClose) {}

    /// Starts a socket close operation.
    /// Callback will be called when the socket has been fully closed.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    int                     code = 0;
    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
};
//------------------------------------------------------------------------------------------------------
struct FileRead;
/// @brief Result for FileRead
struct FileReadResult : public AsyncResultOf<FileRead>
{
    FileReadResult(FileRead& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] SC::Result moveTo(Span<char>& data)
    {
        data = readData;
        return returnCode;
    }

  private:
    friend struct EventLoop;
    Span<char> readData;
};
/// @brief Starts a file receive operation, reading bytes from a file.
/// Callback will be called when some data is read from file.
struct FileRead : public AsyncRequest
{
    using Result = FileReadResult;
    FileRead() : AsyncRequest(Type::FileRead) {}
    /// Starts a file receive operation, that will return when some data is read from file.
    [[nodiscard]] SC::Result start(EventLoop& loop, FileDescriptor::Handle fileDescriptor, Span<char> readBuffer);

    uint64_t offset = 0;

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    FileDescriptor::Handle fileDescriptor;
    Span<char>             readBuffer;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct FileWrite;
/// @brief Result for FileWrite
struct FileWriteResult : public AsyncResultOf<FileWrite>
{
    FileWriteResult(FileWrite& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] SC::Result moveTo(size_t& writtenSizeInBytes)
    {
        writtenSizeInBytes = writtenBytes;
        return returnCode;
    }

  private:
    friend struct EventLoop;
    size_t writtenBytes = 0;
};
/// @brief Starts a file write operation, writing bytes to a file.
/// Callback will be called when the file is ready to receive more bytes to write.
struct FileWrite : public AsyncRequest
{
    using Result = FileWriteResult;
    FileWrite() : AsyncRequest(Type::FileWrite) {}

    /// Starts a file receive operation, that will return when the file is ready to receive more bytes to write.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, FileDescriptor::Handle fileDescriptor,
                                   Span<const char> writeBuffer);

    uint64_t offset = 0;

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    FileDescriptor::Handle fileDescriptor;
    Span<const char>       writeBuffer;
#if SC_PLATFORM_WINDOWS
    detail::WinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct FileClose;
using FileCloseResult = AsyncResultOf<FileClose>;
/// @brief Starts a file close operation, closing the OS file descriptor.
/// Callback will be called when the file is actually closed.
struct FileClose : public AsyncRequest
{
    using Result = FileCloseResult;
    FileClose() : AsyncRequest(Type::FileClose) {}

    [[nodiscard]] SC::Result start(EventLoop& eventLoop, FileDescriptor::Handle fileDescriptor);

    int                     code = 0;
    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    FileDescriptor::Handle fileDescriptor;
};
//------------------------------------------------------------------------------------------------------
#if SC_PLATFORM_WINDOWS || DOXYGEN
struct WindowsPoll;
using WindowsPollResult = AsyncResultOf<WindowsPoll>;
/// @brief Starts a windows poll operation, to be signaled by `GetOverlappedResult`.
/// Callback will be called when `GetOverlappedResult` signals events on the given file descriptor.
struct WindowsPoll : public AsyncRequest
{
    using Result = WindowsPollResult;
    WindowsPoll() : AsyncRequest(Type::WindowsPoll) {}

    /// Starts a windows poll operation, monitoring the given file descriptor with GetOverlappedResult
    [[nodiscard]] SC::Result start(EventLoop& loop, FileDescriptor::Handle fileDescriptor);

    [[nodiscard]] auto& getOverlappedOpaque() { return overlapped; }

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    FileDescriptor::Handle fileDescriptor;

    detail::WinOverlappedOpaque overlapped;
};
#endif
//! @}

} // namespace Async
} // namespace SC
//------------------------------------------------------------------------------------------------------

struct SC::Async::EventLoop
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
    /// The parameter is an Async::LoopWakeUp that must have been previously started (with Async::LoopWakeUp::start).
    [[nodiscard]] Result wakeUpFromExternalThread(Async::LoopWakeUp& wakeUp);

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked)
    [[nodiscard]] Result wakeUpFromExternalThread();

    /// Helper to creates a TCP socket with Async::AsyncRequest flags of the given family (IPV4 / IPV6).
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

    IntrusiveDoubleLinkedList<Async::AsyncRequest> submissions;
    IntrusiveDoubleLinkedList<Async::AsyncRequest> activeTimers;
    IntrusiveDoubleLinkedList<Async::AsyncRequest> activeWakeUps;
    IntrusiveDoubleLinkedList<Async::AsyncRequest> manualCompletions;

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

    void removeActiveHandle(Async::AsyncRequest& async);
    void addActiveHandle(Async::AsyncRequest& async);
    void scheduleManualCompletion(Async::AsyncRequest& async);
    void increaseActiveCount();
    void decreaseActiveCount();

    // Timers
    [[nodiscard]] const Time::HighResolutionCounter* findEarliestTimer() const;

    void invokeExpiredTimers();
    void updateTime();
    void executeTimers(KernelQueue& queue, const Time::HighResolutionCounter& nextTimer);

    [[nodiscard]] Result stopAsync(Async::AsyncRequest& async);

    // LoopWakeUp
    void executeWakeUps(AsyncResult& result);

    // Setup
    [[nodiscard]] Result queueSubmission(Async::AsyncRequest& async);

    // Phases
    [[nodiscard]] Result stageSubmission(KernelQueue& queue, Async::AsyncRequest& async);
    [[nodiscard]] Result setupAsync(KernelQueue& queue, Async::AsyncRequest& async);
    [[nodiscard]] Result activateAsync(KernelQueue& queue, Async::AsyncRequest& async);
    [[nodiscard]] Result cancelAsync(KernelQueue& queue, Async::AsyncRequest& async);

    void reportError(KernelQueue& queue, Async::AsyncRequest& async, Result&& returnCode);
    void completeAsync(KernelQueue& queue, Async::AsyncRequest& async, Result&& returnCode, bool& reactivate);
    void completeAndEventuallyReactivate(KernelQueue& queue, Async::AsyncRequest& async, Result&& returnCode);

    enum class PollMode
    {
        NoWait,
        ForcedForwardProgress
    };

    [[nodiscard]] Result runStep(PollMode pollMode);

    friend struct Async::AsyncRequest;
};

//! @}
