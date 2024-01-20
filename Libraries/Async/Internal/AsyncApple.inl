// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Deferred.h"
#include <errno.h>     // errno
#include <netdb.h>     // socketlen_t/getsocketopt/send/recv
#include <sys/event.h> // kqueue
#include <sys/time.h>  // timespec
#include <sys/wait.h>  // WIFEXITED / WEXITSTATUS
#include <unistd.h>    // read/write/pread/pwrite

#include "../Async.h"

struct SC::AsyncEventLoop::Internal
{
    FileDescriptor loopFd;

    AsyncFileRead  wakeupPipeRead;
    PipeDescriptor wakeupPipe;
    char           wakeupPipeReadBuf[10];

    ~Internal() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] Result close()
    {
        return Result(wakeupPipe.readPipe.close() and wakeupPipe.writePipe.close() and loopFd.close());
    }

    [[nodiscard]] Result createEventLoop()
    {
        const int newQueue = kqueue();
        if (newQueue == -1)
        {
            // TODO: Better error handling
            return Result::Error("AsyncEventLoop::Internal::createEventLoop() failed");
        }
        SC_TRY(loopFd.assign(newQueue));
        return Result(true);
    }

    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop& loop)
    {
        SC_TRY(createWakeup(loop));
        SC_TRY(loop.runNoWait());   // Register the read handle before everything else
        loop.decreaseActiveCount(); // Avoids wakeup (read) keeping the queue up. Must be after runNoWait().
        // TODO: For consistency in the future decreaseActiveCount() should be usable immediately after
        // AsyncRequest::start() (similar to uv_unref).
        return Result(true);
    }

    [[nodiscard]] Result createWakeup(AsyncEventLoop& loop)
    {
        // Create
        SC_TRY(wakeupPipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteNonInheritable));
        SC_TRY(wakeupPipe.readPipe.setBlocking(false));
        SC_TRY(wakeupPipe.writePipe.setBlocking(false));

        // Register
        FileDescriptor::Handle wakeUpPipeDescriptor;
        SC_TRY(wakeupPipe.readPipe.get(
            wakeUpPipeDescriptor,
            Result::Error("AsyncEventLoop::Internal::createWakeup() - AsyncRequest read handle invalid")));
        SC_TRY(wakeupPipeRead.start(loop, wakeUpPipeDescriptor, {wakeupPipeReadBuf, sizeof(wakeupPipeReadBuf)}));
        return Result(true);
    }

    [[nodiscard]] static AsyncRequest* getAsyncRequest(const struct kevent& event)
    {
        return static_cast<AsyncRequest*>(event.udata);
    }

    [[nodiscard]] static Result stopSingleWatcherImmediate(AsyncRequest& async, SocketDescriptor::Handle handle,
                                                           short filter)
    {
        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRUST_RESULT(async.eventLoop->internal.get().loopFd.get(
            loopNativeDescriptor, Result::Error("AsyncEventLoop::Internal::pollAsync() - Invalid Handle")));
        struct kevent kev;
        EV_SET(&kev, handle, filter, EV_DELETE, 0, 0, nullptr);
        const int res = kevent(loopNativeDescriptor, &kev, 1, 0, 0, nullptr);
        if (res == 0 or (errno == EBADF or errno == ENOENT))
        {
            return Result(true);
        }
        return Result::Error("stopSingleWatcherImmediate failed");
    }
};

struct SC::AsyncEventLoop::KernelQueue
{
    static constexpr int totalNumEvents = 1024;

    struct kevent events[totalNumEvents];

    int newEvents = 0;

    KernelQueue() { memset(events, 0, sizeof(events)); }

