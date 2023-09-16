// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Function.h"
#include "../Foundation/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Span.h"
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
struct AsyncResult;

// Operations
struct AsyncLoopTimeout;
struct AsyncLoopWakeUp;
struct AsyncProcessExit;
struct AsyncSocketAccept;
struct AsyncSocketConnect;
struct AsyncSocketSend;
struct AsyncSocketReceive;
struct AsyncFileRead;
struct AsyncFileWrite;
#if SC_PLATFORM_WINDOWS
struct AsyncWindowsPoll;
#endif

struct AsyncLoopTimeoutResult;
struct AsyncLoopWakeUpResult;
struct AsyncProcessExitResult;
struct AsyncSocketAcceptResult;
struct AsyncSocketConnectResult;
struct AsyncSocketSendResult;
struct AsyncSocketReceiveResult;
struct AsyncFileReadResult;
struct AsyncFileWriteResult;

#if SC_PLATFORM_WINDOWS
struct AsyncWindowsPollResult;
#endif
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
    using LoopTimeout   = AsyncLoopTimeout;
    using LoopWakeUp    = AsyncLoopWakeUp;
    using ProcessExit   = AsyncProcessExit;
    using SocketAccept  = AsyncSocketAccept;
    using SocketConnect = AsyncSocketConnect;
    using SocketSend    = AsyncSocketSend;
    using SocketReceive = AsyncSocketReceive;
    using FileRead      = AsyncFileRead;
    using FileWrite     = AsyncFileWrite;
#if SC_PLATFORM_WINDOWS
    using WindowsPoll = AsyncWindowsPoll;
#endif

    AsyncLoopTimeout*   asLoopTimeout();
    AsyncLoopWakeUp*    asLoopWakeUp();
    AsyncProcessExit*   asProcessExit();
    AsyncSocketAccept*  asSocketAccept();
    AsyncSocketConnect* asSocketConnect();
    AsyncSocketSend*    asSocketSend();
    AsyncSocketReceive* asSocketReceive();
    AsyncFileRead*      asFileRead();
    AsyncFileWrite*     asFileWrite();
#if SC_PLATFORM_WINDOWS
    AsyncWindowsPoll* asWindowsPoll();
#endif

    enum class Type : uint8_t
    {
        LoopTimeout,
        LoopWakeUp,
        ProcessExit,
        SocketAccept,
        SocketConnect,
        SocketSend,
        SocketReceive,
        FileRead,
        FileWrite,
#if SC_PLATFORM_WINDOWS
        WindowsPoll,
#endif
    };

    static StringView TypeToString(Type type);

    enum class State : uint8_t
    {
        Free,       // not in any queue
        Active,     // when monitored by OS syscall
        Submitting, // when in submission queue
        Cancelling  // when in cancellation queue
    };

    Async(Type type) : type(type) {}

    Type getType() const { return type; }

    EventLoop* eventLoop = nullptr;
    Async*     next      = nullptr;
    Async*     prev      = nullptr;

    const char* debugName = "None";

    State state = State::Free;

  private:
    Type type = Type::LoopTimeout;
};

struct SC::AsyncLoopTimeout : public Async
{
    AsyncLoopTimeout() : Async(Type::LoopTimeout) {}

    Function<void(AsyncLoopTimeoutResult&)> callback;

    IntegerMilliseconds timeout; // not needed, but keeping just for debugging
    TimeCounter         expirationTime;
};

struct SC::AsyncLoopWakeUp : public Async
{
    AsyncLoopWakeUp() : Async(Type::LoopWakeUp) {}

    Function<void(AsyncLoopWakeUpResult&)> callback;

    [[nodiscard]] ReturnCode wakeUp();

    EventObject* eventObject = nullptr;
    Atomic<bool> pending     = false;
};

struct SC::AsyncProcessExit : public Async
{
    AsyncProcessExit() : Async(Type::ProcessExit) {}

    Function<void(AsyncProcessExitResult&)> callback;

    ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
    EventLoopWinWaitHandle       waitHandle;
#endif
};

