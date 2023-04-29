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
        SC_TRY_IF(loop.startRead(wakeupPipeRead, wakeUpPipeDescriptor, {wakeupPipeReadBuf, sizeof(wakeupPipeReadBuf)},
                                 Function<void(AsyncResult&)>()));
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
        asyncResult.eventLoop.executeWakeUps();
    }

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult& result, const struct kevent& event)
    {
        if ((event.flags & EV_DELETE) != 0)
        {
            result.eventLoop.activeHandles.remove(result.async);
            return false; // Do not call callback
        }
        if ((event.flags & EV_ERROR) != 0)
        {
            // TODO: Handle EV_ERROR case
            SC_RELEASE_ASSERT(false);
            return false;
        }
        // regular path
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

            return result.result.fields.accept.acceptedClient.assign(acceptedClient);
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
        switch (async.operation.type)
        {
        case Async::Type::Timeout: eventLoop.activeTimers.queueBack(async); return true;
        case Async::Type::WakeUp: eventLoop.activeWakeUps.queueBack(async); return true;
        case Async::Type::Read: startReadWatcher(async, *async.operation.unionAs<Async::Read>()); break;
        case Async::Type::ProcessExit:
            startProcessExitWatcher(async, *async.operation.unionAs<Async::ProcessExit>());
            break;
        case Async::Type::Accept: startAcceptWatcher(async, *async.operation.unionAs<Async::Accept>()); break;
        }
        newEvents += 1;
        // The handles are not active until we "poll" or "flush"
        eventLoop.stagedHandles.queueBack(async);
        if (isFull())
        {
            SC_TRY_IF(flushQueue(eventLoop));
        }
        return true;
    }

    [[nodiscard]] bool isFull() const { return newEvents >= totalNumEvents; }

    ReturnCode stopKeventWatcher(Async& async, SocketDescriptorNative handle, short filter)
    {
        FileDescriptorNative loopNativeDescriptor;
        SC_TRUST_RESULT(async.eventLoop->internal.get().loopFd.handle.get(
            loopNativeDescriptor, "EventLoop::Internal::pollAsync() - Invalid Handle"_a8));
        struct kevent kev;
        EV_SET(&kev, handle, filter, EV_DELETE, 0, 0, nullptr);
        const int res = kevent(loopNativeDescriptor, &kev, 1, 0, 0, nullptr);
        if (res == 0 or (errno == EBADF or errno == ENOENT))
        {
            return true;
        }
        return "kevent EV_DELETE failed"_a8;
    }

    void startReadWatcher(Async& async, Async::Read& asyncRead)
    {
        EV_SET(events + newEvents, asyncRead.fileDescriptor, EVFILT_READ, EV_ADD, 0, 0, &async);
    }

    ReturnCode stopReadWatcher(Async& async, Async::Read& asyncRead)
    {
        return stopKeventWatcher(async, asyncRead.fileDescriptor, EVFILT_READ);
    }

    void startAcceptWatcher(Async& async, Async::Accept& asyncAccept)
    {
        EV_SET(events + newEvents, asyncAccept.handle, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &async);
    }

    ReturnCode stopAcceptWatcher(Async& async, Async::Accept& asyncAccept)
    {
        return stopKeventWatcher(async, asyncAccept.handle, EVFILT_READ);
    }

    void startProcessExitWatcher(Async& async, Async::ProcessExit& asyncProcessExit)
    {
        EV_SET(events + newEvents, asyncProcessExit.handle, EVFILT_PROC, EV_ADD | EV_ENABLE,
               NOTE_EXIT | NOTE_EXITSTATUS, 0, &async);
    }

    ReturnCode stopProcessExitWatcher(Async& async, Async::ProcessExit& asyncProcessExit)
    {
        return stopKeventWatcher(async, asyncProcessExit.handle, EVFILT_PROC);
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

    [[nodiscard]] ReturnCode pollAsync(EventLoop& self, PollMode pollMode)
    {
        const TimeCounter* nextTimer = pollMode == PollMode::ForcedForwardProgress ? self.findEarliestTimer() : nullptr;
        FileDescriptorNative loopNativeDescriptor;
        SC_TRY_IF(self.internal.get().loopFd.handle.get(loopNativeDescriptor,
                                                        "EventLoop::Internal::pollAsync() - Invalid Handle"_a8));

        struct timespec specTimeout;
        // when nextTimer is null, specTimeout is initialized to 0, so that PollMode::NoWait
        specTimeout = timerToTimespec(self.loopTime, nextTimer);
        int res;
        // in the next kevent call the staged handles will become active
        self.activeHandles.appendBack(self.stagedHandles);
        do
        {
            res = kevent(loopNativeDescriptor, events, newEvents, events, totalNumEvents,
                         nextTimer or pollMode == PollMode::NoWait ? &specTimeout : nullptr);
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
        if (nextTimer)
        {
            self.executeTimers(*this, *nextTimer);
        }
        return true;
    }

    [[nodiscard]] ReturnCode flushQueue(EventLoop& self)
    {
        FileDescriptorNative loopNativeDescriptor;
        SC_TRY_IF(self.internal.get().loopFd.handle.get(loopNativeDescriptor,
                                                        "EventLoop::Internal::flushQueue() - Invalid Handle"_a8));

        int res;
        // in the next kevent call the staged handles will become active
        self.activeHandles.appendBack(self.stagedHandles);
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

    [[nodiscard]] static ReturnCode activateAsync(EventLoop& eventLoop, Async& async)
    {
        SC_UNUSED(eventLoop);
        SC_UNUSED(async);
        return true;
    }

    [[nodiscard]] ReturnCode cancelAsync(EventLoop& eventLoop, Async& async)
    {
        SC_UNUSED(eventLoop);
        switch (async.operation.type)
        {
        case Async::Type::Timeout: return true;
        case Async::Type::WakeUp: return true;
        case Async::Type::Read: return stopReadWatcher(async, *async.operation.unionAs<Async::Read>());
        case Async::Type::ProcessExit:
            return stopProcessExitWatcher(async, *async.operation.unionAs<Async::ProcessExit>());
        case Async::Type::Accept: return stopAcceptWatcher(async, *async.operation.unionAs<Async::Accept>());
        }
        return false;
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
