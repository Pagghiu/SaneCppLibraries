// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Foundation/StringView.h"
#include "Threading.h"

SC::Mutex::Mutex() { InitializeCriticalSection(&mutex.reinterpret_as<CRITICAL_SECTION>()); }
SC::Mutex::~Mutex() { InitializeCriticalSection(&mutex.reinterpret_as<CRITICAL_SECTION>()); }
void SC::Mutex::lock() { EnterCriticalSection(&mutex.reinterpret_as<CRITICAL_SECTION>()); }
void SC::Mutex::unlock() { LeaveCriticalSection(&mutex.reinterpret_as<CRITICAL_SECTION>()); }

SC::ConditionVariable::ConditionVariable()
{
    InitializeConditionVariable(&condition.reinterpret_as<CONDITION_VARIABLE>());
}
SC::ConditionVariable::~ConditionVariable() {} // Nothing to do
void SC::ConditionVariable::wait(Mutex& mutex)
{
    SleepConditionVariableCS(&condition.reinterpret_as<CONDITION_VARIABLE>(),
                             &mutex.mutex.reinterpret_as<CRITICAL_SECTION>(), INFINITE);
}
void SC::ConditionVariable::signal() { WakeConditionVariable(&condition.reinterpret_as<CONDITION_VARIABLE>()); }

struct SC::Thread::Internal
{
    using NativeHandle       = HANDLE;
    using CallbackReturnType = DWORD;

    [[nodiscard]] static ReturnCode createThread(CreateParams& self, OpaqueThread& opaqueThread, HANDLE& threadHandle,
                                                 DWORD(WINAPI* threadFunc)(void* argument))
    {
        DWORD threadID;
        opaqueThread.reinterpret_as<HANDLE>() =
            CreateThread(0, 512 * 1024, threadFunc, &self, CREATE_SUSPENDED, &threadID);
        if (opaqueThread.reinterpret_as<HANDLE>() == nullptr)
        {
            return "Thread::create - CreateThread failed"_a8;
        }
        threadHandle = opaqueThread.reinterpret_as<HANDLE>();
        ResumeThread(threadHandle);
        return true;
    }

    static void setThreadName(HANDLE& threadHandle, const StringView& nameNullTerminated)
    {
        SetThreadDescription(threadHandle, nameNullTerminated.getNullTerminatedNative());
    }

    [[nodiscard]] static ReturnCode joinThread(OpaqueThread* threadNative)
    {
        WaitForSingleObject(threadNative->reinterpret_as<HANDLE>(), INFINITE);
        CloseHandle(threadNative->reinterpret_as<HANDLE>());
        return true;
    }

    [[nodiscard]] static ReturnCode detachThread(OpaqueThread* threadNative)
    {
        CloseHandle(threadNative->reinterpret_as<HANDLE>());
        return true;
    }
};

void SC::Thread::Sleep(uint32_t milliseconds) { ::Sleep(milliseconds); }

SC::uint64_t SC::Thread::CurrentThreadID() { return ::GetCurrentThreadId(); }
