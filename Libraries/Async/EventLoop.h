// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Function.h"
#include "../Foundation/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Span.h"
#include "../Foundation/TaggedUnion.h"
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

struct Async;
struct AsyncResult;

// Operations
struct AsyncTimeout;
struct AsyncWakeUp;
struct AsyncProcessExit;
struct AsyncAccept;
struct AsyncConnect;
struct AsyncSend;
struct AsyncReceive;
struct AsyncRead;
struct AsyncWrite;

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

struct SC::Async
{
    using Timeout     = AsyncTimeout;
    using WakeUp      = AsyncWakeUp;
    using ProcessExit = AsyncProcessExit;
    using Accept      = AsyncAccept;
    using Connect     = AsyncConnect;
    using Send        = AsyncSend;
    using Receive     = AsyncReceive;
    using Read        = AsyncRead;
    using Write       = AsyncWrite;

    AsyncTimeout*     asTimeout();
    AsyncWakeUp*      asWakeUp();
    AsyncProcessExit* asProcessExit();
    AsyncAccept*      asAccept();
    AsyncConnect*     asConnect();
    AsyncSend*        asSend();
    AsyncReceive*     asReceive();
    AsyncRead*        asRead();
    AsyncWrite*       asWrite();

    enum class Type : uint8_t
    {
        Timeout,
        WakeUp,
        ProcessExit,
        Accept,
        Connect,
        Send,
        Receive,
        Read,
        Write,
    };

    enum class State : uint8_t
    {
        Free,       // not in any queue
        Active,     // when monitored by OS syscall
        Submitting, // when in submission queue
        Cancelling  // when in cancellation queue
    };

    Async(Type type) : type(type) {}

    Type getType() const { return type; }

    Function<void(AsyncResult&)> callback;

    EventLoop* eventLoop = nullptr;
    Async*     next      = nullptr;
    Async*     prev      = nullptr;

    State state = State::Free;

  private:
    Type type = Type::Timeout;
};

struct SC::AsyncResult
{
    struct Timeout
    {
    };

    struct WakeUp
    {
    };

    struct ProcessExit
    {
        ProcessDescriptor::ExitStatus exitStatus;
    };

    struct Accept
    {
        SocketDescriptor acceptedClient;
    };

    struct Connect
    {
    };

    struct Send
    {
    };

    struct Receive
    {
    };

    struct Read
    {
        size_t readBytes = 0;
    };

    struct Write
    {
        size_t writtenBytes = 0;
    };
    using Type = Async::Type;

    union Result
    {
        Result() {}
        ~Result() {}

        Timeout     timeout;
        WakeUp      wakeUp;
        ProcessExit processExit;
        Accept      accept;
        Connect     connect;
        Send        send;
        Receive     receive;
        Read        read;
        Write       write;

        using FieldsTypes =
            TypeList<TaggedField<Result, Type, decltype(timeout), &Result::timeout, Type::Timeout>,
                     TaggedField<Result, Type, decltype(wakeUp), &Result::wakeUp, Type::WakeUp>,
                     TaggedField<Result, Type, decltype(processExit), &Result::processExit, Type::ProcessExit>,
                     TaggedField<Result, Type, decltype(accept), &Result::accept, Type::Accept>,
                     TaggedField<Result, Type, decltype(connect), &Result::connect, Type::Connect>,
                     TaggedField<Result, Type, decltype(send), &Result::send, Type::Send>,
                     TaggedField<Result, Type, decltype(receive), &Result::receive, Type::Receive>,
                     TaggedField<Result, Type, decltype(read), &Result::read, Type::Read>,
                     TaggedField<Result, Type, decltype(write), &Result::write, Type::Write>>;
    };

    EventLoop& eventLoop;
    Async&     async;
    void*      userData = nullptr;

    TaggedUnion<Result> result = {};

    // TODO: Add AsyncResult error
};

struct SC::AsyncTimeout : public Async
{
    AsyncTimeout() : Async(Type::Timeout) {}

    IntegerMilliseconds timeout; // not needed, but keeping just for debugging
    TimeCounter         expirationTime;
};

struct SC::AsyncWakeUp : public Async
{
    AsyncWakeUp() : Async(Type::WakeUp) {}

    [[nodiscard]] ReturnCode wakeUp();

    EventObject* eventObject = nullptr;
    Atomic<bool> pending     = false;
};

struct SC::AsyncProcessExit : public Async
{
    AsyncProcessExit() : Async(Type::ProcessExit) {}

    ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
    EventLoopWinWaitHandle       waitHandle;
#endif
};

struct SC::AsyncAccept : public Async
{
    AsyncAccept() : Async(Type::Accept) {}

    SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
    SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
    SocketDescriptor             clientSocket;
    uint8_t                      acceptBuffer[288];
#endif
};

struct SC::AsyncConnect : public Async
{
    AsyncConnect() : Async(Type::Connect) {}

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    SocketIPAddress          ipAddress;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

struct SC::AsyncSend : public Async
{
    AsyncSend() : Async(Type::Send) {}

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<const char>         data;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

struct SC::AsyncReceive : public Async
{
    AsyncReceive() : Async(Type::Receive) {}

    SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
    Span<char>               data;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

struct SC::AsyncRead : public Async
{
    AsyncRead() : Async(Type::Read) {}

