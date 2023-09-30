// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Containers/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Objects/Function.h"
#include "../Foundation/Objects/Span.h"
#include "../System/Time.h"
#include "../Threading/Atomic.h"

// Descriptors
#include "../File/FileDescriptor.h"
#include "../Process/ProcessDescriptor.h"
#include "../Socket/SocketDescriptor.h"
namespace SC
{
template <int offset, typename T, typename R>
inline T& fieldOffset(R& object)
{
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(&object) - offset);
}
} // namespace SC

#define SC_FIELD_OFFSET(Class, Field, Value)                                                                           \
    fieldOffset<SC_OFFSETOF(Class, Field), Class, decltype(Class::Field)>(Value);

namespace SC
{
struct EventObject;
struct EventLoop;

struct Async;
struct AsyncResultBase;
template <typename T>
struct AsyncResult;
// Operations
} // namespace SC

namespace SC
{
struct EventLoopWinOverlapped;
struct EventLoopWinOverlappedSizes
{
    static constexpr int Windows = sizeof(void*) * 7;
};
using EventLoopWinOverlappedTraits = OpaqueTraits<EventLoopWinOverlapped, EventLoopWinOverlappedSizes>;
using EventLoopWinOverlappedOpaque = OpaqueUniqueObject<OpaqueFuncs<EventLoopWinOverlappedTraits>>;

struct EventLoopWinWaitTraits
{
    using Handle                    = FileDescriptor::Handle;  // fd
    static constexpr Handle Invalid = FileDescriptor::Invalid; // invalid fd
    static ReturnCode       releaseHandle(Handle& waitHandle);
};

struct EventLoopWinWaitHandle : public UniqueTaggedHandleTraits<EventLoopWinWaitTraits>
{
};

} // namespace SC

// ASYNCS

struct SC::Async
{
    Async* next = nullptr;
    Async* prev = nullptr;

#if SC_DEBUG
    void setDebugName(const char* newDebugName) { debugName = newDebugName; }
#else
    void setDebugName(const char* newDebugName) { SC_UNUSED(newDebugName); }
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
    [[nodiscard]] static StringView TypeToString(Type type);

    /// Call only from derived async time
    Async(Type type) : state(State::Free), type(type), eventIndex(-1) {}

    /// Stops the async operation
    [[nodiscard]] ReturnCode stop();

  protected:
    [[nodiscard]] ReturnCode validateAsync();
    [[nodiscard]] ReturnCode queueSubmission(EventLoop& eventLoop);

    void       updateTime();
    EventLoop* eventLoop = nullptr;

  private:
    enum class State : uint8_t
    {
        Free,       // not in any queue
        Active,     // when monitored by OS syscall
        Submitting, // when in submission queue
        Cancelling  // when in cancellation queue
    };

    friend struct EventLoop;

    template <typename Lambda>
    [[nodiscard]] static ReturnCode applyOnAsync(Async& async, Lambda&& lambda);

#if SC_DEBUG
    const char* debugName = "None";
#endif
    State   state;
    Type    type;
    int32_t eventIndex;
};

struct SC::AsyncResultBase
{
    using Type = Async::Type;

    AsyncResultBase(ReturnCode&& res) : returnCode(move(res)) {}

    void reactivateRequest(bool value) { shouldBeReactivated = value; }

    [[nodiscard]] const ReturnCode& isValid() const { return returnCode; }

  protected:
    friend struct EventLoop;

    bool       shouldBeReactivated = false;
    ReturnCode returnCode;
};

template <typename T>
struct SC::AsyncResult : public AsyncResultBase
{
    T& async;
    AsyncResult(T& async, ReturnCode&& res) : AsyncResultBase(move(res)), async(async) {}
};

namespace SC
{
// Every async operation takes a callback as parameter that is invoked when the request is fullfilled.
// If the start function returns a valid (non error) Return code, then the user callback will be called both
// in case of success and in case of any error.
// If the function returns an invalid Return code, then the user callback will not be called.
// The memory address of all Async* objects must be stable for the entire duration of a started async request,
// that means they can be freed / moved after the user callback is executed.

struct AsyncLoopTimeout;
using AsyncLoopTimeoutResult = AsyncResult<AsyncLoopTimeout>;

/// Starts a Timeout that is invoked after expiration (relative) time has passed.
struct AsyncLoopTimeout : public Async
{
    using Result = AsyncLoopTimeoutResult;
    AsyncLoopTimeout() : Async(Type::LoopTimeout) {}

    /// Starts a Timeout that is invoked after expiration (relative) time has passed.
    [[nodiscard]] ReturnCode start(EventLoop& loop, IntegerMilliseconds expiration);

