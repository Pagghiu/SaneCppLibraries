// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Deferred.h"
#include "../Async.h"

#if SC_ASYNC_USE_EPOLL

#include <errno.h>        // For error handling
#include <fcntl.h>        // For fcntl function (used for setting non-blocking mode)
#include <signal.h>       // For signal-related functions
#include <sys/epoll.h>    // For epoll functions
#include <sys/signalfd.h> // For signalfd functions
#include <sys/socket.h>   // For socket-related functions

#else

#include <errno.h>     // For error handling
#include <netdb.h>     // socketlen_t/getsocketopt/send/recv
#include <sys/event.h> // kqueue
#include <sys/time.h>  // timespec
#include <sys/wait.h>  // WIFEXITED / WEXITSTATUS
#include <unistd.h>    // read/write/pread/pwrite

#endif

struct SC::AsyncEventLoop::Internal
{
    FileDescriptor loopFd;

    AsyncFileRead  wakeupPipeRead;
    PipeDescriptor wakeupPipe;
    char           wakeupPipeReadBuf[10];
#if SC_ASYNC_USE_EPOLL
    FileDescriptor   signalProcessExitDescriptor;
    AsyncProcessExit signalProcessExit;
#endif

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] Result close()
    {
#if SC_ASYNC_USE_EPOLL
        FileDescriptor::Handle processExitHandle;
        if (signalProcessExitDescriptor.get(processExitHandle, Result::Error("signalProcessExitDescriptor")))
        {
            SC_TRY(stopSingleWatcherImmediate(signalProcessExit, processExitHandle, EPOLLIN | EPOLLET));
        }
        SC_TRY(signalProcessExitDescriptor.close());
#endif
        SC_TRY(wakeupPipe.readPipe.close());
        SC_TRY(wakeupPipe.writePipe.close());
        return loopFd.close();
    }

#if SC_ASYNC_USE_EPOLL
    [[nodiscard]] static bool addEventWatcher(AsyncRequest& async, int fileDescriptor, int32_t filter)
    {
        struct epoll_event event = {0};
        event.events             = filter;
        event.data.ptr           = &async; // data.ptr is a user data pointer
        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRY(async.eventLoop->getLoopFileDescriptor(loopNativeDescriptor));

        int res = ::epoll_ctl(loopNativeDescriptor, EPOLL_CTL_ADD, fileDescriptor, &event);
        if (res == -1)
        {
            return false;
        }
        return true;
    }
#endif

    [[nodiscard]] Result createEventLoop()
    {
#if SC_ASYNC_USE_EPOLL
        const int newQueue = ::epoll_create1(O_CLOEXEC); // Use epoll_create1 instead of kqueue
#else
        const int newQueue = ::kqueue();
#endif
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
#if SC_ASYNC_USE_EPOLL
        SC_TRY(createProcessSignalWatcher(loop));
#endif
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
            Result::Error("AsyncEventLoop::Internal::createSharedWatchers() - AsyncRequest read handle invalid")));
        SC_TRY(wakeupPipeRead.start(loop, wakeUpPipeDescriptor, {wakeupPipeReadBuf, sizeof(wakeupPipeReadBuf)}));
        return Result(true);
    }

#if SC_ASYNC_USE_EPOLL
    // TODO: This should be lazily created on demand
    [[nodiscard]] Result createProcessSignalWatcher(AsyncEventLoop& loop)
    {
        signalProcessExit.eventLoop = &loop;
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);

        if (sigprocmask(SIG_BLOCK, &mask, nullptr) == -1)
        {
            return Result::Error("Failed to set signal mask");
        }
        const int signalFd = signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK);
        if (signalFd == -1)
        {
            return Result::Error("Failed to create signalfd");
        }

        SC_TRY(signalProcessExitDescriptor.assign(signalFd));
        // Signal watcher is active by default.
        // This is a shortcut for .addActiveHandle() and .decreaseActiveCount().
        signalProcessExit.state = AsyncRequest::State::Active;
        SC_TRY(addEventWatcher(signalProcessExit, signalFd, EPOLLIN | EPOLLET));
        return Result(true);
    }
#endif

#if SC_ASYNC_USE_EPOLL
    [[nodiscard]] static AsyncRequest* getAsyncRequest(const struct epoll_event& event)
    {
        return static_cast<AsyncRequest*>(event.data.ptr);
    }
