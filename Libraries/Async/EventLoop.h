// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Atomic.h"
#include "../Foundation/Function.h"
#include "../Foundation/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Limits.h"
#include "../Foundation/Optional.h"
#include "../Foundation/Span.h"
#include "../Foundation/TaggedUnion.h"
#include "../Foundation/Time.h"
#include "../InputOutput/FileDescriptor.h"

namespace SC
{
struct EventLoop;
struct Async;
struct AsyncResult;
struct EventObject;
} // namespace SC

struct SC::AsyncResult
{
    EventLoop& loop;
    Async&     async;
    // TODO: Add AsyncResult error
};

struct SC::Async
{
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

    union Operation
    {
        enum class Type
        {
            Timeout,
            Read
        };

        Operation() {}
        ~Operation() {}

        Timeout timeout;
        Read    read;

        using FieldsTypes =
            TypeList<TaggedField<Operation, Type, decltype(timeout), &Operation::timeout, Type::Timeout>,
                     TaggedField<Operation, Type, decltype(read), &Operation::read, Type::Read>>;
    };

    enum class State
    {
        Free,       // not in any queue
        Active,     // when monitored by OS syscall
        Submitting, // when in submission queue
        Cancelling  // when in cancellation queue (TBD)
    };

    Async* next  = nullptr;
    Async* prev  = nullptr;
    State  state = State::Free;

    TaggedUnion<Operation>       operation;
    Function<void(AsyncResult&)> callback;
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
    [[nodiscard]] ReturnCode addTimeout(IntegerMilliseconds expiration, Async& async,
                                        Function<void(AsyncResult&)>&& callback);
    [[nodiscard]] ReturnCode addRead(Async::Read readOp, Async& async);

    // Operations
    struct ExternalThreadNotifier
    {
        ExternalThreadNotifier*    next = nullptr;
        ExternalThreadNotifier*    prev = nullptr;
        Function<void(EventLoop&)> callback;
        Atomic<bool>               pending     = false;
        EventLoop*                 eventLoop   = nullptr;
        EventObject*               eventObject = nullptr;
    };
    [[nodiscard]] ReturnCode initNotifier(ExternalThreadNotifier& notifier, Function<void(EventLoop&)>&& callback,
                                          EventObject* eventObject = nullptr);
    [[nodiscard]] ReturnCode notifyFromExternalThread(ExternalThreadNotifier& notifier);
    void                     removeNotifier(ExternalThreadNotifier& notifier);

    [[nodiscard]] ReturnCode wakeUpFromExternalThread();

  private:
    IntrusiveDoubleLinkedList<Async> submission;
    IntrusiveDoubleLinkedList<Async> activeTimers;
    IntrusiveDoubleLinkedList<Async> stagedHandles;

    IntrusiveDoubleLinkedList<ExternalThreadNotifier> notifiers;

    TimeCounter loopTime;

    struct Internal;
    struct KernelQueue;
    static constexpr int InternalSize      = 1024;
    static constexpr int InternalAlignment = alignof(void*);
    template <typename T, int N, int Alignment>
    friend struct OpaqueFunctions;
    OpaqueUniqueObject<Internal, InternalSize, InternalAlignment> internal;

    void invokeExpiredTimers();
    void updateTime() { loopTime.snap(); }
    void submitAsync(Async& async);

    void runCompletionForNotifiers();

    [[nodiscard]] bool               shouldQuit();
    [[nodiscard]] const TimeCounter* findEarliestTimer() const;
    [[nodiscard]] ReturnCode         stageSubmissions(KernelQueue& queue);
};
