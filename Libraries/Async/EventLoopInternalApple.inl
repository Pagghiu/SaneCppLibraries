// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include <errno.h>     // errno
#include <netdb.h>     // socketlen_t/getsocketopt/send/recv
#include <sys/event.h> // kqueue
#include <sys/time.h>  // timespec
#include <sys/wait.h>  // WIFEXITED / WEXITSTATUS
#include <unistd.h>    // read/write/pread/pwrite

#include "../Foundation/Array.h"
#include "EventLoop.h"

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    AsyncFileRead  wakeupPipeRead;
    PipeDescriptor wakeupPipe;
    char           wakeupPipeReadBuf[10];

    ~Internal() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] ReturnCode close()
    {
        return wakeupPipe.readPipe.close() and wakeupPipe.writePipe.close() and loopFd.close();
    }

    [[nodiscard]] ReturnCode createEventLoop()
    {
        const int newQueue = kqueue();
        if (newQueue == -1)
        {
            // TODO: Better kqueue error handling
            return "EventLoop::Internal::createEventLoop() - kqueue failed"_a8;
        }
        SC_TRY_IF(loopFd.assign(newQueue));
        return true;
    }

    [[nodiscard]] ReturnCode createWakeup(EventLoop& loop)
    {
        // Create
        SC_TRY_IF(wakeupPipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteNonInheritable));
        SC_TRY_IF(wakeupPipe.readPipe.setBlocking(false));
        SC_TRY_IF(wakeupPipe.writePipe.setBlocking(false));

        // Register
        FileDescriptor::Handle wakeUpPipeDescriptor;
        SC_TRY_IF(wakeupPipe.readPipe.get(wakeUpPipeDescriptor,
                                          "EventLoop::Internal::createWakeup() - Async read handle invalid"_a8));
        SC_TRY_IF(loop.startFileRead(wakeupPipeRead, wakeUpPipeDescriptor,
                                     {wakeupPipeReadBuf, sizeof(wakeupPipeReadBuf)}, {}));
        SC_TRY_IF(loop.runNoWait()); // We want to register the read handle before everything else
        loop.decreaseActiveCount();  // we don't want the read to keep the queue up
        return true;
    }

    [[nodiscard]] static Async* getAsync(const struct kevent& event) { return static_cast<Async*>(event.udata); }

    [[nodiscard]] static ReturnCode stopSingleWatcherImmediate(Async& async, SocketDescriptor::Handle handle,
                                                               short filter)
    {
        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRUST_RESULT(async.eventLoop->internal.get().loopFd.get(
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
};

struct SC::EventLoop::KernelQueue
{
    static constexpr int totalNumEvents = 1024;

    struct kevent events[totalNumEvents];

    int newEvents = 0;

    KernelQueue() { memset(events, 0, sizeof(events)); }

    [[nodiscard]] ReturnCode pushNewSubmission(Async& async)
    {
        async.eventLoop->addActiveHandle(async);
        newEvents += 1;
        if (newEvents >= totalNumEvents)
        {
            SC_TRY_IF(flushQueue(*async.eventLoop));
        }
        return true;
    }

    [[nodiscard]] bool setEventWatcher(Async& async, int fileDescriptor, short filter, short operation,
                                       unsigned int options = 0)
    {
        EV_SET(events + newEvents, fileDescriptor, filter, operation, options, 0, &async);
        return true;
    }

    // POLL
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
        FileDescriptor::Handle loopHandle;
        SC_TRY_IF(self.internal.get().loopFd.get(loopHandle, "pollAsync() - Invalid Handle"_a8));

        struct timespec specTimeout;
        // when nextTimer is null, specTimeout is initialized to 0, so that PollMode::NoWait
        specTimeout = timerToTimespec(self.loopTime, nextTimer);
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
        FileDescriptor::Handle loopHandle;
        SC_TRY_IF(self.internal.get().loopFd.get(loopHandle, "flushQueue() - Invalid Handle"_a8));

        int res;
        do
        {
            res = kevent(loopHandle, events, newEvents, nullptr, 0, nullptr);
        } while (res == -1 && errno == EINTR);
        if (res != 0)
        {
            return "EventLoop::Internal::flushQueue() - kevent failed"_a8;
        }
        newEvents = 0;
        return true;
    }

    [[nodiscard]] static ReturnCode validateEvent(const struct kevent& event, bool& continueProcessing)
    {
        continueProcessing = (event.flags & EV_DELETE) == 0;
        if ((event.flags & EV_ERROR) != 0)
        {
            return "Error in processing event (kqueue EV_ERROR)"_a8;
        }
        return true;
    }

    // TIMEOUT
    [[nodiscard]] static bool completeAsync(AsyncResult::LoopTimeout& result)
    {
        SC_UNUSED(result);
        SC_RELEASE_ASSERT(false and "Async::Type::LoopTimeout cannot be argument of completion");
        return false;
    }

    // WAKEUP
    [[nodiscard]] static bool completeAsync(AsyncResult::LoopWakeUp& result)
    {
        SC_UNUSED(result);
        SC_RELEASE_ASSERT(false and "Async::Type::LoopWakeUp cannot be argument of completion");
        return false;
    }

    static void completeAsyncLoopWakeUpFromFakeRead(AsyncResult::FileRead& result)
    {
        Async& async = result.async;
        // TODO: Investigate usage of MACHPORT to avoid executing this additional read syscall
        Async::FileRead& readOp   = *async.asFileRead();
        auto             readSpan = readOp.readBuffer;
        do
        {
            const ssize_t res = ::read(readOp.fileDescriptor, readSpan.data(), readSpan.sizeInBytes());

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
    [[nodiscard]] bool setupAsync(Async::SocketAccept& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_READ, EV_ADD | EV_ENABLE);
    }
    [[nodiscard]] static bool activateAsync(Async::SocketAccept&) { return true; }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::SocketAccept& result)
    {
        Async::SocketAccept& async = *result.async.asSocketAccept();
        SocketDescriptor     serverSocket;
        SC_TRY_IF(serverSocket.assign(async.handle));
        auto detach = MakeDeferred([&] { serverSocket.detach(); });
        result.acceptedClient.detach();
        return SocketServer(serverSocket).accept(async.addressFamily, result.acceptedClient);
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::SocketAccept& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_READ);
    }

    // Socket CONNECT
    [[nodiscard]] bool setupAsync(Async::SocketConnect& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_WRITE, EV_ADD | EV_ENABLE);
    }

    [[nodiscard]] static ReturnCode activateAsync(Async::SocketConnect& async)
    {
        SocketDescriptor client;
        SC_TRY_IF(client.assign(async.handle));
        auto detach = MakeDeferred([&] { client.detach(); });
        auto res    = SocketClient(client).connect(async.ipAddress);
        // we expect connect to fail with
        if (res)
        {
            return "connect failed (succeded?)"_a8;
        }
        if (errno != EAGAIN and errno != EINPROGRESS)
        {
            return "connect failed (socket is in blocking mode)"_a8;
        }
        return true;
    }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::SocketConnect& result)
    {
        Async::SocketConnect& async = *result.async.asSocketConnect();

        int       errorCode;
        socklen_t errorSize = sizeof(errorCode);
        const int socketRes = ::getsockopt(async.handle, SOL_SOCKET, SO_ERROR, &errorCode, &errorSize);

        // TODO: This is making a syscall for each connected socket, we should probably aggregate them
        // And additionally it's stupid as probably WRITE will be subscribed again anyway
        // But probably this means to review the entire process of async stop
        SC_TRUST_RESULT(Internal::stopSingleWatcherImmediate(result.async, async.handle, EVFILT_WRITE));
        if (socketRes == 0)
        {
            SC_TRY_MSG(errorCode == 0, "connect SO_ERROR"_a8);
            return true;
        }
        return "connect getsockopt failed"_a8;
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::SocketConnect& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_WRITE);
    }

    // Socket SEND
    [[nodiscard]] ReturnCode setupAsync(Async::SocketSend& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_WRITE, EV_ADD | EV_ENABLE);
    }

    [[nodiscard]] static bool activateAsync(Async::SocketSend&) { return true; }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::SocketSend& result)
    {
        Async::SocketSend& async = *result.async.asSocketSend();
        const ssize_t      res   = ::send(async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        SC_TRY_MSG(res >= 0, "error in send"_a8);
        SC_TRY_MSG(size_t(res) == async.data.sizeInBytes(), "send didn't send all data"_a8);
        return true;
    }

    [[nodiscard]] ReturnCode stopAsync(Async::SocketSend& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_WRITE);
    }

    // Socket RECEIVE
    [[nodiscard]] ReturnCode setupAsync(Async::SocketReceive& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_READ, EV_ADD | EV_ENABLE);
    }

    [[nodiscard]] static bool activateAsync(Async::SocketReceive&) { return true; }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::SocketReceive& result)
    {
        Async::SocketReceive& async = *result.async.asSocketReceive();
        const ssize_t         res   = ::recv(async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        SC_TRY_MSG(res >= 0, "error in recv"_a8);
        return async.data.sliceStartLength(0, static_cast<size_t>(res), result.readData);
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::SocketReceive& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_READ);
    }

    // File READ
    [[nodiscard]] bool setupAsync(Async::FileRead& async)
    {
        return setEventWatcher(async, async.fileDescriptor, EVFILT_READ, EV_ADD);
    }

    [[nodiscard]] static bool activateAsync(Async::FileRead&) { return true; }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::FileRead& result)
    {
        if (&result.async == &result.async.eventLoop->internal.get().wakeupPipeRead)
        {
            completeAsyncLoopWakeUpFromFakeRead(result);
        }
        else
        {
            Async::FileRead& operation = *result.async.asFileRead();
            auto             span      = operation.readBuffer;
            ssize_t          res;
            do
            {
                res = ::pread(operation.fileDescriptor, span.data(), span.sizeInBytes(),
                              static_cast<off_t>(operation.offset));
            } while ((res == -1) and (errno == EINTR));
            SC_TRY_MSG(res >= 0, "::read failed"_a8);
            SC_TRY_IF(result.async.readBuffer.sliceStartLength(0, static_cast<size_t>(res), result.readData));
        }
        return true;
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::FileRead& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.fileDescriptor, EVFILT_READ);
    }

    // File WRITE
    [[nodiscard]] bool setupAsync(Async::FileWrite& async)
    {
        return setEventWatcher(async, async.fileDescriptor, EVFILT_WRITE, EV_ADD);
    }

    [[nodiscard]] static bool activateAsync(Async::FileWrite&) { return true; }

    [[nodiscard]] static ReturnCode completeAsync(AsyncResult::FileWrite& result)
    {
        AsyncFileWrite& async = *result.async.asFileWrite();
        auto            span  = async.writeBuffer;
        ssize_t         res;
        do
        {
            res = ::pwrite(async.fileDescriptor, span.data(), span.sizeInBytes(), static_cast<off_t>(async.offset));
        } while ((res == -1) and (errno == EINTR));
        SC_TRY_MSG(res >= 0, "::write failed"_a8);
        result.writtenBytes = static_cast<size_t>(res);
        return true;
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::FileWrite& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.fileDescriptor, EVFILT_WRITE);
    }

    // PROCESS
    [[nodiscard]] bool setupAsync(Async::ProcessExit& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_PROC, EV_ADD | EV_ENABLE, NOTE_EXIT | NOTE_EXITSTATUS);
    }

    [[nodiscard]] static bool activateAsync(Async::ProcessExit&) { return true; }

    [[nodiscard]] ReturnCode completeAsync(AsyncResult::ProcessExit& result)
    {
        const struct kevent event = events[result.index];
        if ((event.fflags & (NOTE_EXIT | NOTE_EXITSTATUS)) > 0)
        {
            const uint32_t data = static_cast<uint32_t>(event.data);
            if (WIFEXITED(data) != 0)
            {
                result.exitStatus.status.assign(WEXITSTATUS(data));
            }
            return true;
        }
        return false;
    }

    [[nodiscard]] static ReturnCode stopAsync(Async::ProcessExit& async)
    {
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_PROC);
    }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread()
{
    Internal& self = internal.get();
    // TODO: We need an atomic bool swap to wait until next run
    const void* fakeBuffer;
    int         asyncFd;
    ssize_t     writtenBytes;
    SC_TRY_IF(self.wakeupPipe.writePipe.get(asyncFd, "writePipe handle"_a8));
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

SC::ReturnCode SC::EventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor&) { return true; }

SC::ReturnCode SC::EventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor&) { return true; }