    [[nodiscard]] Result pushNewSubmission(AsyncRequest& async)
    {
        switch (async.type)
        {
        case AsyncRequest::Type::LoopTimeout:
        case AsyncRequest::Type::LoopWakeUp:
            // These are not added to active queue
            break;
        case AsyncRequest::Type::SocketClose:
        case AsyncRequest::Type::FileClose: {
            async.eventLoop->scheduleManualCompletion(async);
            break;
        }
        default: {
            async.eventLoop->addActiveHandle(async);
            newEvents += 1;
            if (newEvents >= totalNumEvents)
            {
                SC_TRY(flushQueue(*async.eventLoop));
            }
            break;
        }
        }
        return Result(true);
    }

    [[nodiscard]] bool setEventWatcher(AsyncRequest& async, int fileDescriptor, short filter, short operation,
                                       unsigned int options = 0)
    {
        EV_SET(events + newEvents, fileDescriptor, filter, operation, options, 0, &async);
        // NOTE: newEvents will be incremented in ::pushNewSubmission()
        return true;
    }

    // POLL
    static struct timespec timerToTimespec(const Time::HighResolutionCounter& loopTime,
                                           const Time::HighResolutionCounter* nextTimer)
    {
        struct timespec specTimeout;
        if (nextTimer)
        {
            if (nextTimer->isLaterThanOrEqualTo(loopTime))
            {
                const Time::HighResolutionCounter diff = nextTimer->subtractExact(loopTime);

                specTimeout.tv_sec  = diff.part1;
                specTimeout.tv_nsec = diff.part2;
                return specTimeout;
            }
        }
        specTimeout.tv_sec  = 0;
        specTimeout.tv_nsec = 0;
        return specTimeout;
    }

    [[nodiscard]] Result pollAsync(AsyncEventLoop& eventLoop, PollMode pollMode)
    {
        const Time::HighResolutionCounter* nextTimer =
            pollMode == PollMode::ForcedForwardProgress ? eventLoop.findEarliestTimer() : nullptr;
        FileDescriptor::Handle loopHandle;
        SC_TRY(eventLoop.internal.get().loopFd.get(loopHandle, Result::Error("pollAsync() - Invalid Handle")));

        struct timespec specTimeout;
        // when nextTimer is null, specTimeout is initialized to 0, so that PollMode::NoWait
        specTimeout = timerToTimespec(eventLoop.loopTime, nextTimer);
        int res;
        do
        {
            auto spec = nextTimer or pollMode == PollMode::NoWait ? &specTimeout : nullptr;
            res       = kevent(loopHandle, events, newEvents, events, totalNumEvents, spec);
            if (res == -1 && errno == EINTR)
            {
                // Interrupted, we must recompute timeout
                if (nextTimer)
                {
                    eventLoop.updateTime();
                    specTimeout = timerToTimespec(eventLoop.loopTime, nextTimer);
                }
                continue;
            }
            break;
        } while (true);
        if (res == -1)
        {
            return Result::Error("AsyncEventLoop::Internal::poll() - failed");
        }
        newEvents = static_cast<int>(res);
        if (nextTimer)
        {
            eventLoop.executeTimers(*this, *nextTimer);
        }
        return Result(true);
    }

    [[nodiscard]] Result flushQueue(AsyncEventLoop& eventLoop)
    {
        FileDescriptor::Handle loopHandle;
        SC_TRY(eventLoop.internal.get().loopFd.get(loopHandle, Result::Error("flushQueue() - Invalid Handle")));

        int res;
        do
        {
            res = ::kevent(loopHandle, events, newEvents, nullptr, 0, nullptr);
        } while (res == -1 && errno == EINTR);
        if (res != 0)
        {
            return Result::Error("AsyncEventLoop::Internal::flushQueue() - kevent failed");
        }
        newEvents = 0;
        return Result(true);
    }

    [[nodiscard]] static Result validateEvent(const struct kevent& event, bool& continueProcessing)
    {
        continueProcessing = (event.flags & EV_DELETE) == 0;
        if ((event.flags & EV_ERROR) != 0)
        {
            return Result::Error("Error in processing event (kqueue EV_ERROR)");
        }
        return Result(true);
    }