    FileDescriptor::Handle fileDescriptor;
    uint64_t               offset = 0;
    SpanVoid<void>         readBuffer;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

struct SC::AsyncWrite : public Async
{
    AsyncWrite() : Async(Type::Write) {}

    FileDescriptor::Handle fileDescriptor;
    uint64_t               offset = 0;
    SpanVoid<const void>   writeBuffer;
#if SC_PLATFORM_WINDOWS
    EventLoopWinOverlappedOpaque overlapped;
#endif
};

inline SC::AsyncTimeout* SC::Async::asTimeout()
{
    return type == Type::Timeout ? static_cast<AsyncTimeout*>(this) : nullptr;
}
inline SC::AsyncWakeUp* SC::Async::asWakeUp()
{
    return type == Type::WakeUp ? static_cast<AsyncWakeUp*>(this) : nullptr;
}
inline SC::AsyncProcessExit* SC::Async::asProcessExit()
{
    return type == Type::ProcessExit ? static_cast<AsyncProcessExit*>(this) : nullptr;
}
inline SC::AsyncAccept* SC::Async::asAccept()
{
    return type == Type::Accept ? static_cast<AsyncAccept*>(this) : nullptr;
}
inline SC::AsyncConnect* SC::Async::asConnect()
{
    return type == Type::Connect ? static_cast<AsyncConnect*>(this) : nullptr;
}
inline SC::AsyncSend*    SC::Async::asSend() { return type == Type::Send ? static_cast<AsyncSend*>(this) : nullptr; }
inline SC::AsyncReceive* SC::Async::asReceive()
{
    return type == Type::Receive ? static_cast<AsyncReceive*>(this) : nullptr;
}
inline SC::AsyncRead*  SC::Async::asRead() { return type == Type::Read ? static_cast<AsyncRead*>(this) : nullptr; }
inline SC::AsyncWrite* SC::Async::asWrite() { return type == Type::Write ? static_cast<AsyncWrite*>(this) : nullptr; }

struct SC::EventLoop
{
    // Creation
    [[nodiscard]] ReturnCode create();
    [[nodiscard]] ReturnCode close();

    // Execution
    [[nodiscard]] ReturnCode run();
    [[nodiscard]] ReturnCode runOnce();
    [[nodiscard]] ReturnCode runNoWait();

    // Stop any async operation
    [[nodiscard]] ReturnCode stopAsync(Async& async);

    // Start specific async operation
    [[nodiscard]] ReturnCode startTimeout(AsyncTimeout& async, IntegerMilliseconds expiration,
                                          Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startWakeUp(AsyncWakeUp& async, Function<void(AsyncResult&)>&& callback,
                                         EventObject* eventObject = nullptr);

    [[nodiscard]] ReturnCode startProcessExit(AsyncProcessExit& async, Function<void(AsyncResult&)>&& callback,
                                              ProcessDescriptor::Handle process);

    [[nodiscard]] ReturnCode startAccept(AsyncAccept& async, const SocketDescriptor& socketDescriptor,
                                         Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startConnect(AsyncConnect& async, const SocketDescriptor& socketDescriptor,
                                          SocketIPAddress ipAddress, Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startSend(AsyncSend& async, const SocketDescriptor& socketDescriptor,
                                       Span<const char> data, Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startReceive(AsyncReceive& async, const SocketDescriptor& socketDescriptor,
                                          Span<char> data, Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startRead(AsyncRead& async, FileDescriptor::Handle fileDescriptor,
                                       SpanVoid<void> readBuffer, Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startWrite(AsyncWrite& async, FileDescriptor::Handle fileDescriptor,
                                        SpanVoid<const void> writeBuffer, Function<void(AsyncResult&)>&& callback);

    // WakeUp support
    [[nodiscard]] ReturnCode wakeUpFromExternalThread(AsyncWakeUp& wakeUp);

    [[nodiscard]] ReturnCode wakeUpFromExternalThread();

    // Access Internals
    [[nodiscard]] ReturnCode getLoopFileDescriptor(FileDescriptor::Handle& fileDescriptor) const;

    void increaseActiveCount();
    void decreaseActiveCount();

    int getTotalNumberOfActiveHandle() const;

  private:
    int numberOfActiveHandles = 0;
    int numberOfTimers        = 0;
    int numberOfWakeups       = 0;
    int numberOfExternals     = 0;

    IntrusiveDoubleLinkedList<Async> submissions;
    IntrusiveDoubleLinkedList<Async> stagedHandles;
    IntrusiveDoubleLinkedList<Async> activeHandles;
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

    // Timers
    [[nodiscard]] const TimeCounter* findEarliestTimer() const;

    void invokeExpiredTimers();
    void updateTime();
    void executeTimers(KernelQueue& queue, const TimeCounter& nextTimer);

    // WakeUp
    void executeWakeUps();

    // Setup
    [[nodiscard]] ReturnCode queueSubmission(Async& async, Function<void(AsyncResult&)>&& callback);

    // Phases
    [[nodiscard]] ReturnCode stageSubmissions(KernelQueue& queue);

    enum class PollMode
    {
        NoWait,
        ForcedForwardProgress
    };
    [[nodiscard]] ReturnCode runStep(PollMode pollMode);
};
