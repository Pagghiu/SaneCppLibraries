// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Threading.h"

SC::Mutex::Mutex() { ::InitializeCriticalSection(&mutex.reinterpret_as<CRITICAL_SECTION>()); }
SC::Mutex::~Mutex() { ::DeleteCriticalSection(&mutex.reinterpret_as<CRITICAL_SECTION>()); }
void SC::Mutex::lock() { ::EnterCriticalSection(&mutex.reinterpret_as<CRITICAL_SECTION>()); }
void SC::Mutex::unlock() { ::LeaveCriticalSection(&mutex.reinterpret_as<CRITICAL_SECTION>()); }

SC::ConditionVariable::ConditionVariable()
{
    ::InitializeConditionVariable(&condition.reinterpret_as<CONDITION_VARIABLE>());
}
SC::ConditionVariable::~ConditionVariable() {} // Nothing to do
void SC::ConditionVariable::wait(Mutex& mutex)
{
    ::SleepConditionVariableCS(&condition.reinterpret_as<CONDITION_VARIABLE>(),
                               &mutex.mutex.reinterpret_as<CRITICAL_SECTION>(), INFINITE);
}
void SC::ConditionVariable::signal() { ::WakeConditionVariable(&condition.reinterpret_as<CONDITION_VARIABLE>()); }
void SC::ConditionVariable::broadcast() { ::WakeAllConditionVariable(&condition.reinterpret_as<CONDITION_VARIABLE>()); }

struct SC::Thread::Internal
{
    using NativeHandle       = HANDLE;
    using CallbackReturnType = DWORD;
    static CallbackReturnType WINAPI threadFunc(void* argument)
    {
        Thread& self = *static_cast<Thread*>(argument);
        self.userFunction(self);
        return 0;
    }

    [[nodiscard]] static Result createThread(Thread& self, OpaqueThread& opaqueThread,
                                             DWORD(WINAPI* threadFunc)(void* argument))
    {
        DWORD   threadID;
        HANDLE& threadHandle = opaqueThread.reinterpret_as<HANDLE>();
        threadHandle         = ::CreateThread(0, 512 * 1024, threadFunc, &self, CREATE_SUSPENDED, &threadID);
        if (threadHandle == nullptr)
        {
            return Result::Error("Thread::create - CreateThread failed");
        }
        ResumeThread(threadHandle);
        return Result(true);
    }

    static void setThreadName(const wchar_t* nameNullTerminated)
    {
        ::SetThreadDescription(::GetCurrentThread(), nameNullTerminated);
    }

    [[nodiscard]] static Result joinThread(OpaqueThread& threadNative)
    {
        ::WaitForSingleObject(threadNative.reinterpret_as<HANDLE>(), INFINITE);
        ::CloseHandle(threadNative.reinterpret_as<HANDLE>());
        return Result(true);
    }

    [[nodiscard]] static Result detachThread(OpaqueThread& threadNative)
    {
        CloseHandle(threadNative.reinterpret_as<HANDLE>());
        return Result(true);
    }
};

void SC::Thread::Sleep(uint32_t milliseconds) { ::Sleep(milliseconds); }

SC::uint64_t SC::Thread::CurrentThreadID() { return ::GetCurrentThreadId(); }

SC::uint64_t SC::Thread::threadID()
{
    OpaqueThread* threadNative = nullptr;
    if (thread.get(threadNative))
    {
        return ::GetThreadId(threadNative->reinterpret_as<HANDLE>());
    }
    return 0;
}