    [[nodiscard]] auto getTimeout() const { return timeout; }

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    IntegerMilliseconds timeout; // not needed, but keeping just for debugging
    TimeCounter         expirationTime;
};
//------------------------------------------------------------------------------------------------------
struct AsyncLoopWakeUp;
using AsyncLoopWakeUpResult = AsyncResult<AsyncLoopWakeUp>;

/// Starts a wake up request, that will be fullfilled when an external thread calls wakeUpFromExternalThread.
struct AsyncLoopWakeUp : public Async
{
    using Result = AsyncLoopWakeUpResult;
    AsyncLoopWakeUp() : Async(Type::LoopWakeUp) {}

    /// Starts a wake up request, that will be fullfilled when an external thread calls wakeUpFromExternalThread.
    /// EventObject is optional and allows the external thread to wait until the user callback has completed execution.
    [[nodiscard]] ReturnCode start(EventLoop& eventLoop, EventObject* eventObject = nullptr);

    [[nodiscard]] ReturnCode wakeUp();

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    EventObject* eventObject = nullptr;
    Atomic<bool> pending     = false;
};
//------------------------------------------------------------------------------------------------------
struct AsyncProcessExit;
struct AsyncProcessExitResult : public AsyncResult<AsyncProcessExit>
{
    AsyncProcessExitResult(AsyncProcessExit& async, ReturnCode&& res) : AsyncResult(async, move(res)) {}

    [[nodiscard]] ReturnCode moveTo(ProcessDescriptor::ExitStatus& status)
    {
        status = exitStatus;
        return AsyncResultBase::returnCode;
    }

  private:
    friend struct EventLoop;
    ProcessDescriptor::ExitStatus exitStatus;
};

/// Starts a process exit notification request, that will be fullfilled when the given process is exited.
struct AsyncProcessExit : public Async
{
    using Result = AsyncProcessExitResult;

    AsyncProcessExit() : Async(Type::ProcessExit) {}

    /// Starts a process exit notification request, that will be fullfilled when the given process is exited.
    [[nodiscard]] ReturnCode start(EventLoop& eventLoop, ProcessDescriptor::Handle process);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
    EventLoopWinWaitHandle       waitHandle;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketAccept;
struct AsyncSocketAcceptResult : public AsyncResult<AsyncSocketAccept>
{
    AsyncSocketAcceptResult(AsyncSocketAccept& async, ReturnCode&& res) : AsyncResult(async, move(res)) {}

    [[nodiscard]] ReturnCode moveTo(SocketDescriptor& client)
    {
        SC_TRY(AsyncResultBase::returnCode);
        return client.assign(move(acceptedClient));
    }

  private:
    friend struct EventLoop;
    SocketDescriptor acceptedClient;
};

/// Starts a socket accept operation, that will return a new socket connected to the given listening endpoint.
struct AsyncSocketAccept : public Async
{
    using Result = AsyncSocketAcceptResult;
    AsyncSocketAccept() : Async(Type::SocketAccept) {}

    /// Starts a socket accept operation, that will return a new socket connected to the given listening endpoint.
    /// SocketDescriptor must be created with async flags (createAsyncTCPSocket) and already bound and listening.
    [[nodiscard]] ReturnCode start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
    SocketDescriptor             clientSocket;
    uint8_t                      acceptBuffer[288];
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketConnect;
using AsyncSocketConnectResult = AsyncResult<AsyncSocketConnect>;
/// Starts a socket connect operation. Callback will be called when the given socket is connected to ipAddress.
struct AsyncSocketConnect : public Async
{
    using Result = AsyncSocketConnectResult;
    AsyncSocketConnect() : Async(Type::SocketConnect) {}

    /// Starts a socket connect operation. Callback will be called when the given socket is connected to ipAddress.
    [[nodiscard]] ReturnCode start(EventLoop& loop, const SocketDescriptor& socketDescriptor,
                                   SocketIPAddress ipAddress);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    SocketIPAddress          ipAddress;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketSend;
using AsyncSocketSendResult = AsyncResult<AsyncSocketSend>;
/// Starts a socket send operation. Callback will be called when the given socket is ready to send more data.
struct AsyncSocketSend : public Async
{
    using Result = AsyncSocketSendResult;
    AsyncSocketSend() : Async(Type::SocketSend) {}

    /// Starts a socket send operation. Callback will be called when the given socket is ready to send more data.
    [[nodiscard]] ReturnCode start(EventLoop& loop, const SocketDescriptor& socketDescriptor, Span<const char> data);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<const char>         data;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketReceive;
struct AsyncSocketReceiveResult : public AsyncResult<AsyncSocketReceive>
{
    AsyncSocketReceiveResult(AsyncSocketReceive& async, ReturnCode&& res) : AsyncResult(async, move(res)) {}