struct SC::AsyncSocketAccept : public Async
{
    AsyncSocketAccept() : Async(Type::SocketAccept) {}

    Function<void(AsyncSocketAcceptResult&)> callback;

    SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
    SocketDescriptor             clientSocket;
    uint8_t                      acceptBuffer[288];
#endif
};

struct SC::AsyncSocketConnect : public Async
{
    AsyncSocketConnect() : Async(Type::SocketConnect) {}

    Function<void(AsyncSocketConnectResult&)> callback;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    SocketIPAddress          ipAddress;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

struct SC::AsyncSocketSend : public Async
{
    AsyncSocketSend() : Async(Type::SocketSend) {}

    Function<void(AsyncSocketSendResult&)> callback;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<const char>         data;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

struct SC::AsyncSocketReceive : public Async
{
    AsyncSocketReceive() : Async(Type::SocketReceive) {}

    Function<void(AsyncSocketReceiveResult&)> callback;

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<char>               data;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

struct SC::AsyncFileRead : public Async
{
    Function<void(AsyncFileReadResult&)> callback;
    AsyncFileRead() : Async(Type::FileRead) {}

    FileDescriptor::Handle fileDescriptor;
    uint64_t               offset = 0;
    Span<char>             readBuffer;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

struct SC::AsyncFileWrite : public Async
{
    AsyncFileWrite() : Async(Type::FileWrite) {}

    Function<void(AsyncFileWriteResult&)> callback;

    FileDescriptor::Handle fileDescriptor;
    uint64_t               offset = 0;
    Span<const char>       writeBuffer;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

#if SC_PLATFORM_WINDOWS
struct SC::AsyncWindowsPoll : public Async
{
    AsyncWindowsPoll() : Async(Type::WindowsPoll) {}

    Function<void(AsyncWindowsPollResult&)> callback;

    FileDescriptor::Handle       fileDescriptor;
    EventLoopWinOverlappedOpaque overlapped;
};
#endif

// RESULTS

struct SC::AsyncResult
{
    using LoopTimeout   = AsyncLoopTimeoutResult;
    using LoopWakeUp    = AsyncLoopWakeUpResult;
    using ProcessExit   = AsyncProcessExitResult;
    using SocketAccept  = AsyncSocketAcceptResult;
    using SocketConnect = AsyncSocketConnectResult;
    using SocketSend    = AsyncSocketSendResult;
    using SocketReceive = AsyncSocketReceiveResult;
    using FileRead      = AsyncFileReadResult;
    using FileWrite     = AsyncFileWriteResult;
#if SC_PLATFORM_WINDOWS
    using WindowsPoll = AsyncWindowsPollResult;
#endif

    using Type = Async::Type;

    int32_t index = 0;

    AsyncResult(int32_t index, ReturnCode&& res) : index(index), returnCode(forward<ReturnCode>(res)) {}

    void reactivateRequest(bool value) { shouldBeReactivated = value; }

    const ReturnCode& isValid() const { return returnCode; }

  protected:
    friend struct EventLoop;

    bool shouldBeReactivated = false;

    ReturnCode returnCode;
};

struct SC::AsyncLoopTimeoutResult : public AsyncResult
{
    AsyncLoopTimeoutResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asLoopTimeout())
    {}

    AsyncLoopTimeout& async;
};

struct SC::AsyncLoopWakeUpResult : public AsyncResult
{
    AsyncLoopWakeUpResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asLoopWakeUp())
    {}

    AsyncLoopWakeUp& async;
};

struct SC::AsyncProcessExitResult : public AsyncResult
{
    AsyncProcessExitResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asProcessExit())
    {}

    AsyncProcessExit& async;

    [[nodiscard]] ReturnCode moveTo(ProcessDescriptor::ExitStatus& status)
    {
        status = exitStatus;
        return AsyncResult::returnCode;
    }