    // TIMEOUT
    [[nodiscard]] static bool setupAsync(AsyncLoopTimeout& async)
    {
        async.eventLoop->activeTimers.queueBack(async);
        async.eventLoop->numberOfTimers += 1;
        return true;
    }
    [[nodiscard]] static bool activateAsync(AsyncLoopTimeout& async)
    {
        async.state = AsyncRequest::State::Active;
        return true;
    }
    [[nodiscard]] static bool completeAsync(AsyncLoopTimeout::Result& result)
    {
        SC_COMPILER_UNUSED(result);
        SC_ASSERT_RELEASE(false and "AsyncRequest::Type::LoopTimeout cannot be argument of completion");
        return false;
    }

    [[nodiscard]] static bool stopAsync(AsyncLoopTimeout& async)
    {
        async.eventLoop->numberOfTimers -= 1;
        async.state = AsyncRequest::State::Free;
        return true;
    }

    // WAKEUP
    [[nodiscard]] static bool setupAsync(AsyncLoopWakeUp& async)
    {
        async.eventLoop->activeWakeUps.queueBack(async);
        async.eventLoop->numberOfWakeups += 1;
        return true;
    }
    [[nodiscard]] static bool activateAsync(AsyncLoopWakeUp& async)
    {
        async.state = AsyncRequest::State::Active;
        return true;
    }
    [[nodiscard]] static bool completeAsync(AsyncLoopWakeUp::Result& result)
    {
        SC_COMPILER_UNUSED(result);
        SC_ASSERT_RELEASE(false and "AsyncRequest::Type::LoopWakeUp cannot be argument of completion");
        return false;
    }
    [[nodiscard]] static bool stopAsync(AsyncLoopWakeUp& async)
    {
        async.eventLoop->numberOfWakeups -= 1;
        async.state = AsyncRequest::State::Free;
        return true;
    }

    static void completeAsyncLoopWakeUpFromFakeRead(AsyncFileRead::Result& result)
    {
        AsyncFileRead& async = result.async;
        // TODO: Investigate usage of MACHPORT to avoid executing this additional read syscall
        auto readSpan = async.readBuffer;
        do
        {
            const ssize_t res = ::read(async.fileDescriptor, readSpan.data(), readSpan.sizeInBytes());

            if (res >= 0 and (static_cast<size_t>(res) == readSpan.sizeInBytes()))
                continue;

            if (res != -1)
                break;

            if (errno == EWOULDBLOCK or errno == EAGAIN)
                break;

        } while (errno == EINTR);
        result.async.eventLoop->executeWakeUps(result);
    }

