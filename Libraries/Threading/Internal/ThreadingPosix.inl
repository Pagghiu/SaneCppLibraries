// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Foundation/Strings/StringView.h"
#include "../Threading.h"

#include <pthread.h>
#include <unistd.h> // usleep
SC::Mutex::Mutex() { pthread_mutex_init(&mutex.reinterpret_as<pthread_mutex_t>(), 0); }
SC::Mutex::~Mutex() { pthread_mutex_destroy(&mutex.reinterpret_as<pthread_mutex_t>()); }
void SC::Mutex::lock() { pthread_mutex_lock(&mutex.reinterpret_as<pthread_mutex_t>()); }
void SC::Mutex::unlock() { pthread_mutex_unlock(&mutex.reinterpret_as<pthread_mutex_t>()); }

SC::ConditionVariable::ConditionVariable() { pthread_cond_init(&condition.reinterpret_as<pthread_cond_t>(), 0); }
SC::ConditionVariable::~ConditionVariable() { pthread_cond_destroy(&condition.reinterpret_as<pthread_cond_t>()); }
void SC::ConditionVariable::wait(Mutex& mutex)
{
    pthread_cond_wait(&condition.reinterpret_as<pthread_cond_t>(), &mutex.mutex.reinterpret_as<pthread_mutex_t>());
}
void SC::ConditionVariable::signal() { pthread_cond_signal(&condition.reinterpret_as<pthread_cond_t>()); }

struct SC::Thread::Internal
{
    using NativeHandle       = pthread_t;
    using CallbackReturnType = void*;

    [[nodiscard]] static Result createThread(CreateParams& self, OpaqueThread& opaqueThread, pthread_t& threadHandle,
                                             void* (*threadFunc)(void* argument))
    {
        const int res = pthread_create(&opaqueThread.reinterpret_as<pthread_t>(), 0, threadFunc, &self);
        threadHandle  = opaqueThread.reinterpret_as<pthread_t>();
        if (res != 0)
        {
            return Result::Error("Thread::create - pthread_create failed");
        }
        return Result(true);
    }

    static void setThreadName(pthread_t& threadHandle, const StringView& nameNullTerminated)
    {
        SC_COMPILER_UNUSED(threadHandle);
#if !SC_PLATFORM_EMSCRIPTEN
        pthread_setname_np(nameNullTerminated.getNullTerminatedNative());
#endif
    }

    [[nodiscard]] static Result joinThread(OpaqueThread* threadNative)
    {
        int res = pthread_join(threadNative->reinterpret_as<pthread_t>(), 0);
        if (res != 0)
        {
            return Result::Error("phread_join error");
        }
        return Result(true);
    }

    [[nodiscard]] static Result detachThread(OpaqueThread* threadNative)
    {
        int res = pthread_detach(threadNative->reinterpret_as<pthread_t>());
        if (res != 0)
        {
            return Result::Error("pthread_detach error");
        }
        return Result(true);
    }
};

void SC::Thread::Sleep(uint32_t milliseconds) { ::usleep(milliseconds * 1000); }

SC::uint64_t SC::Thread::CurrentThreadID()
{
#if SC_PLATFORM_EMSCRIPTEN
    return 0;
#else
    uint64_t tid;
    pthread_threadid_np(NULL, &tid);
    return tid;
#endif
}