  private:
    friend struct EventLoop;
    ProcessDescriptor::ExitStatus exitStatus;
};

struct SC::AsyncSocketAcceptResult : public AsyncResult
{
    AsyncSocketAcceptResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asSocketAccept())
    {}

    AsyncSocketAccept& async;

    [[nodiscard]] ReturnCode moveTo(SocketDescriptor& client)
    {
        SC_TRY_IF(AsyncResult::returnCode);
        return client.assign(move(acceptedClient));
    }

  private:
    friend struct EventLoop;
    SocketDescriptor acceptedClient;
};

struct SC::AsyncSocketConnectResult : public AsyncResult
{
    AsyncSocketConnectResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asSocketConnect())
    {}

    AsyncSocketConnect& async;
};

struct SC::AsyncSocketSendResult : public AsyncResult
{
    AsyncSocketSendResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asSocketSend())
    {}

    AsyncSocketSend& async;
};

struct SC::AsyncSocketReceiveResult : public AsyncResult
{
    AsyncSocketReceiveResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asSocketReceive())
    {}
    AsyncSocketReceive& async;

    [[nodiscard]] ReturnCode moveTo(Span<char>& data)
    {
        data = readData;
        return AsyncResult::returnCode;
    }

  private:
    friend struct EventLoop;
    Span<char> readData;
};

struct SC::AsyncFileReadResult : public AsyncResult
{
    AsyncFileReadResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asFileRead())
    {}
    AsyncFileRead& async;

    [[nodiscard]] ReturnCode moveTo(Span<char>& data)
    {
        data = readData;
        return AsyncResult::returnCode;
    }

  private:
    friend struct EventLoop;
    Span<char> readData;
};

struct SC::AsyncFileWriteResult : public AsyncResult
{
    AsyncFileWriteResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asFileWrite())
    {}

    AsyncFileWrite& async;

    [[nodiscard]] ReturnCode moveTo(size_t& writtenSizeInBytes)
    {
        writtenSizeInBytes = writtenBytes;
        return AsyncResult::returnCode;
    }

  private:
    friend struct EventLoop;
    size_t writtenBytes = 0;
};

#if SC_PLATFORM_WINDOWS
struct SC::AsyncWindowsPollResult : public AsyncResult
{
    AsyncWindowsPollResult(Async& async, int32_t index, ReturnCode&& res)
        : AsyncResult(index, forward<ReturnCode>(res)), async(*async.asWindowsPoll())
    {}

    AsyncWindowsPoll& async;
};
#endif
// clang-format off
inline SC::AsyncLoopTimeout*    SC::Async::asLoopTimeout()  { return type == Type::LoopTimeout ? static_cast<AsyncLoopTimeout*>(this) : nullptr;}
inline SC::AsyncLoopWakeUp*     SC::Async::asLoopWakeUp()   { return type == Type::LoopWakeUp ? static_cast<AsyncLoopWakeUp*>(this) : nullptr;}
inline SC::AsyncProcessExit*    SC::Async::asProcessExit()  { return type == Type::ProcessExit ? static_cast<AsyncProcessExit*>(this) : nullptr;}
inline SC::AsyncSocketAccept*   SC::Async::asSocketAccept() { return type == Type::SocketAccept ? static_cast<AsyncSocketAccept*>(this) : nullptr;}
inline SC::AsyncSocketConnect*  SC::Async::asSocketConnect(){ return type == Type::SocketConnect ? static_cast<AsyncSocketConnect*>(this) : nullptr;}
inline SC::AsyncSocketSend*     SC::Async::asSocketSend()   { return type == Type::SocketSend ? static_cast<AsyncSocketSend*>(this) : nullptr; }
inline SC::AsyncSocketReceive*  SC::Async::asSocketReceive(){ return type == Type::SocketReceive ? static_cast<AsyncSocketReceive*>(this) : nullptr;}
inline SC::AsyncFileRead*       SC::Async::asFileRead()     { return type == Type::FileRead ? static_cast<AsyncFileRead*>(this) : nullptr; }
inline SC::AsyncFileWrite*      SC::Async::asFileWrite()    { return type == Type::FileWrite ? static_cast<AsyncFileWrite*>(this) : nullptr; }
#if SC_PLATFORM_WINDOWS
inline SC::AsyncWindowsPoll*    SC::Async::asWindowsPoll()  { return type == Type::WindowsPoll ? static_cast<AsyncWindowsPoll*>(this) : nullptr; }
#endif
// clang-format on