#else
    [[nodiscard]] static AsyncRequest* getAsyncRequest(const struct kevent& event)
    {
        return static_cast<AsyncRequest*>(event.udata);
    }
#endif

    [[nodiscard]] static Result stopSingleWatcherImmediate(AsyncRequest& async, SocketDescriptor::Handle handle,
                                                           int32_t filter)
    {
        FileDescriptor::Handle loopNativeDescriptor;
        SC_TRY(async.eventLoop->internal.get().loopFd.get(
            loopNativeDescriptor, Result::Error("AsyncEventLoop::Internal::pollAsync() - Invalid Handle")));
#if SC_ASYNC_USE_EPOLL
        struct epoll_event event;
        event.events   = filter;
        event.data.ptr = &async;
        const int res  = ::epoll_ctl(loopNativeDescriptor, EPOLL_CTL_DEL, handle, &event);
#else
        struct kevent kev;
        EV_SET(&kev, handle, filter, EV_DELETE, 0, 0, nullptr);
        const int res = ::kevent(loopNativeDescriptor, &kev, 1, 0, 0, nullptr);
#endif
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

#if SC_ASYNC_USE_EPOLL
    epoll_event events[totalNumEvents];
#else
    struct kevent events[totalNumEvents];
#endif
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
#if SC_ASYNC_USE_EPOLL
        case AsyncRequest::Type::FileWrite:
#endif
        case AsyncRequest::Type::SocketClose:
        case AsyncRequest::Type::FileClose: {
            async.eventLoop->scheduleManualCompletion(async);
            break;
        }
        default: {
#if SC_ASYNC_USE_EPOLL
            if (async.type == AsyncRequest::Type::FileRead)
            {
                if (&async != &async.eventLoop->internal.get().wakeupPipeRead)
                {
                    async.eventLoop->scheduleManualCompletion(async);
                    break;
                }
            }
#endif
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

#if SC_ASYNC_USE_EPOLL
    // In epoll (differently from kqueue) the watcher is immediately added, so we call this 'add' instead of 'set'
    [[nodiscard]] bool addEventWatcher(AsyncRequest& async, int fileDescriptor, int32_t filter)
    {
        return Internal::addEventWatcher(async, fileDescriptor, filter);
    }
#else
    [[nodiscard]] bool setEventWatcher(AsyncRequest& async, int fileDescriptor, short filter, unsigned int options = 0)
    {
        // NOTE: newEvents will be incremented in ::pushNewSubmission()
        EV_SET(events + newEvents, fileDescriptor, filter, EV_ADD | EV_ENABLE, options, 0, &async);
        return true;
    }
#endif

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
#if SC_ASYNC_USE_EPOLL
            res = ::epoll_pwait2(loopHandle, events, totalNumEvents, spec, 0);
#else
            res = ::kevent(loopHandle, events, newEvents, events, totalNumEvents, spec);
#endif
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
#if SC_ASYNC_USE_EPOLL
        SC_COMPILER_UNUSED(eventLoop);
        // TODO: Implement flush for epoll
#else
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
#endif
        return Result(true);
    }

#if SC_ASYNC_USE_EPOLL
    [[nodiscard]] static Result validateEvent(const epoll_event& event, bool& continueProcessing)
    {
        continueProcessing = true;

        if ((event.events & EPOLLERR) != 0 || (event.events & EPOLLHUP) != 0)
        {
            continueProcessing = false;
            return Result::Error("Error in processing event (epoll EPOLLERR or EPOLLHUP)");
        }
        return Result(true);
    }
#else
    [[nodiscard]] static Result validateEvent(const struct kevent& event, bool& continueProcessing)
    {
        continueProcessing = (event.flags & EV_DELETE) == 0;
        if ((event.flags & EV_ERROR) != 0)
        {
            return Result::Error("Error in processing event (kqueue EV_ERROR)");
        }
        return Result(true);
    }
#endif

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
        // TODO: Investigate MACHPORT (kqueue) and eventfd (epoll) to avoid the additional read syscall
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
#if SC_ASYNC_USE_EPOLL
        return addEventWatcher(async, async.handle, EPOLLIN);
#else
        return setEventWatcher(async, async.handle, EVFILT_READ);
#endif
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
#if SC_ASYNC_USE_EPOLL
        return Internal::stopSingleWatcherImmediate(async, async.handle, EPOLLIN);
#else
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_READ);
#endif
    }

    // Socket CONNECT
    [[nodiscard]] bool setupAsync(AsyncSocketConnect& async)
    {
#if SC_ASYNC_USE_EPOLL
        return addEventWatcher(async, async.handle, EPOLLOUT);
#else
        return setEventWatcher(async, async.handle, EVFILT_WRITE);
#endif
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
#if SC_ASYNC_USE_EPOLL
        SC_TRUST_RESULT(Internal::stopSingleWatcherImmediate(result.async, async.handle, EPOLLOUT));
#else
        SC_TRUST_RESULT(Internal::stopSingleWatcherImmediate(result.async, async.handle, EVFILT_WRITE));
#endif
        if (socketRes == 0)
        {
            SC_TRY_MSG(errorCode == 0, "connect SO_ERROR");
            return Result(true);
        }
        return Result::Error("connect getsockopt failed");
    }

    [[nodiscard]] static Result stopAsync(AsyncSocketConnect& async)
    {
#if SC_ASYNC_USE_EPOLL
        return Internal::stopSingleWatcherImmediate(async, async.handle, EPOLLOUT);
#else
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_WRITE);
#endif
    }

    // Socket SEND
    [[nodiscard]] Result setupAsync(AsyncSocketSend& async)
    {
#if SC_ASYNC_USE_EPOLL
        return Result(addEventWatcher(async, async.handle, EPOLLOUT));
#else
        return Result(setEventWatcher(async, async.handle, EVFILT_WRITE));
#endif
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
#if SC_ASYNC_USE_EPOLL
        return Internal::stopSingleWatcherImmediate(async, async.handle, EPOLLOUT);
#else
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_WRITE);
#endif
    }

    // Socket RECEIVE
    [[nodiscard]] Result setupAsync(AsyncSocketReceive& async)
    {
#if SC_ASYNC_USE_EPOLL
        return Result(addEventWatcher(async, async.handle, EPOLLIN | EPOLLRDHUP));
#else
        return Result(setEventWatcher(async, async.handle, EVFILT_READ));
#endif
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
#if SC_ASYNC_USE_EPOLL
        return Internal::stopSingleWatcherImmediate(async, async.handle, EPOLLIN | EPOLLRDHUP);
#else
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_READ);
#endif
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
#if SC_ASYNC_USE_EPOLL
        // TODO: Check if we need EPOLLET for edge-triggered mode
        if (&async == &async.eventLoop->internal.get().wakeupPipeRead)
        {
            return addEventWatcher(async, async.fileDescriptor, EPOLLIN | EPOLLET);
        }
        // TODO: epoll doesn't support regular file descriptors (needs a thread pool).
        return true;
