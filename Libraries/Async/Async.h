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

namespace SC
{
struct EventObject;
struct EventLoop;

struct AsyncRequest;
struct AsyncResult;
template <typename T>
struct AsyncResultOf;
} // namespace SC

namespace SC
{
struct AsyncWinOverlapped;
struct AsyncWinOverlappedDefinition
{
    static constexpr int Windows = sizeof(void*) * 7;

    static constexpr size_t Alignment = alignof(void*);

    using Object = AsyncWinOverlapped;
};
using AsyncWinOverlappedOpaque = OpaqueObject<AsyncWinOverlappedDefinition>;

struct AsyncWinWaitDefinition
{
    using Handle                    = FileDescriptor::Handle;  // fd
    static constexpr Handle Invalid = FileDescriptor::Invalid; // invalid fd
    static Result           releaseHandle(Handle& waitHandle);
};

struct AsyncWinWaitHandle : public UniqueHandle<AsyncWinWaitDefinition>
{
};

} // namespace SC

struct SC::AsyncRequest
{
    AsyncRequest* next = nullptr;
    AsyncRequest* prev = nullptr;

#if SC_CONFIGURATION_DEBUG
    void setDebugName(const char* newDebugName) { debugName = newDebugName; }
#else
    void setDebugName(const char* newDebugName) { SC_COMPILER_UNUSED(newDebugName); }
#endif

    [[nodiscard]] EventLoop* getEventLoop() const { return eventLoop; }

    enum class Type : uint8_t
    {
        LoopTimeout,
        LoopWakeUp,
        ProcessExit,
        SocketAccept,
        SocketConnect,
        SocketSend,
        SocketReceive,
        SocketClose,
        FileRead,
        FileWrite,
        FileClose,
#if SC_PLATFORM_WINDOWS
        WindowsPoll,
#endif
    };

    /// Call only from derived async time
    AsyncRequest(Type type) : state(State::Free), type(type), eventIndex(-1) {}

    /// Stops the async operation
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
    [[nodiscard]] static Result applyOnAsync(AsyncRequest& async, Lambda&& lambda);

#if SC_CONFIGURATION_DEBUG
    const char* debugName = "None";
#endif
    State   state;
    Type    type;
    int32_t eventIndex;
};

struct SC::AsyncResult
{
    using Type = AsyncRequest::Type;

    AsyncResult(Result&& res) : returnCode(move(res)) {}

    void reactivateRequest(bool value) { shouldBeReactivated = value; }

    [[nodiscard]] const Result& isValid() const { return returnCode; }

  protected:
    friend struct EventLoop;

    bool   shouldBeReactivated = false;
    Result returnCode;
};

template <typename T>
struct SC::AsyncResultOf : public AsyncResult
{
    T& async;
    AsyncResultOf(T& async, Result&& res) : AsyncResult(move(res)), async(async) {}
};

namespace SC
{
// Every async operation takes a callback as parameter that is invoked when the request is fullfilled.
// If the start function returns a valid (non error) Return code, then the user callback will be called both
// in case of success and in case of any error.
// If the function returns an invalid Return code, then the user callback will not be called.
// The memory address of all AsyncRequest* objects must be stable for the entire duration of a started async request,
// that means they can be freed / moved after the user callback is executed.

struct AsyncLoopTimeout;
using AsyncLoopTimeoutResult = AsyncResultOf<AsyncLoopTimeout>;

/// Starts a Timeout that is invoked after expiration (relative) time has passed.
struct AsyncLoopTimeout : public AsyncRequest
{
    using Result = AsyncLoopTimeoutResult;
    AsyncLoopTimeout() : AsyncRequest(Type::LoopTimeout) {}

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
struct AsyncLoopWakeUp;
using AsyncLoopWakeUpResult = AsyncResultOf<AsyncLoopWakeUp>;

/// Starts a wake up request, that will be fullfilled when an external thread calls wakeUpFromExternalThread.
struct AsyncLoopWakeUp : public AsyncRequest
{
    using Result = AsyncLoopWakeUpResult;
    AsyncLoopWakeUp() : AsyncRequest(Type::LoopWakeUp) {}

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
struct AsyncProcessExit;
struct AsyncProcessExitResult : public AsyncResultOf<AsyncProcessExit>
{
    AsyncProcessExitResult(AsyncProcessExit& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] Result moveTo(ProcessDescriptor::ExitStatus& status)
    {
        status = exitStatus;
        return AsyncResult::returnCode;
    }

