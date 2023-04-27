// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../FileSystem/FileDescriptor.h"
#include "../Foundation/Function.h"
#include "../Foundation/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Span.h"
#include "../Foundation/TaggedUnion.h"
#include "../Networking/SocketDescriptor.h"
#include "../System/ProcessDescriptor.h"
#include "../System/Time.h"
#include "../Threading/Atomic.h"

namespace SC
{
struct EventLoop;
struct Async;
struct AsyncTimeout;
struct AsyncRead;
struct AsyncWakeUp;
struct AsyncProcessExit;
struct AsyncAccept;
struct AsyncResult;
struct EventObject;
} // namespace SC

namespace SC
{
struct EventLoopWindowsOverlapped;
struct EventLoopWindowsOverlappedSizes
{
    static constexpr int Windows = sizeof(void*) * 7;
};
using EventLoopWindowsOverlappedTraits = OpaqueTraits<EventLoopWindowsOverlapped, EventLoopWindowsOverlappedSizes>;
using EventLoopWindowsOverlappedOpaque = OpaqueUniqueObject<OpaqueFuncs<EventLoopWindowsOverlappedTraits>>;
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
        FileDescriptorNative fileDescriptor;
        Span<uint8_t>        readBuffer;
    };

    struct WakeUp
    {
        EventObject* eventObject = nullptr;
        Atomic<bool> pending     = false;
    };

    struct ProcessExit
    {
        ProcessNative     handle = ProcessNativeInvalid;
        ProcessExitOpaque opaque; // TODO: We should make this a pointer as it's too big on Win
    };

    struct AcceptSupport
    {
#if SC_PLATFORM_WINDOWS
        SocketDescriptorNativeHandle     clientSocket;
        EventLoopWindowsOverlappedOpaque overlapped;
        uint8_t                          acceptBuffer[288];
#endif
    };
    struct Accept
    {
        SocketDescriptorNative handle  = SocketDescriptorNativeInvalid;
        AcceptSupport*         support = nullptr;
    };
    enum class Type
    {
        Timeout,
        Read,
        WakeUp,
        ProcessExit,
        Accept
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

        using FieldsTypes =
            TypeList<TaggedField<Operation, Type, decltype(timeout), &Operation::timeout, Type::Timeout>,
                     TaggedField<Operation, Type, decltype(read), &Operation::read, Type::Read>,
                     TaggedField<Operation, Type, decltype(wakeUp), &Operation::wakeUp, Type::WakeUp>,
                     TaggedField<Operation, Type, decltype(processExit), &Operation::processExit, Type::ProcessExit>,
                     TaggedField<Operation, Type, decltype(accept), &Operation::accept, Type::Accept>>;
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
        ProcessExitStatus exitStatus;
    };

    struct Accept
    {
        SocketDescriptorNative acceptedClient =
            SocketDescriptorNativeInvalid; // TODO: Make this SocketDescriptorNativeHandle
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

        using FieldsTypes =
            TypeList<TaggedField<Result, Type, decltype(timeout), &Result::timeout, Type::Timeout>,
                     TaggedField<Result, Type, decltype(read), &Result::read, Type::Read>,
                     TaggedField<Result, Type, decltype(wakeUp), &Result::wakeUp, Type::WakeUp>,
                     TaggedField<Result, Type, decltype(processExit), &Result::processExit, Type::ProcessExit>,
                     TaggedField<Result, Type, decltype(accept), &Result::accept, Type::Accept>>;
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
};

struct SC::EventLoop
{
    // Creation
    [[nodiscard]] ReturnCode create();
    [[nodiscard]] ReturnCode close();

    // Execution
    [[nodiscard]] ReturnCode run();
    [[nodiscard]] ReturnCode runOnce();

    // Async Operations
    [[nodiscard]] ReturnCode addTimeout(AsyncTimeout& async, IntegerMilliseconds expiration,
                                        Function<void(AsyncResult&)>&& callback);

    [[nodiscard]] ReturnCode addRead(AsyncRead& async, FileDescriptorNative fileDescriptor, Span<uint8_t> readBuffer);

    [[nodiscard]] ReturnCode addWakeUp(AsyncWakeUp& async, Function<void(AsyncResult&)>&& callback,
                                       EventObject* eventObject = nullptr);

    [[nodiscard]] ReturnCode addProcessExit(AsyncProcessExit& async, Function<void(AsyncResult&)>&& callback,
                                            ProcessNative process);

    [[nodiscard]] ReturnCode addAccept(AsyncAccept& async, Async::AcceptSupport& support,
                                       const SocketDescriptorNativeHandle& socketDescriptor,
                                       Function<void(AsyncResult&)>&&      callback);

    // WakeUp support
    [[nodiscard]] ReturnCode wakeUpFromExternalThread(AsyncWakeUp& wakeUp);

    [[nodiscard]] ReturnCode wakeUpFromExternalThread();

    [[nodiscard]] ReturnCode getLoopFileDescriptor(FileDescriptorNative& fileDescriptor) const;

  private:
    IntrusiveDoubleLinkedList<Async> submission;
    IntrusiveDoubleLinkedList<Async> stagedHandles;
    IntrusiveDoubleLinkedList<Async> activeTimers;
    IntrusiveDoubleLinkedList<Async> activeWakeUps;

    TimeCounter loopTime;

    struct KernelQueue;

    struct Internal;
    struct InternalSizes
    {
        static constexpr int Windows = 192;
        static constexpr int Apple   = 136;
        static constexpr int Default = sizeof(void*);
    };

  public:
    using InternalTraits = OpaqueTraits<Internal, InternalSizes>;

  private:
    using InternalOpaque = OpaqueUniqueObject<OpaqueFuncs<InternalTraits>>;
    InternalOpaque internal;

    void invokeExpiredTimers();
    void updateTime() { loopTime.snap(); }
    void submitAsync(Async& async);

    void                             runCompletionForNotifiers();
    void                             runCompletionFor(AsyncResult& result);
    [[nodiscard]] bool               shouldQuit();
    [[nodiscard]] const TimeCounter* findEarliestTimer() const;
    [[nodiscard]] ReturnCode         stageSubmissions(KernelQueue& queue);
};
