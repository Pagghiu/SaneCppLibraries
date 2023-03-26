// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include <errno.h>
#include <sys/event.h> // kqueue
#include <sys/time.h>  // timespec
#include <unistd.h>

#include "EventLoop.h"

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    Async              wakeupAsync;
    FileDescriptorPipe wakeupPipe;

    ~Internal() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] ReturnCode close()
    {
        return wakeupPipe.readPipe.handle.close() and wakeupPipe.writePipe.handle.close() and loopFd.handle.close();
    }

    [[nodiscard]] ReturnCode createEventLoop()
    {
        const int newQueue = kqueue();
        if (newQueue == -1)
        {
            // TODO: Better kqueue error handling
            return "EventLoop::Internal::createEventLoop() - kqueue failed"_a8;
        }
        SC_TRY_IF(loopFd.handle.assign(newQueue));
        return true;
    }

    [[nodiscard]] ReturnCode createWakeup(EventLoop& loop)
    {
        // Create
        SC_TRY_IF(
            wakeupPipe.createPipe(FileDescriptorPipe::ReadNonInheritable, FileDescriptorPipe::WriteNonInheritable));
        SC_TRY_IF(wakeupPipe.readPipe.setBlocking(false));
        SC_TRY_IF(wakeupPipe.writePipe.setBlocking(false));

        // Register
        FileDescriptorNative pipeHandle;
        SC_TRY_IF(wakeupPipe.readPipe.handle.get(pipeHandle,
                                                 "EventLoop::Internal::createWakeup() - Async read handle invalid"_a8));
        loop.addRead(pipeHandle, wakeupAsync);
        return true;
    }
};

struct SC::EventLoop::KernelQueue
{
    static constexpr int totalNumEvents         = 1024;
    struct kevent        events[totalNumEvents] = {0};
    int                  newEvents              = 0;

    [[nodiscard]] ReturnCode pushAsync(EventLoop& eventLoop, Async* async)
    {
        switch (async->operation.type)
        {
        case Async::Operation::Type::Timeout: {
            eventLoop.activeTimers.queueBack(*async);
        }
        break;
        case Async::Operation::Type::Read: {
            addReadWatcher(eventLoop.internal.get().loopFd, async->operation.fields.read.fileDescriptor);
            eventLoop.stagedHandles.queueBack(*async);
        }
        break;
        }
        return true;
    }

    [[nodiscard]] bool isFull() const { return newEvents >= totalNumEvents; }

    void addReadWatcher(FileDescriptor& loopFd, FileDescriptorNative fileDescriptor)
    {
        int fflags = 0;
        EV_SET(events + newEvents, fileDescriptor, EVFILT_READ, EV_ADD, fflags, 0, 0);
        newEvents += 1;
    }

    static struct timespec timerToTimespec(const TimeCounter& loopTime, const TimeCounter* nextTimer)
    {
        struct timespec specTimeout;
        if (nextTimer)
        {
            if (nextTimer->isLaterThanOrEqualTo(loopTime))
            {
                const TimeCounter diff = nextTimer->subtractExact(loopTime);
                struct timespec   specTimeout;
                specTimeout.tv_sec  = diff.part1;
                specTimeout.tv_nsec = diff.part2;
                return specTimeout;
            }
        }
        specTimeout.tv_sec  = 0;
        specTimeout.tv_nsec = 0;
        return specTimeout;
    }

    [[nodiscard]] ReturnCode poll(EventLoop& self, const TimeCounter* nextTimer)
    {
        FileDescriptorNative loopNativeDescriptor;
        SC_TRY_IF(self.internal.get().loopFd.handle.get(loopNativeDescriptor,
                                                        "EventLoop::Internal::poll() - Invalid Handle"_a8));

        struct timespec specTimeout;
        specTimeout = timerToTimespec(self.loopTime, nextTimer);
        size_t res;
        do
        {
            res = kevent(loopNativeDescriptor, events, newEvents, events, totalNumEvents,
                         nextTimer ? &specTimeout : nullptr);
            if (res == -1 && errno == EINTR)
            {
                // Interrupted, we must recompute timeout
                if (nextTimer)
                {
                    self.updateTime();
                    specTimeout = timerToTimespec(self.loopTime, nextTimer);
                }
                continue;
            }
            break;
        } while (true);
        if (res == -1)
        {
            return "EventLoop::Internal::poll() - kevent failed"_a8;
        }
        newEvents = static_cast<int>(res);
        return true;
    }

    [[nodiscard]] ReturnCode commitQueue(EventLoop& self)
    {
        FileDescriptorNative loopNativeDescriptor;
        SC_TRY_IF(self.internal.get().loopFd.handle.get(loopNativeDescriptor,
                                                        "EventLoop::Internal::commitQueue() - Invalid Handle"_a8));

        int res;
        do
        {
            res = kevent(loopNativeDescriptor, events, newEvents, nullptr, 0, nullptr);
        } while (res == -1 && errno == EINTR);
        if (res != 0)
        {
            return "EventLoop::Internal::commitQueue() - kevent failed"_a8;
        }
        newEvents = 0;
        return true;
    }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread()
{
    Internal& self = internal.get();
    // TODO: We need an atomic bool swap to wait until next run
    const void* fakeBuffer;
    ssize_t     numBytes;
    int         asyncFd;
    ssize_t     writtenBytes;
    SC_TRY_IF(self.wakeupPipe.writePipe.handle.get(asyncFd, "writePipe handle"_a8));
    fakeBuffer = "";
    numBytes   = 1;
    do
    {
        writtenBytes = ::write(asyncFd, fakeBuffer, numBytes);
    } while (writtenBytes == -1 && errno == EINTR);

    if (writtenBytes != numBytes)
    {
        return "EventLoop::wakeUpFromExternalThread - Error in write"_a8;
    }
    return true;
}
