// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Threading.h"
#include "../Foundation/Assert.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/ThreadingWindows.inl"
#else
#include "Internal/ThreadingPosix.inl"
#endif

//-------------------------------------------------------------------------------------------------------
// Thread
//-------------------------------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------------------------------
// RWLock
//-------------------------------------------------------------------------------------------------------
void SC::RWLock::lockRead()
{
    mutex.lock();
    while (writerActive || waitingWriters > 0)
    {
        readersQ.wait(mutex);
    }
    activeReaders++;
    mutex.unlock();
}

void SC::RWLock::unlockRead()
{
    mutex.lock();
    activeReaders--;
    if (activeReaders == 0 && waitingWriters > 0)
    {
        writersQ.signal();
    }
    mutex.unlock();
}

void SC::RWLock::lockWrite()
{
    mutex.lock();
    waitingWriters++;
    while (writerActive || activeReaders > 0)
    {
        writersQ.wait(mutex);
    }
    waitingWriters--;
    writerActive = true;
    mutex.unlock();
}

void SC::RWLock::unlockWrite()
{
    mutex.lock();
    writerActive = false;
    if (waitingWriters > 0)
    {
        writersQ.signal();
    }
    else
    {
        readersQ.broadcast();
    }
    mutex.unlock();
}

//-------------------------------------------------------------------------------------------------------
// Barrier
//-------------------------------------------------------------------------------------------------------
void SC::Barrier::wait()
{
    mutex.lock();
    const uint32_t gen = generation;
    waitCount++;

    if (waitCount == threadCount)
    {
        waitCount = 0;
        generation++;
        condition.broadcast();
    }
    else
    {
        while (gen == generation)
        {
            condition.wait(mutex);
        }
    }
    mutex.unlock();
}

//-------------------------------------------------------------------------------------------------------
// EventObject
//-------------------------------------------------------------------------------------------------------
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
    cond.signal();
    mutex.unlock();
}
