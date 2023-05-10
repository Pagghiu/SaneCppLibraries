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
#include "../Networking/SocketDescriptor.h"
#include "../Process/ProcessDescriptor.h"
#include "../System/FileDescriptor.h"

namespace SC
{
struct EventObject;
struct EventLoop;

struct Async;
struct AsyncResult;

// Operations
struct AsyncTimeout;
struct AsyncRead;
struct AsyncWakeUp;
struct AsyncProcessExit;
struct AsyncAccept;
struct AsyncConnect;
struct AsyncSend;
struct AsyncReceive;
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
} // namespace SC

struct SC::Async
{
    struct ProcessExitInternal;
    struct ProcessExitSizes
    {
        static constexpr int Windows = sizeof(void*) * 7;
        static constexpr int Apple   = sizeof(void*);
        static constexpr int Default = sizeof(void*);
    };
    using ProcessExitTraits = OpaqueTraits<ProcessExitInternal, ProcessExitSizes>;
    using ProcessExitOpaque = OpaqueUniqueObject<OpaqueFuncs<ProcessExitTraits>>;

    struct Timeout
    {
        IntegerMilliseconds timeout; // not needed, but keeping just for debugging
        TimeCounter         expirationTime;
    };

    struct Read
    {
        FileDescriptor::Handle fileDescriptor;
        Span<uint8_t>          readBuffer;
    };

    struct WakeUp
    {
        EventObject* eventObject = nullptr;
        Atomic<bool> pending     = false;
    };

    struct ProcessExit
    {
        ProcessDescriptor::Handle handle = ProcessDescriptor::Invalid;
        ProcessExitOpaque         opaque; // TODO: We should make this a pointer as it's too big on Win
    };

    struct AcceptSupport
    {
#if SC_PLATFORM_WINDOWS
        SocketDescriptor             clientSocket;
        EventLoopWinOverlappedOpaque overlapped;
        uint8_t                      acceptBuffer[288];
#endif
    };

    struct Accept
    {
        SocketDescriptor::Handle   handle        = SocketDescriptor::Invalid;
        SocketFlags::AddressFamily addressFamily = SocketFlags::AddressFamilyIPV4;
        AcceptSupport*             support       = nullptr;
    };

    struct ConnectSupport
    {
        SocketIPAddress ipAddress;
#if SC_PLATFORM_WINDOWS
        EventLoopWinOverlappedOpaque overlapped;
#endif
    };

    struct Connect
    {
        SocketDescriptor::Handle handle  = SocketDescriptor::Invalid;
        ConnectSupport*          support = nullptr;
    };

    struct SendSupport
    {
#if SC_PLATFORM_WINDOWS
        EventLoopWinOverlappedOpaque overlapped;
#endif
    };

    struct Send
    {
        SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
        Span<const char>         data;
        SendSupport*             support = nullptr;
    };

    struct ReceiveSupport
    {
#if SC_PLATFORM_WINDOWS
        EventLoopWinOverlappedOpaque overlapped;
#endif
    };

    struct Receive
    {
        SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
        Span<char>               data;
        ReceiveSupport*          support = nullptr;
    };

    enum class Type
    {
        Timeout,
        Read,
        WakeUp,
        ProcessExit,
        Accept,
        Connect,
        Send,
        Receive,
    };

    union Operation
    {
        Operation() {}
        ~Operation() {}

        Timeout     timeout;
        Read        read;
        WakeUp      wakeUp;
        ProcessExit processExit;
        Accept      accept;
        Connect     connect;
        Send        send;
        Receive     receive;

        using FieldsTypes =
            TypeList<TaggedField<Operation, Type, decltype(timeout), &Operation::timeout, Type::Timeout>,
                     TaggedField<Operation, Type, decltype(read), &Operation::read, Type::Read>,
                     TaggedField<Operation, Type, decltype(wakeUp), &Operation::wakeUp, Type::WakeUp>,
                     TaggedField<Operation, Type, decltype(processExit), &Operation::processExit, Type::ProcessExit>,
                     TaggedField<Operation, Type, decltype(accept), &Operation::accept, Type::Accept>,
                     TaggedField<Operation, Type, decltype(connect), &Operation::connect, Type::Connect>,
                     TaggedField<Operation, Type, decltype(send), &Operation::send, Type::Send>,
                     TaggedField<Operation, Type, decltype(receive), &Operation::receive, Type::Receive>>;
    };