  private:
    friend struct EventLoop;
    ProcessDescriptor::ExitStatus exitStatus;
};

/// Starts a process exit notification request, that will be fullfilled when the given process is exited.
struct AsyncProcessExit : public AsyncRequest
{
    using Result = AsyncProcessExitResult;

    AsyncProcessExit() : AsyncRequest(Type::ProcessExit) {}

    /// Starts a process exit notification request, that will be fullfilled when the given process is exited.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, ProcessDescriptor::Handle process);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    AsyncWinOverlappedOpaque overlapped;
    AsyncWinWaitHandle       waitHandle;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketAccept;
struct AsyncSocketAcceptResult : public AsyncResultOf<AsyncSocketAccept>
{
    AsyncSocketAcceptResult(AsyncSocketAccept& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] Result moveTo(SocketDescriptor& client)
    {
        SC_TRY(AsyncResult::returnCode);
        return client.assign(move(acceptedClient));
    }

  private:
    friend struct EventLoop;
    SocketDescriptor acceptedClient;
};

/// Starts a socket accept operation, that will return a new socket connected to the given listening endpoint.
struct AsyncSocketAccept : public AsyncRequest
{
    using Result = AsyncSocketAcceptResult;
    AsyncSocketAccept() : AsyncRequest(Type::SocketAccept) {}

    /// Starts a socket accept operation, that will return a new socket connected to the given listening endpoint.
    /// SocketDescriptor must be created with async flags (createAsyncTCPSocket) and already bound and listening.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
#if SC_PLATFORM_WINDOWS
    AsyncWinOverlappedOpaque overlapped;
    SocketDescriptor         clientSocket;
    uint8_t                  acceptBuffer[288];
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketConnect;
using AsyncSocketConnectResult = AsyncResultOf<AsyncSocketConnect>;
/// Starts a socket connect operation. Callback will be called when the given socket is connected to ipAddress.
struct AsyncSocketConnect : public AsyncRequest
{
    using Result = AsyncSocketConnectResult;
    AsyncSocketConnect() : AsyncRequest(Type::SocketConnect) {}

    /// Starts a socket connect operation. Callback will be called when the given socket is connected to ipAddress.
    [[nodiscard]] SC::Result start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                   SocketIPAddress ipAddress);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    SocketIPAddress          ipAddress;
#if SC_PLATFORM_WINDOWS
    AsyncWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketSend;
using AsyncSocketSendResult = AsyncResultOf<AsyncSocketSend>;
/// Starts a socket send operation. Callback will be called when the given socket is ready to send more data.
struct AsyncSocketSend : public AsyncRequest
{
    using Result = AsyncSocketSendResult;
    AsyncSocketSend() : AsyncRequest(Type::SocketSend) {}

    /// Starts a socket send operation. Callback will be called when the given socket is ready to send more data.
    [[nodiscard]] SC::Result start(EventLoop& loop, const SocketDescriptor& socketDescriptor, Span<const char> data);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<const char>         data;
#if SC_PLATFORM_WINDOWS
    AsyncWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketReceive;
struct AsyncSocketReceiveResult : public AsyncResultOf<AsyncSocketReceive>
{
    AsyncSocketReceiveResult(AsyncSocketReceive& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] Result moveTo(Span<char>& outData)
    {
        outData = readData;
        return AsyncResult::returnCode;
    }

  private:
    friend struct EventLoop;
    Span<char> readData;
};
/// Starts a socket receive operation. Callback will be called when some data is read from socket.
struct AsyncSocketReceive : public AsyncRequest
{
    using Result = AsyncSocketReceiveResult;

    AsyncSocketReceive() : AsyncRequest(Type::SocketReceive) {}