#else
        return setEventWatcher(async, async.fileDescriptor, EVFILT_READ);
#endif
    }

    [[nodiscard]] static bool activateAsync(AsyncFileRead& async)
    {
#if SC_ASYNC_USE_EPOLL
        if (&async != &async.eventLoop->internal.get().wakeupPipeRead)
        {
            // TODO: This is a synchronous operation! Run this code in a threadpool.
            Result                res(true);
            AsyncFileRead::Result result(async, move(res));
            if (executeOperation(result))
            {
                result.async.syncReadBytes = result.readData.sizeInBytes();
                return true;
            }
            return false;
        }
#else
        SC_COMPILER_UNUSED(async);
#endif
        return true;
    }

    [[nodiscard]] static Result executeOperation(AsyncFileRead::Result& result)
    {
        auto    span = result.async.readBuffer;
        ssize_t res;
        do
        {
            res = ::pread(result.async.fileDescriptor, span.data(), span.sizeInBytes(),
                          static_cast<off_t>(result.async.offset));
        } while ((res == -1) and (errno == EINTR));
        SC_TRY_MSG(res >= 0, "::read failed");
        return Result(result.async.readBuffer.sliceStartLength(0, static_cast<size_t>(res), result.readData));
    }

    [[nodiscard]] static Result completeAsync(AsyncFileRead::Result& result)
    {
        if (&result.async == &result.async.eventLoop->internal.get().wakeupPipeRead)
        {
            completeAsyncLoopWakeUpFromFakeRead(result);
            return Result(true);
        }
        else
        {
#if SC_ASYNC_USE_EPOLL
            return Result(result.async.readBuffer.sliceStartLength(0, result.async.syncReadBytes, result.readData));
#else
            return executeOperation(result);
#endif
        }
    }

    [[nodiscard]] static Result stopAsync(AsyncFileRead& async)
    {
#if SC_ASYNC_USE_EPOLL
        if (&async == &async.eventLoop->internal.get().wakeupPipeRead)
        {
            return Internal::stopSingleWatcherImmediate(async, async.fileDescriptor, EPOLLIN | EPOLLET);
        }
        return Result(true);
#else
        return Internal::stopSingleWatcherImmediate(async, async.fileDescriptor, EVFILT_READ);
#endif
    }

    // File WRITE
    [[nodiscard]] bool setupAsync(AsyncFileWrite& async)
    {
#if SC_ASYNC_USE_EPOLL
        SC_COMPILER_UNUSED(async);
        // TODO: epoll doesn't support regular file descriptors (needs a thread pool).
        return true;
#else
        return setEventWatcher(async, async.fileDescriptor, EVFILT_WRITE);
#endif
    }

    [[nodiscard]] static bool activateAsync(AsyncFileWrite& async)
    {
#if SC_ASYNC_USE_EPOLL
        // TODO: This is a synchronous operation! Run this code in a threadpool.
        Result                 res(true);
        AsyncFileWrite::Result result(async, move(res));
        if (executeOperation(result))
        {
            async.syncWrittenBytes = result.writtenBytes;
            return true;
        }
        return false;
#else
        SC_COMPILER_UNUSED(async);
        return true;
#endif
    }

    [[nodiscard]] static Result executeOperation(AsyncFileWrite::Result& result)
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

    [[nodiscard]] static Result completeAsync(AsyncFileWrite::Result& result)
    {
#if SC_ASYNC_USE_EPOLL
        result.writtenBytes = result.async.syncWrittenBytes;
        return Result(true);
#else
        return executeOperation(result);
#endif
    }

    [[nodiscard]] static Result stopAsync(AsyncFileWrite& async)
    {
#if SC_ASYNC_USE_EPOLL
        SC_COMPILER_UNUSED(async);
        // TODO: epoll doesn't support regular file descriptors (needs a thread pool).
        return Result(true);
#else
        return Internal::stopSingleWatcherImmediate(async, async.fileDescriptor, EVFILT_WRITE);
#endif
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
#if SC_ASYNC_USE_EPOLL
        async.eventLoop->activeProcessChild.queueBack(async);
        return true;
#else
        return setEventWatcher(async, async.handle, EVFILT_PROC, NOTE_EXIT | NOTE_EXITSTATUS);
#endif
    }

    [[nodiscard]] static bool activateAsync(AsyncProcessExit&) { return true; }

