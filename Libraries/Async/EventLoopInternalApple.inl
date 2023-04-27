// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include <errno.h>
#include <sys/event.h> // kqueue
#include <sys/time.h>  // timespec
#include <sys/wait.h>  // WIFEXITED / WEXITSTATUS
#include <unistd.h>

#include "../Foundation/Array.h"
#include "EventLoop.h"

#include <netdb.h>
#include <sys/select.h> // fd_set for emscripten
#include <unistd.h>

struct SC::Async::ProcessExitInternal
{
};

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    AsyncRead          wakeupPipeRead;
    FileDescriptorPipe wakeupPipe;
    uint8_t            wakeupPipeReadBuf[10];

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
        FileDescriptorNative wakeUpPipeDescriptor;
        SC_TRY_IF(wakeupPipe.readPipe.handle.get(wakeUpPipeDescriptor,
                                                 "EventLoop::Internal::createWakeup() - Async read handle invalid"_a8));
        // Optimization: we avoid one indirect function call, see runCompletionFor checking if async == wakeupPipeRead
        // wakeupPipeRead.callback.bind<Internal, &Internal::runCompletionForWakeUp>(this);
        SC_TRY_IF(loop.addRead(wakeupPipeRead, wakeUpPipeDescriptor, {wakeupPipeReadBuf, sizeof(wakeupPipeReadBuf)}));
        return true;
    }

    [[nodiscard]] Async* getAsync(const struct kevent& event) const { return static_cast<Async*>(event.udata); }

    [[nodiscard]] void* getUserData(const struct kevent& event) const
    {
        SC_UNUSED(event);
        return nullptr;
    }

    void runCompletionForWakeUp(AsyncResult& asyncResult)
    {
        Async& async = asyncResult.async;
        // TODO: Investigate usage of MACHPORT to avoid executing this additional read syscall
        Async::Read&  readOp   = *async.operation.unionAs<Async::Read>();
        Span<uint8_t> readSpan = readOp.readBuffer;
        do
        {
            const ssize_t res = read(readOp.fileDescriptor, readSpan.data(), readSpan.sizeInBytes());

            if (res >= 0 and (static_cast<size_t>(res) == readSpan.sizeInBytes()))
                continue;

            if (res != -1)
                break;

            if (errno == EWOULDBLOCK or errno == EAGAIN)
                break;

        } while (errno == EINTR);
        asyncResult.eventLoop.runCompletionForNotifiers();
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult& result, const struct kevent& event)
    {
        switch (result.async.operation.type)
        {
        case Async::Type::Timeout: {
            SC_RELEASE_ASSERT(false and "Async::Type::Timeout cannot be argument of completion");
            break;
        }
        case Async::Type::Read: {
            if (&result.async == &wakeupPipeRead)
            {
                runCompletionForWakeUp(result);
            }
            else
            {
                // TODO: do the actual read operation here
            }
            break;
        }
        case Async::Type::WakeUp: {
            SC_RELEASE_ASSERT(false and "Async::Type::WakeUp cannot be argument of completion");
            break;
        }
        case Async::Type::ProcessExit: {
            // Process has exited
            if ((event.fflags & (NOTE_EXIT | NOTE_EXITSTATUS)) > 0)
            {
                AsyncResult::ProcessExit& res = result.result.fields.processExit;

                const uint32_t data = static_cast<uint32_t>(event.data);
                if (WIFEXITED(data) != 0)
                {
                    res.exitStatus.status.assign(WEXITSTATUS(data));
                }
            }
            break;
        }
        case Async::Type::Accept: {
            SocketDescriptorNative listenDescriptor = result.async.operation.fields.accept.handle;

            struct sockaddr_in sAddr;
            socklen_t          sAddrSize = sizeof(sAddr);

            SocketDescriptorNative acceptedClient;
            acceptedClient = ::accept(listenDescriptor, reinterpret_cast<struct sockaddr*>(&sAddr), &sAddrSize);
            SC_TRY_MSG(acceptedClient != SocketDescriptorNativeInvalid, "accept failed"_a8);

            result.result.fields.accept.acceptedClient = acceptedClient;
            break;
        }
        }
        return true;
    }
};