struct SC::EventLoop
{
    // Creation
    /// Creates the event loop kernel object
    [[nodiscard]] ReturnCode create();

    /// Closes the event loop kernel object
    [[nodiscard]] ReturnCode close();

    // Execution
    /// Runs until there are no more active handles left.
    [[nodiscard]] ReturnCode run();
    /// Runs just a single step. Ensures forward progress. If there are no events it blocks.
    [[nodiscard]] ReturnCode runOnce();
    /// Runs just a single step without forward progress. If there are no events returns immediately.
    [[nodiscard]] ReturnCode runNoWait();

    // Stop any async operation
    [[nodiscard]] ReturnCode stopAsync(Async& async);

    // Start specific async operation request.
    // Every async operation takes a callback as parameter that is invoked when the request is fullfilled.
    // If the function returns a valid (non error) Return code, then the user callback will be called both
    // in case of success and in case of any error.
    // If the function returns an invalid Return code, then the user callback will not be called.
    // The memory address of all Async* objects must be stable for the entire duration of a started async request,
    // that means they can be freed / moved after the user callback is executed.

    /// Starts a Timeout that is invoked after expiration (relative) time has passed.
    [[nodiscard]] ReturnCode startLoopTimeout(AsyncLoopTimeout& async, IntegerMilliseconds expiration,
                                              Function<void(AsyncLoopTimeoutResult&)>&& callback);

    /// Starts a wake up request, that will be fullfilled when an external thread calls wakeUpFromExternalThread.
    /// EventObject is optional and allows the external thread to wait until the user callback has completed execution.
    [[nodiscard]] ReturnCode startLoopWakeUp(AsyncLoopWakeUp& async, Function<void(AsyncLoopWakeUpResult&)>&& callback,
                                             EventObject* eventObject = nullptr);

    /// Starts a process exit notification request, that will be fullfilled when the given process is exited.
    [[nodiscard]] ReturnCode startProcessExit(AsyncProcessExit&                         async,
                                              Function<void(AsyncProcessExitResult&)>&& callback,
                                              ProcessDescriptor::Handle                 process);

    /// Starts a socket accept operation, that will return a new socket connected to the given listening endpoint.
    /// SocketDescriptor must be created with async flags (createAsyncTCPSocket) and already bound and listening.
    [[nodiscard]] ReturnCode startSocketAccept(AsyncSocketAccept& async, const SocketDescriptor& socketDescriptor,
                                               Function<void(AsyncSocketAcceptResult&)>&& callback);

    /// Starts a socket connect operation. Callback will be called when the given socket is connected to ipAddress.
    [[nodiscard]] ReturnCode startSocketConnect(AsyncSocketConnect& async, const SocketDescriptor& socketDescriptor,
                                                SocketIPAddress                             ipAddress,
                                                Function<void(AsyncSocketConnectResult&)>&& callback);

    /// Starts a socket send operation. Callback will be called when the given socket is ready to send more data.
    [[nodiscard]] ReturnCode startSocketSend(AsyncSocketSend& async, const SocketDescriptor& socketDescriptor,
                                             Span<const char> data, Function<void(AsyncSocketSendResult&)>&& callback);

    /// Starts a socket receive operation. Callback will be called when some data is read from socket.
    [[nodiscard]] ReturnCode startSocketReceive(AsyncSocketReceive& async, const SocketDescriptor& socketDescriptor,
                                                Span<char> data, Function<void(AsyncSocketReceiveResult&)>&& callback);

