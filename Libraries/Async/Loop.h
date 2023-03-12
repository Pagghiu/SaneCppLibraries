// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Foundation/Function.h"
#include "../Foundation/Limits.h"
#include "../Foundation/Time.h"
#include "../InputOutput/FileDescriptor.h"

namespace SC
{
struct Loop;
} // namespace SC

struct SC::Loop
{
    [[nodiscard]] ReturnCode create();
    [[nodiscard]] ReturnCode addTimer(IntegerMilliseconds expiration, Action&& callback);
    [[nodiscard]] ReturnCode run();
    [[nodiscard]] ReturnCode runOnce();
    [[nodiscard]] ReturnCode close();
    [[nodiscard]] ReturnCode wakeUpFromExternalThread();

  private:
    struct Timer
    {
        IntegerMilliseconds expiration;
        TimeCounter         expirationTime;
        Action              callback;
    };
    Vector<Timer> registeredTimers;
    TimeCounter   loopTime;
    struct Internal;
    static constexpr int InternalSize      = 1024;
    static constexpr int InternalAlignment = alignof(void*);
    template <typename T, int N, int Alignment>
    friend struct OpaqueFunctions;
    OpaqueUniqueObject<Internal, InternalSize, InternalAlignment> internal;

    [[nodiscard]] bool               shouldQuit() { return registeredTimers.isEmpty(); }
    [[nodiscard]] const TimeCounter* findEarliestTimer() const;
    void                             invokeExpiredTimers();
    void                             updateTime() { loopTime.snap(); }
};