#if SC_ASYNC_USE_EPOLL
    [[nodiscard]] Result completeAsync(AsyncProcessExit::Result& result)
    {
        struct signalfd_siginfo siginfo;
        FileDescriptor::Handle  sigHandle;

        Internal& internal = result.async.eventLoop->internal.get();
        SC_TRY(internal.signalProcessExitDescriptor.get(sigHandle, Result::Error("Invalid signal handle")));
        ssize_t size = ::read(sigHandle, &siginfo, sizeof(siginfo));

        if (size == sizeof(siginfo))
        {
            // Check if the received signal is related to process exit
            if (siginfo.ssi_signo == SIGCHLD)
            {
                // Loop all process handles to find if one of our interest has exited
                AsyncProcessExit* current = result.async.eventLoop->activeProcessChild.front;

                while (current)
                {
                    if (siginfo.ssi_pid == current->handle)
                    {
                        result.exitStatus.status = siginfo.ssi_status;
                        result.async.eventLoop->activeProcessChild.remove(*current);
                        current->callback(result);
                        // TODO: Handle lazy deactivation for signals when no more processes exist
                        result.reactivateRequest(true);
                        return Result(true);
                    }
                    current = static_cast<AsyncProcessExit*>(current->next);
                }
            }
        }
        return Result(true); // Return true even if unhandled, we have received a SIGCHLD for a non monitored pid
    }
#else
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
#endif
    [[nodiscard]] static Result stopAsync(AsyncProcessExit& async)
    {
#if SC_ASYNC_USE_EPOLL
        SC_COMPILER_UNUSED(async);
        return Result(true);
#else
        return Internal::stopSingleWatcherImmediate(async, async.handle, EVFILT_PROC);
#endif
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
