// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Threading.h"

#include "../Foundation/StringNative.h"

#if SC_PLATFORM_WINDOWS
#include "ThreadingInternalWindows.inl"
#else
#include "ThreadingInternalPosix.inl"
#endif

struct SC::Thread::CreateParams
{
    CreateParams(Action&& callback, Thread* thread) : callback(forward<Action>(callback)), thread(thread) {}

    EventObject event;
    Action&&    callback;
    Thread*     thread;
    StringView  nameNullTerminated;

    Internal::NativeHandle threadHandle;

    static Internal::CallbackReturnType threadFunc(void* argument)
    {
        CreateParams& self = *static_cast<CreateParams*>(argument);

        if (not self.nameNullTerminated.isEmpty())
        {
            Internal::setThreadName(self.threadHandle, self.nameNullTerminated);
        }
        // Note: If cb has captured heap allocated objects, they will get freed on the thread
        Action cb = move(self.callback);
        self.event.signal();
        cb();
        return 0;
    }
};

SC::Thread::~Thread() { SC_DEBUG_ASSERT(not thread.hasValue() && "Forgot to call join() or detach()"); }

SC::ReturnCode SC::Thread::start(StringView name, Action&& func)
{
    if (thread.hasValue())
        return "Error thread already started"_a8;
    CreateParams      self(forward<Action>(func), this);
    StringNative<128> nameNative;
    SC_TRY_IF(nameNative.convertNullTerminateFastPath(name, self.nameNullTerminated));
    OpaqueThread opaqueThread;
    SC_TRY_IF(Internal::createThread(self, opaqueThread, self.threadHandle, &CreateParams::threadFunc));
    thread.assign(move(opaqueThread));
    self.event.wait();
    return true;
}

SC::ReturnCode SC::Thread::join()
{
    OpaqueThread* threadNative;
    SC_TRY_IF(thread.get(threadNative));
    SC_TRY_IF(Internal::joinThread(threadNative));
    thread.clear();
    return true;
}

SC::ReturnCode SC::Thread::detach()
{
    OpaqueThread* threadNative;
    SC_TRY_IF(thread.get(threadNative));
    SC_TRY_IF(Internal::detachThread(threadNative));
    thread.clear();
    return true;
}

void SC::EventObject::wait()
{
    mutex.lock();
    while (not isSignaled)
    {
        cond.wait(mutex);
    }
    isSignaled = false;
    mutex.unlock();
}

void SC::EventObject::signal()
{
    mutex.lock();
    isSignaled = true;
    mutex.unlock();
    cond.signal();
}