    [[nodiscard]] ReturnCode moveTo(Span<char>& outData)
    {
        outData = readData;
        return AsyncResultBase::returnCode;
    }

  private:
    friend struct EventLoop;
    Span<char> readData;
};
/// Starts a socket receive operation. Callback will be called when some data is read from socket.
struct AsyncSocketReceive : public Async
{
    using Result = AsyncSocketReceiveResult;

    AsyncSocketReceive() : Async(Type::SocketReceive) {}

    /// Starts a socket receive operation. Callback will be called when some data is read from socket.
    [[nodiscard]] ReturnCode start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor, Span<char> data);

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<char>               data;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncSocketClose;
using AsyncSocketCloseResult = AsyncResult<AsyncSocketClose>;
/// Starts a socket close operation. Callback will be called when the socket has been fully closed.
struct AsyncSocketClose : public Async
{
    using Result = AsyncSocketCloseResult;
    AsyncSocketClose() : Async(Type::SocketClose) {}

    /// Starts a socket close operation. Callback will be called when the socket has been fully closed.
    [[nodiscard]] ReturnCode start(EventLoop& eventLoop, const SocketDescriptor& socketDescriptor);

    int                     code = 0;
    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
};
//------------------------------------------------------------------------------------------------------
struct AsyncFileRead;
struct AsyncFileReadResult : public AsyncResult<AsyncFileRead>
{
    AsyncFileReadResult(AsyncFileRead& async, ReturnCode&& res) : AsyncResult(async, move(res)) {}

    [[nodiscard]] ReturnCode moveTo(Span<char>& data)
    {
        data = readData;
        return AsyncResultBase::returnCode;
    }

  private:
    friend struct EventLoop;
    Span<char> readData;
};
/// Starts a file receive operation, that will return when some data is read from file.
struct AsyncFileRead : public Async
{
    using Result = AsyncFileReadResult;
    AsyncFileRead() : Async(Type::FileRead) {}
    /// Starts a file receive operation, that will return when some data is read from file.
    [[nodiscard]] ReturnCode start(EventLoop& loop, FileDescriptor::Handle fileDescriptor, Span<char> readBuffer);

    uint64_t offset = 0;

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    FileDescriptor::Handle fileDescriptor;
    Span<char>             readBuffer;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncFileWrite;
struct AsyncFileWriteResult : public AsyncResult<AsyncFileWrite>
{
    AsyncFileWriteResult(AsyncFileWrite& async, ReturnCode&& res) : AsyncResult(async, move(res)) {}

    [[nodiscard]] ReturnCode moveTo(size_t& writtenSizeInBytes)
    {
        writtenSizeInBytes = writtenBytes;
        return AsyncResultBase::returnCode;
    }

  private:
    friend struct EventLoop;
    size_t writtenBytes = 0;
};
/// Starts a file receive operation, that will return when the file is ready to receive more bytes to write.
struct AsyncFileWrite : public Async
{
    using Result = AsyncFileWriteResult;
    AsyncFileWrite() : Async(Type::FileWrite) {}

    /// Starts a file receive operation, that will return when the file is ready to receive more bytes to write.
    [[nodiscard]] ReturnCode start(EventLoop& eventLoop, FileDescriptor::Handle fileDescriptor,
                                   Span<const char> writeBuffer);

    uint64_t offset = 0;

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    FileDescriptor::Handle fileDescriptor;
    Span<const char>       writeBuffer;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};
//------------------------------------------------------------------------------------------------------
struct AsyncFileClose;
using AsyncFileCloseResult = AsyncResult<AsyncFileClose>;
struct AsyncFileClose : public Async
{
    using Result = AsyncFileCloseResult;
    AsyncFileClose() : Async(Type::FileClose) {}

    [[nodiscard]] ReturnCode start(EventLoop& eventLoop, FileDescriptor::Handle fileDescriptor);

    int                     code = 0;
    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;
    FileDescriptor::Handle fileDescriptor;
};
//------------------------------------------------------------------------------------------------------
#if SC_PLATFORM_WINDOWS
struct AsyncWindowsPoll;
using AsyncWindowsPollResult = AsyncResult<AsyncWindowsPoll>;
/// Starts a windows poll operation, monitoring the given file descriptor with GetOverlappedResult
struct AsyncWindowsPoll : public Async
{
    using Result = AsyncWindowsPollResult;
    AsyncWindowsPoll() : Async(Type::WindowsPoll) {}

    /// Starts a windows poll operation, monitoring the given file descriptor with GetOverlappedResult
    [[nodiscard]] ReturnCode start(EventLoop& loop, FileDescriptor::Handle fileDescriptor);