    /// Starts a socket receive operation. Callback will be called when some data is read from socket.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor, Span<char> data);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<char>               data;
#if SC_PLATFORM_WINDOWS
    AsyncWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketClose;
using AsyncSocketCloseResult = AsyncResultOf<AsyncSocketClose>;
/// Starts a socket close operation. Callback will be called when the socket has been fully closed.
struct AsyncSocketClose : public AsyncRequest
{
    using Result = AsyncSocketCloseResult;
    AsyncSocketClose() : AsyncRequest(Type::SocketClose) {}

    /// Starts a socket close operation. Callback will be called when the socket has been fully closed.
    [[nodiscard]] SC::Result start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    int                     code = 0;
    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
};
//------------------------------------------------------------------------------------------------------
struct AsyncFileRead;
struct AsyncFileReadResult : public AsyncResultOf<AsyncFileRead>
{
    AsyncFileReadResult(AsyncFileRead& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] Result moveTo(Span<char>& data)
    {
        data = readData;
        return AsyncResult::returnCode;
    }

  private:
    friend struct EventLoop;
    Span<char> readData;
};
/// Starts a file receive operation, that will return when some data is read from file.
struct AsyncFileRead : public AsyncRequest
{
    using Result = AsyncFileReadResult;
    AsyncFileRead() : AsyncRequest(Type::FileRead) {}
    /// Starts a file receive operation, that will return when some data is read from file.
    [[nodiscard]] SC::Result start(EventLoop& loop, FileDescriptor::Handle fileDescriptor, Span<char> readBuffer);

    uint64_t offset = 0;

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    FileDescriptor::Handle fileDescriptor;
    Span<char>             readBuffer;
#if SC_PLATFORM_WINDOWS
    AsyncWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncFileWrite;
struct AsyncFileWriteResult : public AsyncResultOf<AsyncFileWrite>
{
    AsyncFileWriteResult(AsyncFileWrite& async, Result&& res) : AsyncResultOf(async, move(res)) {}

    [[nodiscard]] Result moveTo(size_t& writtenSizeInBytes)
    {
        writtenSizeInBytes = writtenBytes;
        return AsyncResult::returnCode;
    }

  private:
    friend struct EventLoop;
    size_t writtenBytes = 0;
};
/// Starts a file receive operation, that will return when the file is ready to receive more bytes to write.
struct AsyncFileWrite : public AsyncRequest
{
    using Result = AsyncFileWriteResult;
    AsyncFileWrite() : AsyncRequest(Type::FileWrite) {}

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
    AsyncWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncFileClose;
using AsyncFileCloseResult = AsyncResultOf<AsyncFileClose>;
struct AsyncFileClose : public AsyncRequest
{
    using Result = AsyncFileCloseResult;
    AsyncFileClose() : AsyncRequest(Type::FileClose) {}

    [[nodiscard]] SC::Result start(EventLoop& eventLoop, FileDescriptor::Handle fileDescriptor);

    int                     code = 0;
    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    FileDescriptor::Handle fileDescriptor;
};
//------------------------------------------------------------------------------------------------------
#if SC_PLATFORM_WINDOWS
struct AsyncWindowsPoll;
using AsyncWindowsPollResult = AsyncResultOf<AsyncWindowsPoll>;
/// Starts a windows poll operation, monitoring the given file descriptor with GetOverlappedResult
struct AsyncWindowsPoll : public AsyncRequest
{
    using Result = AsyncWindowsPollResult;
    AsyncWindowsPoll() : AsyncRequest(Type::WindowsPoll) {}

    /// Starts a windows poll operation, monitoring the given file descriptor with GetOverlappedResult
    [[nodiscard]] SC::Result start(EventLoop& loop, FileDescriptor::Handle fileDescriptor);

    [[nodiscard]] auto& getOverlappedOpaque() { return overlapped; }

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    FileDescriptor::Handle   fileDescriptor;
    AsyncWinOverlappedOpaque overlapped;
};
#endif
} // namespace SC
//------------------------------------------------------------------------------------------------------

struct SC::EventLoop
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
