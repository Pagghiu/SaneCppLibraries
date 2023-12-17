// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Threading.h"
#include "../Foundation/Assert.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/ThreadingWindows.inl"
#else
#include "Internal/ThreadingPosix.inl"
#endif

SC::Thread::~Thread() { SC_ASSERT_DEBUG(not thread.hasValue() && "Forgot to call join() or detach()"); }

SC::Result SC::Thread::start(Function<void(Thread&)>&& func)
{
    SC_TRY(func.isValid());
    if (thread.hasValue())
        return Result::Error("Error thread already started");

    OpaqueThread opaqueThread;
    userFunction = move(func);
    SC_TRY(Internal::createThread(*this, opaqueThread, &Internal::threadFunc));
    thread.assign(move(opaqueThread));
    return Result(true);
}

void SC::Thread::setThreadName(const native_char_t* name) { Internal::setThreadName(name); }

SC::Result SC::Thread::join()
{
    OpaqueThread* threadNative;
    SC_TRY(thread.get(threadNative));
    SC_TRY(Internal::joinThread(*threadNative));
    thread.clear();
    return Result(true);
}

SC::Result SC::Thread::detach()
{
    OpaqueThread* threadNative;
    SC_TRY(thread.get(threadNative));
    SC_TRY(Internal::detachThread(*threadNative));
    thread.clear();
    return Result(true);
}

bool SC::Thread::wasStarted() const { return thread.hasValue(); }

void SC::EventObject::wait()
{
    mutex.lock();
    while (not isSignaled)
    {
        cond.wait(mutex);
    }
    if (autoReset)
    {
        isSignaled = false;
    }
    mutex.unlock();
}

void SC::EventObject::signal()
{
    mutex.lock();
    isSignaled = true;
    mutex.unlock();
    cond.signal();
}