    // Socket ACCEPT
    [[nodiscard]] bool setupAsync(AsyncSocketAccept& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_READ, EV_ADD | EV_ENABLE);
    }
    [[nodiscard]] static bool activateAsync(AsyncSocketAccept&) { return true; }

    [[nodiscard]] static Result completeAsync(AsyncSocketAccept::Result& result)
    {
        AsyncSocketAccept& async = result.async;
        SocketDescriptor   serverSocket;
        SC_TRY(serverSocket.assign(async.handle));
        auto detach = MakeDeferred([&] { serverSocket.detach(); });
        result.acceptedClient.detach();
        return SocketServer(serverSocket).accept(async.addressFamily, result.acceptedClient);
    }

    [[nodiscard]] static Result stopAsync(AsyncSocketAccept& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_READ);
    }

    // Socket CONNECT
    [[nodiscard]] bool setupAsync(AsyncSocketConnect& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_WRITE, EV_ADD | EV_ENABLE);
    }

    [[nodiscard]] static Result activateAsync(AsyncSocketConnect& async)
    {
        SocketDescriptor client;
        SC_TRY(client.assign(async.handle));
        auto detach = MakeDeferred([&] { client.detach(); });
        auto res    = SocketClient(client).connect(async.ipAddress);
        // we expect connect to fail with
        if (res)
        {
            return Result::Error("connect failed (succeeded?)");
        }
        if (errno != EAGAIN and errno != EINPROGRESS)
        {
            return Result::Error("connect failed (socket is in blocking mode)");
        }
        return Result(true);
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketConnect::Result& result)
    {
        AsyncSocketConnect& async = result.async;

        int       errorCode;
        socklen_t errorSize = sizeof(errorCode);
        const int socketRes = ::getsockopt(async.handle, SOL_SOCKET, SO_ERROR, &errorCode, &errorSize);

        // TODO: This is making a syscall for each connected socket, we should probably aggregate them
        // And additionally it's stupid as probably WRITE will be subscribed again anyway
        // But probably this means to review the entire process of async stop
        SC_TRUST_RESULT(Internal::stopSingleWatcherImmediate(result.async, async.handle, EVFILT_WRITE));
        if (socketRes == 0)
        {
            SC_TRY_MSG(errorCode == 0, "connect SO_ERROR");
            return Result(true);
        }
        return Result::Error("connect getsockopt failed");
    }

    [[nodiscard]] static Result stopAsync(AsyncSocketConnect& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_WRITE);
    }

    // Socket SEND
    [[nodiscard]] Result setupAsync(AsyncSocketSend& async)
    {
        return Result(setEventWatcher(async, async.handle, EVFILT_WRITE, EV_ADD | EV_ENABLE));
    }

    [[nodiscard]] static bool activateAsync(AsyncSocketSend&) { return true; }

    [[nodiscard]] static Result completeAsync(AsyncSocketSend::Result& result)
    {
        AsyncSocketSend& async = result.async;
        const ssize_t    res   = ::send(async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        SC_TRY_MSG(res >= 0, "error in send");
        SC_TRY_MSG(size_t(res) == async.data.sizeInBytes(), "send didn't send all data");
        return Result(true);
    }

    [[nodiscard]] Result stopAsync(AsyncSocketSend& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_WRITE);
    }

    // Socket RECEIVE
    [[nodiscard]] Result setupAsync(AsyncSocketReceive& async)
    {
        return Result(setEventWatcher(async, async.handle, EVFILT_READ, EV_ADD | EV_ENABLE));
    }

    [[nodiscard]] static bool activateAsync(AsyncSocketReceive&) { return true; }

    [[nodiscard]] static Result completeAsync(AsyncSocketReceive::Result& result)
    {
        AsyncSocketReceive& async = result.async;
        const ssize_t       res   = ::recv(async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        SC_TRY_MSG(res >= 0, "error in recv");
        return Result(async.data.sliceStartLength(0, static_cast<size_t>(res), result.readData));
    }

    [[nodiscard]] static Result stopAsync(AsyncSocketReceive& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_READ);
    }

    // Socket CLOSE
    [[nodiscard]] static Result setupAsync(AsyncSocketClose& async)
    {
        async.code = ::close(async.handle);
        SC_TRY_MSG(async.code == 0, "Close returned error");
        return Result(true);
    }
    [[nodiscard]] static bool activateAsync(AsyncSocketClose&) { return true; }
    [[nodiscard]] static bool completeAsync(AsyncSocketClose::Result&) { return true; }
    [[nodiscard]] static bool stopAsync(AsyncSocketClose&) { return true; }

    // File READ
    [[nodiscard]] bool setupAsync(AsyncFileRead& async)
    {
        return setEventWatcher(async, async.fileDescriptor, EVFILT_READ, EV_ADD);
    }

    [[nodiscard]] static bool activateAsync(AsyncFileRead&) { return true; }

    [[nodiscard]] static Result completeAsync(AsyncFileRead::Result& result)
    {
        if (&result.async == &result.async.eventLoop->internal.get().wakeupPipeRead)
        {
            completeAsyncLoopWakeUpFromFakeRead(result);
        }
        else
        {
            auto    span = result.async.readBuffer;
            ssize_t res;
            do
            {
                res = ::pread(result.async.fileDescriptor, span.data(), span.sizeInBytes(),
                              static_cast<off_t>(result.async.offset));
            } while ((res == -1) and (errno == EINTR));
            SC_TRY_MSG(res >= 0, "::read failed");
            SC_TRY(result.async.readBuffer.sliceStartLength(0, static_cast<size_t>(res), result.readData));
        }
        return Result(true);
    }

    [[nodiscard]] static Result stopAsync(AsyncFileRead& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.fileDescriptor, EVFILT_READ);
    }

    // File WRITE
    [[nodiscard]] bool setupAsync(AsyncFileWrite& async)
    {
        return setEventWatcher(async, async.fileDescriptor, EVFILT_WRITE, EV_ADD);
    }

    [[nodiscard]] static bool activateAsync(AsyncFileWrite&) { return true; }

    [[nodiscard]] static Result completeAsync(AsyncFileWrite::Result& result)
    {
        AsyncFileWrite& async = result.async;

        auto    span = async.writeBuffer;
        ssize_t res;
        do
        {
            res = ::pwrite(async.fileDescriptor, span.data(), span.sizeInBytes(), static_cast<off_t>(async.offset));
        } while ((res == -1) and (errno == EINTR));
        SC_TRY_MSG(res >= 0, "::write failed");
        result.writtenBytes = static_cast<size_t>(res);
        return Result(true);
    }

    [[nodiscard]] static Result stopAsync(AsyncFileWrite& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.fileDescriptor, EVFILT_WRITE);
    }

    // File Close
    [[nodiscard]] Result setupAsync(AsyncFileClose& async)
    {
        async.code = ::close(async.fileDescriptor);
        SC_TRY_MSG(async.code == 0, "Close returned error");
        return Result(true);
    }

    [[nodiscard]] static bool activateAsync(AsyncFileClose&) { return true; }
    [[nodiscard]] static bool completeAsync(AsyncFileClose::Result&) { return true; }
    [[nodiscard]] static bool stopAsync(AsyncFileClose&) { return true; }

    // PROCESS
    [[nodiscard]] bool setupAsync(AsyncProcessExit& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_PROC, EV_ADD | EV_ENABLE, NOTE_EXIT | NOTE_EXITSTATUS);
    }

    [[nodiscard]] static bool activateAsync(AsyncProcessExit&) { return true; }

    [[nodiscard]] Result completeAsync(AsyncProcessExit::Result& result)
    {
        SC_TRY_MSG(result.async.eventIndex >= 0, "Invalid event Index");
        const struct kevent event = events[result.async.eventIndex];
        if ((event.fflags & (NOTE_EXIT | NOTE_EXITSTATUS)) > 0)
        {
            const uint32_t data = static_cast<uint32_t>(event.data);
            if (WIFEXITED(data) != 0)
            {
                result.exitStatus.status = WEXITSTATUS(data);
            }
            return Result(true);
        }
        return Result(false);
    }

    [[nodiscard]] static Result stopAsync(AsyncProcessExit& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_PROC);
    }
};

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread()
{
    Internal& self = internal.get();
    // TODO: We need an atomic bool swap to wait until next run
    const void* fakeBuffer;
    int         asyncFd;
    ssize_t     writtenBytes;
    SC_TRY(self.wakeupPipe.writePipe.get(asyncFd, Result::Error("writePipe handle")));
    fakeBuffer = "";
    do
    {
        writtenBytes = ::write(asyncFd, fakeBuffer, 1);
    } while (writtenBytes == -1 && errno == EINTR);

    if (writtenBytes != 1)
    {
        return Result::Error("AsyncEventLoop::wakeUpFromExternalThread - Error in write");
    }
    return Result(true);
}

SC::Result SC::AsyncEventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }

SC::Result SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