    /// Starts a file receive operation, that will return when some data is read from file.
    [[nodiscard]] ReturnCode startFileRead(AsyncFileRead& async, FileDescriptor::Handle fileDescriptor,
                                           Span<char> readBuffer, Function<void(AsyncFileReadResult&)>&& callback);

    /// Starts a file receive operation, that will return when the file is ready to receive more bytes to write.
    [[nodiscard]] ReturnCode startFileWrite(AsyncFileWrite& async, FileDescriptor::Handle fileDescriptor,
                                            Span<const char>                        writeBuffer,
                                            Function<void(AsyncFileWriteResult&)>&& callback);

#if SC_PLATFORM_WINDOWS
    [[nodiscard]] ReturnCode startWindowsPoll(AsyncWindowsPoll& async, FileDescriptor::Handle fileDescriptor,
                                              Function<void(AsyncWindowsPollResult&)>&& callback);
#endif
    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked).
    /// The parameter is an AsyncLoopWakeUp that must have been previously started (with startLoopWakeUp).
    [[nodiscard]] ReturnCode wakeUpFromExternalThread(AsyncLoopWakeUp& wakeUp);

    /// Wake up the event loop from a thread different than the one where run() is called (and potentially blocked)
    [[nodiscard]] ReturnCode wakeUpFromExternalThread();

    /// Helper to creates a TCP socket with Async flags of the given family (IPV4 / IPV6).
    /// It also automatically registers the socket with the eventLoop (associateExternallyCreatedTCPSocket)
    ReturnCode createAsyncTCPSocket(SocketFlags::AddressFamily family, SocketDescriptor& outDescriptor);

    /// Associates a TCP Socket created externally (without using createAsyncTCPSocket) with the eventloop.
    ReturnCode associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor);

    /// Associates a File descriptor created externally with the eventloop.
    ReturnCode associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor);

    // Advanced usages

    /// Returns handle to the kernel level IO queue object.
    /// Used by external systems calling OS async function themselves (FileSystemWatcher on windows for example)
    [[nodiscard]] ReturnCode getLoopFileDescriptor(FileDescriptor::Handle& fileDescriptor) const;

    /// Manually increases active handles count for external system calling OS async function themselves.
    void increaseActiveCount();

    /// Manually decreases active handles count for external system calling OS async function themselves.
    void decreaseActiveCount();

  private:
    int numberOfActiveHandles = 0;
    int numberOfTimers        = 0;
    int numberOfWakeups       = 0;
    int numberOfExternals     = 0;

    IntrusiveDoubleLinkedList<Async> submissions;
    IntrusiveDoubleLinkedList<Async> activeTimers;
    IntrusiveDoubleLinkedList<Async> activeWakeUps;

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

    int getTotalNumberOfActiveHandle() const;

    void removeActiveHandle(Async& async);
    void addActiveHandle(Async& async);

    // Timers
    [[nodiscard]] const TimeCounter* findEarliestTimer() const;

    void invokeExpiredTimers();
    void updateTime();
    void executeTimers(KernelQueue& queue, const TimeCounter& nextTimer);

    // LoopWakeUp
    void executeWakeUps(AsyncResult& result);

    // Setup
    [[nodiscard]] ReturnCode validateAsync(Async& async);
    [[nodiscard]] ReturnCode queueSubmission(Async& async);

    // Phases
    [[nodiscard]] ReturnCode stageSubmission(KernelQueue& queue, Async& async);
    [[nodiscard]] ReturnCode setupAsync(KernelQueue& queue, Async& async);
    [[nodiscard]] ReturnCode activateAsync(KernelQueue& queue, Async& async);
    [[nodiscard]] ReturnCode cancelAsync(KernelQueue& queue, Async& async);

    void        reportError(KernelQueue& queue, Async& async, ReturnCode&& returnCode);
    static void completeAsync(KernelQueue& queue, Async& async, int32_t eventIndex, ReturnCode&& returnCode,
                              bool& reactivate);

    enum class PollMode
    {
        NoWait,
        ForcedForwardProgress
    };
    [[nodiscard]] ReturnCode runStep(PollMode pollMode);
};