    enum class State
    {
        Free,       // not in any queue
        Active,     // when monitored by OS syscall
        Submitting, // when in submission queue
        Cancelling  // when in cancellation queue (TBD)
    };

    EventLoop* eventLoop = nullptr;
    Async*     next      = nullptr;
    Async*     prev      = nullptr;
    State      state     = State::Free;

    TaggedUnion<Operation>       operation;
    Function<void(AsyncResult&)> callback;
};

struct SC::AsyncResult
{
    struct Timeout
    {
    };

    struct Read
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
    using Type = Async::Type;

    union Result
    {
        Result() {}
        ~Result() {}

        Timeout     timeout;
        Read        read;
        WakeUp      wakeUp;
        ProcessExit processExit;
        Accept      accept;
        Connect     connect;
        Send        send;
        Receive     receive;

        using FieldsTypes =
            TypeList<TaggedField<Result, Type, decltype(timeout), &Result::timeout, Type::Timeout>,
                     TaggedField<Result, Type, decltype(read), &Result::read, Type::Read>,
                     TaggedField<Result, Type, decltype(wakeUp), &Result::wakeUp, Type::WakeUp>,
                     TaggedField<Result, Type, decltype(processExit), &Result::processExit, Type::ProcessExit>,
                     TaggedField<Result, Type, decltype(accept), &Result::accept, Type::Accept>,
                     TaggedField<Result, Type, decltype(connect), &Result::connect, Type::Connect>,
                     TaggedField<Result, Type, decltype(send), &Result::send, Type::Send>,
                     TaggedField<Result, Type, decltype(receive), &Result::receive, Type::Receive>>;
    };

    EventLoop& eventLoop;
    Async&     async;
    void*      userData = nullptr;

    TaggedUnion<Result> result = {};

    // TODO: Add AsyncResult error
};

struct SC::AsyncTimeout : public Async
{
};

struct SC::AsyncRead : public Async
{
};

struct SC::AsyncWakeUp : public Async
{
    [[nodiscard]] ReturnCode wakeUp();
};

struct SC::AsyncProcessExit : public Async
{
};

struct SC::AsyncAccept : public Async
{
    using Support = Async::AcceptSupport;
};

struct SC::AsyncConnect : public Async
{
    using Support = Async::ConnectSupport;
};

struct SC::AsyncSend : public Async
{
    using Support = Async::SendSupport;
};

struct SC::AsyncReceive : public Async
{
    using Support = Async::ReceiveSupport;
};

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

    [[nodiscard]] ReturnCode startRead(AsyncRead& async, FileDescriptor::Handle fileDescriptor,
                                       Span<uint8_t> readBuffer, Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startWakeUp(AsyncWakeUp& async, Function<void(AsyncResult&)>&& callback,
                                         EventObject* eventObject = nullptr);

    [[nodiscard]] ReturnCode startProcessExit(AsyncProcessExit& async, Function<void(AsyncResult&)>&& callback,
                                              ProcessDescriptor::Handle process);

    [[nodiscard]] ReturnCode startAccept(AsyncAccept& async, Async::AcceptSupport& support,
                                         const SocketDescriptor&        socketDescriptor,
                                         Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startConnect(AsyncConnect& async, Async::ConnectSupport& support,
                                          const SocketDescriptor& socketDescriptor, SocketIPAddress ipAddress,
                                          Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startSend(AsyncSend& async, AsyncSend::SendSupport& support,
                                       const SocketDescriptor& socketDescriptor, Span<const char> data,
                                       Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode startReceive(AsyncReceive& async, AsyncReceive::ReceiveSupport& support,
                                          const SocketDescriptor& socketDescriptor, Span<char> data,
                                          Function<void(AsyncResult&)>&& callback);

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