    [[nodiscard]] auto& getOverlappedOpaque() { return overlapped; }

    Function<void(Result&)> callback;

  private:
    friend struct EventLoop;

    FileDescriptor::Handle       fileDescriptor;
    EventLoopWinOverlappedOpaque overlapped;
};
#endif
} // namespace SC
//------------------------------------------------------------------------------------------------------

struct SC::EventLoop
{
    /// Creates the event loop kernel object
    [[nodiscard]] ReturnCode create();

    /// Closes the event loop kernel object
    [[nodiscard]] ReturnCode close();

    /// Runs until there are no more active handles left.
    [[nodiscard]] ReturnCode run();

    /// Runs just a single step. Ensures forward progress. If there are no events it blocks.
    [[nodiscard]] ReturnCode runOnce();

    /// Runs just a single step without forward progress. If there are no events returns immediately.
    [[nodiscard]] ReturnCode runNoWait();

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked).
    /// The parameter is an AsyncLoopWakeUp that must have been previously started (with AsyncLoopWakeUp::start).
    [[nodiscard]] ReturnCode wakeUpFromExternalThread(AsyncLoopWakeUp& wakeUp);

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked)
    [[nodiscard]] ReturnCode wakeUpFromExternalThread();

    /// Helper to creates a TCP socket with Async flags of the given family (IPV4 / IPV6).
    /// It also automatically registers the socket with the eventLoop (associateExternallyCreatedTCPSocket)
    [[nodiscard]] ReturnCode createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor);

    /// Associates a TCP Socket created externally (without using createAsyncTCPSocket) with the eventloop.
    [[nodiscard]] ReturnCode associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor);

    /// Associates a File descriptor created externally with the eventloop.
    [[nodiscard]] ReturnCode associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor);

    /// Returns handle to the kernel level IO queue object.
    /// Used by external systems calling OS async function themselves (FileSystemWatcher on windows for example)
    [[nodiscard]] ReturnCode getLoopFileDescriptor(FileDescriptor::Handle& fileDescriptor) const;

    /// Get Loop time
    [[nodiscard]] TimeCounter getLoopTime() const { return loopTime; }

  private:
    int numberOfActiveHandles = 0;
    int numberOfTimers        = 0;
    int numberOfWakeups       = 0;
    int numberOfExternals     = 0;

    IntrusiveDoubleLinkedList<Async> submissions;
    IntrusiveDoubleLinkedList<Async> activeTimers;
    IntrusiveDoubleLinkedList<Async> activeWakeUps;
    IntrusiveDoubleLinkedList<Async> manualCompletions;

    TimeCounter loopTime;

    struct KernelQueue;

    struct Internal;
    struct InternalSizes
    {
        static constexpr int Windows = 224;
        static constexpr int Apple   = 144;
        static constexpr int Default = sizeof(void*);
    };

  public:
    using InternalTraits = OpaqueTraits<Internal, InternalSizes>;

  private:
    using InternalOpaque = OpaqueUniqueObject<OpaqueFuncs<InternalTraits>>;
    InternalOpaque internal;

    [[nodiscard]] int getTotalNumberOfActiveHandle() const;

    void removeActiveHandle(Async& async);
    void addActiveHandle(Async& async);
    void scheduleManualCompletion(Async& async);
    void increaseActiveCount();
    void decreaseActiveCount();

    // Timers
    [[nodiscard]] const TimeCounter* findEarliestTimer() const;

    void invokeExpiredTimers();
    void updateTime();
    void executeTimers(KernelQueue& queue, const TimeCounter& nextTimer);

    [[nodiscard]] ReturnCode stopAsync(Async& async);

    // LoopWakeUp
    void executeWakeUps(AsyncResultBase& result);

    // Setup
    [[nodiscard]] ReturnCode queueSubmission(Async& async);

    // Phases
    [[nodiscard]] ReturnCode stageSubmission(KernelQueue& queue, Async& async);
    [[nodiscard]] ReturnCode setupAsync(KernelQueue& queue, Async& async);
    [[nodiscard]] ReturnCode activateAsync(KernelQueue& queue, Async& async);
    [[nodiscard]] ReturnCode cancelAsync(KernelQueue& queue, Async& async);

    void reportError(KernelQueue& queue, Async& async, ReturnCode&& returnCode);
    void completeAsync(KernelQueue& queue, Async& async, ReturnCode&& returnCode, bool& reactivate);
    void completeAndEventuallyReactivate(KernelQueue& queue, Async& async, ReturnCode&& returnCode);

    enum class PollMode
    {
        NoWait,
        ForcedForwardProgress
    };

    [[nodiscard]] ReturnCode runStep(PollMode pollMode);

    friend struct Async;
};
