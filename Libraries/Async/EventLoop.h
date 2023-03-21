// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Function.h"
#include "../Foundation/IntrusiveDoubleLinkedList.h"
#include "../Foundation/Limits.h"
#include "../Foundation/TaggedUnion.h"
#include "../Foundation/Time.h"
#include "../InputOutput/FileDescriptor.h"

namespace SC
{
struct EventLoop;
struct Async;
struct AsyncResult;
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
        // TODO: We should add also the read buffer to make it io_uring / completion style
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
    void addTimeout(IntegerMilliseconds expiration, Async& async, Function<void(AsyncResult&)>&& callback);
    void addRead(FileDescriptorNative fd, Async& async);

    // Operations
    [[nodiscard]] ReturnCode wakeUpFromExternalThread();

  private:
    void submitAsync(Async& async);

    IntrusiveDoubleLinkedList<Async> submission;

    TimeCounter loopTime;
    int         activeHandles = 0;

    struct Internal;
    static constexpr int InternalSize      = 1024;
    static constexpr int InternalAlignment = alignof(void*);
    template <typename T, int N, int Alignment>
    friend struct OpaqueFunctions;
    OpaqueUniqueObject<Internal, InternalSize, InternalAlignment> internal;

    [[nodiscard]] bool               shouldQuit();
    [[nodiscard]] const TimeCounter* findEarliestTimer() const;
    void                             invokeExpiredTimers();
    void                             updateTime() { loopTime.snap(); }
};