struct SC::EventLoop::KernelQueue
{
    static constexpr int totalNumEvents = 1024;

    struct kevent events[totalNumEvents];

    int newEvents = 0;

    KernelQueue() { memset(events, 0, sizeof(events)); }

    [[nodiscard]] ReturnCode stageAsync(EventLoop& eventLoop, Async& async)
    {
        FileDescriptor& loopFd = eventLoop.internal.get().loopFd;
        switch (async.operation.type)
        {
        case Async::Type::Timeout: eventLoop.activeTimers.queueBack(async); return true;
        case Async::Type::WakeUp: eventLoop.activeWakeUps.queueBack(async); return true;
        case Async::Type::Read: startReadWatcher(loopFd, async); break;
        case Async::Type::ProcessExit: startProcessExitWatcher(loopFd, async); break;
        case Async::Type::Accept: startAcceptWatcher(loopFd, async); break;
        }
        eventLoop.stagedHandles.queueBack(async);
        return true;
    }

    [[nodiscard]] bool isFull() const { return newEvents >= totalNumEvents; }

    void startReadWatcher(FileDescriptor& loopFd, Async& async)
    {
        FileDescriptorNative fileDescriptor = async.operation.fields.read.fileDescriptor;
        SC_UNUSED(loopFd);
        constexpr int fflags = 0;
        EV_SET(events + newEvents, fileDescriptor, EVFILT_READ, EV_ADD, fflags, 0, &async);
        newEvents += 1;
    }

    void startAcceptWatcher(FileDescriptor& loopFd, Async& async)
    {
        SocketDescriptorNative socketDescriptor = async.operation.fields.accept.handle;
        SC_UNUSED(loopFd);
        constexpr int fflags = 0;
        EV_SET(events + newEvents, socketDescriptor, EVFILT_READ, EV_ADD | EV_ENABLE, fflags, 0, &async);
        newEvents += 1;
    }

    void startProcessExitWatcher(FileDescriptor& loopFd, Async& async)
    {
        ProcessNative processHandle = async.operation.fields.processExit.handle;
        SC_UNUSED(loopFd);
        constexpr uint32_t fflags = NOTE_EXIT | NOTE_EXITSTATUS;
        EV_SET(events + newEvents, processHandle, EVFILT_PROC, EV_ADD | EV_ENABLE, fflags, 0, &async);
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
                specTimeout.tv_sec     = diff.part1;
                specTimeout.tv_nsec    = diff.part2;
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
        int res;
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

    [[nodiscard]] ReturnCode flushQueue(EventLoop& self)
    {
        FileDescriptorNative loopNativeDescriptor;
        SC_TRY_IF(self.internal.get().loopFd.handle.get(loopNativeDescriptor,
                                                        "EventLoop::Internal::flushQueue() - Invalid Handle"_a8));

        int res;
        do
        {
            res = kevent(loopNativeDescriptor, events, newEvents, nullptr, 0, nullptr);
        } while (res == -1 && errno == EINTR);
        if (res != 0)
        {
            return "EventLoop::Internal::flushQueue() - kevent failed"_a8;
        }
        newEvents = 0;
        return true;
    }
    
    [[nodiscard]] static ReturnCode rearmAsync(EventLoop& eventLoop, Async& async)
    {
        SC_UNUSED(eventLoop);
        SC_UNUSED(async);
        return true;
    }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread()
{
    Internal& self = internal.get();
    // TODO: We need an atomic bool swap to wait until next run
    const void* fakeBuffer;
    int         asyncFd;
    ssize_t     writtenBytes;
    SC_TRY_IF(self.wakeupPipe.writePipe.handle.get(asyncFd, "writePipe handle"_a8));
    fakeBuffer = "";
    do
    {
        writtenBytes = ::write(asyncFd, fakeBuffer, 1);
    } while (writtenBytes == -1 && errno == EINTR);

    if (writtenBytes != 1)
    {
        return "EventLoop::wakeUpFromExternalThread - Error in write"_a8;
    }
    return true;
}
