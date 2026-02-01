// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Async/Internal/AsyncInternal.h"
#include <poll.h>

#include "../../Foundation/Assert.h"
#include "../../Foundation/Deferred.h"
#include "../../Socket/Socket.h"

#if SC_PLATFORM_LINUX
#define SC_ASYNC_USE_EPOLL 1 // uses epoll
#endif

#if SC_ASYNC_USE_EPOLL

#include <errno.h>        // For error handling
#include <fcntl.h>        // For fcntl function (used for setting non-blocking mode)
#include <signal.h>       // For signal-related functions
#include <sys/epoll.h>    // For epoll functions
#include <sys/sendfile.h> // For sendfile
#include <sys/signalfd.h> // For signalfd functions
#include <sys/socket.h>   // For socket-related functions
#include <sys/stat.h>     // fstat
#include <sys/uio.h>      // writev, pwritev
#include <sys/wait.h>     // waitpid / WIFEXITED / WEXITSTATUS

#else

#include <errno.h>     // For error handling
#include <netdb.h>     // socklen_t/getsockopt/recv
#include <sys/event.h> // kqueue
#include <sys/stat.h>  // fstat
#include <sys/time.h>  // timespec
#include <sys/uio.h>   // writev, pwritev
#include <sys/wait.h>  // WIFEXITED / WEXITSTATUS
#include <unistd.h>    // read/write/pread/pwrite
#endif

struct SC::AsyncEventLoop::Internal::KernelQueuePosix
{
    FileDescriptor loopFd;

    AsyncFilePoll  wakeUpPoll;
    PipeDescriptor wakeupPipe;
#if SC_ASYNC_USE_EPOLL
    FileDescriptor signalProcessExitDescriptor;
    AsyncFilePoll  signalProcessExit;
#endif
    KernelQueuePosix() {}
    ~KernelQueuePosix() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] static constexpr bool needsThreadPoolForFileOperations() { return true; }

    const KernelQueuePosix& getPosix() const { return *this; }

    Result close()
    {
#if SC_ASYNC_USE_EPOLL
        SC_TRY(signalProcessExitDescriptor.close());
#endif
        SC_TRY(wakeupPipe.readPipe.close());
        SC_TRY(wakeupPipe.writePipe.close());
        return loopFd.close();
    }

    Result createEventLoop(AsyncEventLoop::Options options = AsyncEventLoop::Options())
    {
        if (options.apiType == AsyncEventLoop::Options::ApiType::ForceUseIOURing)
        {
            return Result::Error("createEventLoop: Cannot use io_uring");
        }
#if SC_ASYNC_USE_EPOLL
        const int newQueue = ::epoll_create1(O_CLOEXEC);
#else
        const int newQueue = ::kqueue();
#endif
        if (newQueue == -1)
        {
            // TODO: Better error handling
            return Result::Error("AsyncEventLoop::KernelQueuePosix::createEventLoop() failed");
        }
        SC_TRY(loopFd.assign(newQueue));
        return Result(true);
    }

    Result createSharedWatchers(AsyncEventLoop& eventLoop)
    {
#if SC_ASYNC_USE_EPOLL
        SC_TRY(createProcessSignalWatcher(eventLoop));
#endif
        SC_TRY(createWakeup(eventLoop));
        SC_TRY(eventLoop.runNoWait()); // Register the read handle before everything else
        // Calls to excludeFromActiveCount must be after runNoWait()

        // WakeUp (poll) doesn't keep the kernelEvents active
        eventLoop.excludeFromActiveCount(wakeUpPoll);
        wakeUpPoll.flags |= Flag_Internal;
#if SC_ASYNC_USE_EPOLL
        // Process watcher doesn't keep the kernelEvents active
        eventLoop.excludeFromActiveCount(signalProcessExit);
        signalProcessExit.flags |= Flag_Internal;
#endif
        return Result(true);
    }

    Result createWakeup(AsyncEventLoop& eventLoop)
    {
        // Create
        PipeOptions options;
        options.blocking = false;
        SC_TRY(wakeupPipe.createPipe(options));

        // Register
        FileDescriptor::Handle wakeUpPipeDescriptor;
        SC_TRY(wakeupPipe.readPipe.get(
            wakeUpPipeDescriptor,
            Result::Error(
                "AsyncEventLoop::KernelQueuePosix::createSharedWatchers() - AsyncRequest read handle invalid")));
        wakeUpPoll.callback.bind<&KernelQueuePosix::completeWakeUp>();
        wakeUpPoll.setDebugName("SharedWakeUpPoll");
        SC_TRY(wakeUpPoll.start(eventLoop, wakeUpPipeDescriptor));
        return Result(true);
    }

    static void completeWakeUp(AsyncFilePoll::Result& result)
    {
        AsyncFilePoll& async = result.getAsync();
        // TODO: Investigate MACHPORT (kqueue) and eventfd (epoll) to avoid the additional read syscall

        char fakeBuffer[10];
        for (;;)
        {
            ssize_t res;
            do
            {
                res = ::read(async.handle, fakeBuffer, sizeof(fakeBuffer));
            } while (res < 0 and errno == EINTR);

            if (res >= 0 and (static_cast<size_t>(res) == sizeof(fakeBuffer)))
                continue;

            if (res != -1)
                break;

            if (errno == EWOULDBLOCK or errno == EAGAIN)
                break;
        }
        result.eventLoop.internal.executeWakeUps(result.eventLoop);
        result.reactivateRequest(true);
    }

    Result wakeUpFromExternalThread()
    {
        // TODO: We need an atomic bool swap to wait until next run
        int asyncFd;
        SC_TRY(wakeupPipe.writePipe.get(asyncFd, Result::Error("writePipe handle")));
        ssize_t writtenBytes;
        do
        {
            writtenBytes = ::write(asyncFd, "", 1);
        } while (writtenBytes == -1 && errno == EINTR);

        if (writtenBytes != 1)
        {
            return Result::Error("AsyncEventLoop::wakeUpFromExternalThread - Error in write");
        }
        return Result(true);
    }

#if SC_ASYNC_USE_EPOLL
    // TODO: This should be lazily created on demand
    // TODO: Or it's probably even better migrate this one to pidfd
    Result createProcessSignalWatcher(AsyncEventLoop& eventLoop)
    {
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
        signalProcessExit.callback.bind<KernelQueuePosix, &KernelQueuePosix::onSIGCHLD>(*this);
        return signalProcessExit.start(eventLoop, signalFd);
    }

    void onSIGCHLD(AsyncFilePoll::Result& result)
    {
        struct signalfd_siginfo siginfo;
        FileDescriptor::Handle  sigHandle;

        Result res = signalProcessExitDescriptor.get(sigHandle, Result::Error("Invalid signal handle"));
        if (not res)
        {
            return;
        }
        const ssize_t size = ::read(sigHandle, &siginfo, sizeof(siginfo));

        // TODO: Handle lazy deactivation for signals when no more processes exist
        result.reactivateRequest(true);

        if (size != sizeof(siginfo))
        {
            return;
        }

        // Check if the received signal is related to process exit
        if (siginfo.ssi_signo != SIGCHLD)
        {
            return;
        }
        while (true)
        {
            // Multiple SIGCHLD may have been merged together, we must check all of them with waitpid(-1)
            // https://stackoverflow.com/questions/8398298/handling-multiple-sigchld
            int   status = -1;
            pid_t pid;
            do
            {
                pid = ::waitpid(-1, &status, 0);
            } while (pid == -1 and errno == EINTR);
            if (pid == -1)
            {
                return; // no more queued child processes
            }

            // Loop all process handles to find if one of our interest has exited
            AsyncProcessExit* current = result.eventLoop.internal.activeProcessExits.front;

            while (current)
            {
                if (pid == current->handle)
                {
                    Result                   res(true);
                    AsyncProcessExit::Result processResult(result.eventLoop, *current, res);
                    processResult.completionData.exitStatus = WEXITSTATUS(status);
                    result.eventLoop.internal.removeActiveHandle(*current);
                    current->callback(processResult);
                    break;
                }
                current = static_cast<AsyncProcessExit*>(current->next);
            }
        }
    }

    static Result setEventWatcher(AsyncEventLoop& eventLoop, AsyncRequest& async, int fileDescriptor, int32_t filter)
    {
        struct epoll_event event = {0};
        event.events             = filter;
        event.data.ptr           = &async; // data.ptr is a user data pointer
        FileDescriptor::Handle loopFd;
        SC_TRY(eventLoop.internal.kernelQueue.get().getPosix().loopFd.get(loopFd, Result::Error("loop")));

        int res = ::epoll_ctl(loopFd, EPOLL_CTL_ADD, fileDescriptor, &event);
        if (res == -1)
        {
            return Result::Error("epoll_ctl");
        }
        return Result(true);
    }

#endif
    template <int VALUE>
    static Result setSingleWatcherImmediate(AsyncEventLoop& eventLoop, SocketDescriptor::Handle handle, int32_t filter)
    {
        FileDescriptor::Handle loopFd;
        SC_TRY(eventLoop.internal.kernelQueue.get().getPosix().loopFd.get(
            loopFd, Result::Error("AsyncEventLoop::KernelQueuePosix::syncWithKernel() - Invalid Handle")));
#if SC_ASYNC_USE_EPOLL
        struct epoll_event event;
        event.events   = filter;
        event.data.ptr = nullptr;
        const int res  = ::epoll_ctl(loopFd, VALUE, handle, &event);
#else
        struct kevent kev;
        EV_SET(&kev, handle, filter, VALUE, 0, 0, nullptr);
        const int res = ::kevent(loopFd, &kev, 1, 0, 0, nullptr);
#endif
        if (res == 0 or (errno == EBADF or errno == ENOENT))
        {
            return Result(true);
        }
        return Result::Error("stopSingleWatcherImmediate failed");
    }

    static Result stopSingleWatcherImmediate(AsyncEventLoop& eventLoop, SocketDescriptor::Handle handle, int32_t filter)
    {
#if SC_ASYNC_USE_EPOLL
        constexpr auto VALUE = EPOLL_CTL_DEL;
#else
        constexpr auto VALUE = EV_DELETE;
#endif
        return setSingleWatcherImmediate<VALUE>(eventLoop, handle, filter);
    }

    static Result startSingleWatcherImmediate(AsyncEventLoop& eventLoop, SocketDescriptor::Handle handle,
                                              int32_t filter)
    {
#if SC_ASYNC_USE_EPOLL
        constexpr auto VALUE = EPOLL_CTL_ADD;
#else
        constexpr auto VALUE = EV_ADD;
#endif
        return setSingleWatcherImmediate<VALUE>(eventLoop, handle, filter);
    }

    static Result associateExternallyCreatedSocket(SocketDescriptor&) { return Result(true); }
    static Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
    static Result removeAllAssociationsFor(SocketDescriptor&) { return Result(true); }
    static Result removeAllAssociationsFor(FileDescriptor&) { return Result(true); }
};

struct SC::AsyncEventLoop::Internal::KernelEventsPosix
{
  private:
#if SC_ASYNC_USE_EPOLL
    epoll_event* events;
#else
    struct kevent* events;
#endif
    KernelEvents& parentKernelEvents;

    int&      newEvents;
    const int totalNumEvents;

  public:
#if SC_PLATFORM_APPLE
    KernelEventsPosix(KernelQueue&, AsyncKernelEvents& kernelEvents)
        : parentKernelEvents(*this),
#else
    KernelEventsPosix(KernelEvents& ke, AsyncKernelEvents& kernelEvents)
        : parentKernelEvents(ke),
#endif
          newEvents(kernelEvents.numberOfEvents),
          totalNumEvents(static_cast<int>(kernelEvents.eventsMemory.sizeInBytes() / sizeof(events[0])))
    {
        events = reinterpret_cast<decltype(events)>(kernelEvents.eventsMemory.data());
    }

    uint32_t getNumEvents() const { return static_cast<uint32_t>(newEvents); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t idx) const
    {
#if SC_ASYNC_USE_EPOLL
        return static_cast<AsyncRequest*>(events[idx].data.ptr);
#else
        return static_cast<AsyncRequest*>(events[idx].udata);
#endif
    }

#if SC_ASYNC_USE_EPOLL

    static constexpr int32_t INPUT_EVENTS_MASK         = EPOLLIN;
    static constexpr int32_t OUTPUT_EVENTS_MASK        = EPOLLOUT;
    static constexpr int32_t SOCKET_INPUT_EVENTS_MASK  = EPOLLIN | EPOLLRDHUP;
    static constexpr int32_t SOCKET_OUTPUT_EVENTS_MASK = EPOLLOUT;

    // In epoll (differently from kqueue) the watcher is immediately added
    static Result setEventWatcher(AsyncEventLoop& eventLoop, AsyncRequest& async, int fileDescriptor, int32_t filter)
    {
        return KernelQueuePosix::setEventWatcher(eventLoop, async, fileDescriptor, filter);
    }

    [[nodiscard]] static bool isDescriptorWriteWatchable(int fd, bool& canBeWatched)
    {
        struct stat file_stat;
        if (::fstat(fd, &file_stat) == -1)
        {
            return false;
        }
        // epoll doesn't support regular file descriptors
        canBeWatched = S_ISREG(file_stat.st_mode) == 0;
        return true;
    }

    Result validateEvent(uint32_t idx, bool& continueProcessing)
    {
        const epoll_event& event = events[idx];
        continueProcessing       = true;

        const bool epollHUP = (event.events & EPOLLHUP) != 0;
        const bool epollERR = (event.events & EPOLLERR) != 0;
        if (epollERR or epollHUP)
        {
            const AsyncRequest* request = getAsyncRequest(idx);
            if (request->type == AsyncRequest::Type::FileRead or request->type == AsyncRequest::Type::FileWrite)
            {
                return Result(true);
            }
            continueProcessing = false;
            return Result::Error("Error in processing event (epoll EPOLLERR or EPOLLHUP)");
        }
        return Result(true);
    }

#else
    static constexpr short INPUT_EVENTS_MASK  = EVFILT_READ;
    static constexpr short OUTPUT_EVENTS_MASK = EVFILT_WRITE;

    static constexpr short SOCKET_INPUT_EVENTS_MASK  = EVFILT_READ;
    static constexpr short SOCKET_OUTPUT_EVENTS_MASK = EVFILT_WRITE;

    Result setEventWatcher(AsyncEventLoop& eventLoop, AsyncRequest& async, int fileDescriptor, short filter,
                           unsigned int options = 0)
    {
        EV_SET(events + newEvents, fileDescriptor, filter, EV_ADD, options, 0, &async);
        newEvents += 1;
        if (newEvents >= totalNumEvents)
        {
            SC_TRY(flushQueue(eventLoop));
        }
        return Result(true);
    }

    Result flushQueue(AsyncEventLoop& eventLoop)
    {
        FileDescriptor::Handle loopFd;
        SC_TRY(eventLoop.internal.kernelQueue.get().loopFd.get(loopFd, Result::Error("flushQueue() - Invalid Handle")));

        int res;
        do
        {
            res = ::kevent(loopFd, events, newEvents, nullptr, 0, nullptr);
        } while (res == -1 && errno == EINTR);
        if (res != 0)
        {
            return Result::Error("AsyncEventLoop::KernelQueuePosix::flushQueue() - kevent failed");
        }
        newEvents = 0;
        return Result(true);
    }

    [[nodiscard]] static constexpr bool isDescriptorWriteWatchable(int, bool& canBeWatched)
    {
        canBeWatched = true; // kevent can also watch regular buffered files (differently from epoll)
        return true;
    }

    Result validateEvent(uint32_t idx, bool& continueProcessing)
    {
        const struct kevent& event = events[idx];
        continueProcessing         = (event.flags & EV_DELETE) == 0;
        if ((event.flags & EV_ERROR) != 0)
        {
            const AsyncRequest* request = getAsyncRequest(idx);
            // Processes that exit too fast error out with ESRCH errno, but we do not consider it an error...
            if (request->type != AsyncRequest::Type::ProcessExit or event.data != ESRCH)
            {
                return Result::Error("Error in processing event (kqueue EV_ERROR)");
            }
        }
        return Result(true);
    }
#endif

    [[nodiscard]] static bool isDescriptorReadWatchable(int fd, bool& canBeWatched)
    {
        struct stat file_stat;
        if (::fstat(fd, &file_stat) == -1)
        {
            return false;
        }
        // epoll doesn't support regular file descriptors
        // kqueue doesn't report EOF on vnodes (regular files) for EVFILT_READ
        canBeWatched = S_ISREG(file_stat.st_mode) == 0;
        return true;
    }

    static struct timespec timerToRelativeTimespec(const TimeMs& loopTime, const TimeMs* nextTimer)
    {
        struct timespec specTimeout;
        if (nextTimer)
        {
            if (nextTimer->milliseconds >= loopTime.milliseconds)
            {
                const auto diff = nextTimer->milliseconds - loopTime.milliseconds;

                specTimeout.tv_sec  = diff / 1000;
                specTimeout.tv_nsec = (diff % 1000) * 1000 * 1000;
                return specTimeout;
            }
        }
        specTimeout.tv_sec  = 0;
        specTimeout.tv_nsec = 0;
        return specTimeout;
    }

    Result syncWithKernel(AsyncEventLoop& eventLoop, Internal::SyncMode syncMode)
    {
        AsyncLoopTimeout* loopTimeout = nullptr;
        const TimeMs*     nextTimer   = nullptr;
        if (syncMode == Internal::SyncMode::ForcedForwardProgress)
        {
            loopTimeout = eventLoop.internal.findEarliestLoopTimeout();
            if (loopTimeout)
            {
                nextTimer = &loopTimeout->expirationTime;
            }
        }
        static constexpr Result errorResult = Result::Error("syncWithKernel() - Invalid Handle");
        FileDescriptor::Handle  loopFd;
        SC_TRY(eventLoop.internal.kernelQueue.get().getPosix().loopFd.get(loopFd, errorResult));

        struct timespec specTimeout;
        // when nextTimer is null, specTimeout is initialized to 0, so that SyncMode::NoWait
        specTimeout = timerToRelativeTimespec(eventLoop.internal.loopTime, nextTimer);
        int res;
        do
        {
            auto spec = nextTimer or syncMode == Internal::SyncMode::NoWait ? &specTimeout : nullptr;
#if SC_ASYNC_USE_EPOLL
            int timeout_ms = -1; // infinite wait
            if (spec)
            {
                if (spec->tv_sec == 0 && spec->tv_nsec == 0)
                {
                    timeout_ms = 0; // no wait
                }
                else
                {
                    timeout_ms = static_cast<int>((spec->tv_sec * 1000) + (spec->tv_nsec / 1000000));
                }
            }
            res = ::epoll_pwait(loopFd, events, totalNumEvents, timeout_ms, 0);
#else
            res = ::kevent(loopFd, events, newEvents, events, totalNumEvents, spec);
#endif
            if (res == -1 && errno == EINTR)
            {
                // Interrupted, we must recompute timeout
                if (nextTimer)
                {
                    eventLoop.internal.updateTime();
                    specTimeout = timerToRelativeTimespec(eventLoop.internal.loopTime, nextTimer);
                }
                continue;
            }
            break;
        } while (true);
        if (res == -1)
        {
            return Result::Error("AsyncEventLoop::KernelQueuePosix::poll() - failed");
        }
        newEvents = static_cast<int>(res);
        if (loopTimeout)
        {
            eventLoop.internal.runTimers = true;
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // TIMEOUT
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncLoopTimeout& async)
    {
        async.expirationTime = Internal::offsetTimeClamped(eventLoop.getLoopTime(), async.relativeTimeout);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // WAKEUP
    //-------------------------------------------------------------------------------------------------------
    // Nothing to do :)

    //-------------------------------------------------------------------------------------------------------
    // WORK
    //-------------------------------------------------------------------------------------------------------
    static Result executeOperation(AsyncLoopWork& loopWork, AsyncLoopWork::CompletionData&) { return loopWork.work(); }

    //-------------------------------------------------------------------------------------------------------
    // Socket ACCEPT
    //-------------------------------------------------------------------------------------------------------
    Result setupAsync(AsyncEventLoop& eventLoop, AsyncSocketAccept& async)
    {
        return setEventWatcher(eventLoop, async, async.handle, INPUT_EVENTS_MASK);
    }

    static Result teardownAsync(AsyncSocketAccept*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.socketHandle,
                                                            INPUT_EVENTS_MASK);
    }

    static Result completeAsync(AsyncSocketAccept::Result& result)
    {
        AsyncSocketAccept& async = result.getAsync();
        SocketDescriptor   serverSocket;
        SC_TRY(serverSocket.assign(async.handle));
        auto detach = MakeDeferred([&] { serverSocket.detach(); });
        result.completionData.acceptedClient.detach();
        return SocketServer(serverSocket).accept(async.addressFamily, result.completionData.acceptedClient);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket CONNECT
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketConnect& async)
    {
        SocketDescriptor client;
        SC_TRY(client.assign(async.handle));
        auto detach = MakeDeferred([&] { client.detach(); });
        auto res    = SocketClient(client).connect(async.ipAddress);
        if (res)
        {
            return Result::Error("connect unexpected error");
        }
        if (errno != EAGAIN and errno != EINPROGRESS)
        {
            return Result::Error("connect failed");
        }

        async.flags |= Internal::Flag_WatcherSet;
        return setEventWatcher(eventLoop, async, async.handle, OUTPUT_EVENTS_MASK);
    }

    static Result completeAsync(AsyncSocketConnect::Result& result)
    {
        AsyncSocketConnect& async = result.getAsync();

        int       errorCode;
        socklen_t errorSize = sizeof(errorCode);
        const int socketRes = ::getsockopt(async.handle, SOL_SOCKET, SO_ERROR, &errorCode, &errorSize);

        // TODO: This is making a syscall for each connected socket, we should probably aggregate them
        // And additionally it's stupid as probably WRITE will be subscribed again anyway
        // But probably this means to review the entire process of async stop
        AsyncEventLoop& eventLoop = result.eventLoop;
        async.flags &= ~Internal::Flag_WatcherSet;
        SC_TRUST_RESULT(KernelQueuePosix::stopSingleWatcherImmediate(eventLoop, async.handle, OUTPUT_EVENTS_MASK));
        if (socketRes == 0)
        {
            SC_TRY_MSG(errorCode == 0, "connect SO_ERROR");
            return Result(true);
        }
        return Result::Error("connect getsockopt failed");
    }

    //-------------------------------------------------------------------------------------------------------
    // Posix Write (Shared between Socket Send and File Write)
    //-------------------------------------------------------------------------------------------------------

    /// @brief Stops the write watcher for the given socket handle if no other async is monitoring it.
    /// Otherwise updates the watcher to point to a valid async.
    static Result posixUpdateSocketWriteWatcher(AsyncEventLoop& eventLoop, SocketDescriptor::Handle handle,
                                                decltype(AsyncRequest::flags)& flags)
    {
        if ((flags & Internal::Flag_WatcherSet) == 0)
        {
            return Result(true);
        }
        // Check activeSocketSends
        {
            AsyncSocketSend* current = eventLoop.internal.activeSocketSends.front;
            while (current)
            {
                if (handle == current->handle and (current->flags & Internal::Flag_WatcherSet))
                {
                    return KernelQueuePosix::startSingleWatcherImmediate(eventLoop, current->handle,
                                                                         OUTPUT_EVENTS_MASK);
                }
                current = static_cast<AsyncSocketSend*>(current->next);
            }
        }
        // Check activeSocketSendsTo
        {
            AsyncSocketSendTo* current = eventLoop.internal.activeSocketSendsTo.front;
            while (current)
            {
                if (handle == current->handle and (current->flags & Internal::Flag_WatcherSet))
                {
                    return KernelQueuePosix::startSingleWatcherImmediate(eventLoop, current->handle,
                                                                         OUTPUT_EVENTS_MASK);
                }
                current = static_cast<AsyncSocketSendTo*>(current->next);
            }
        }
        // Check activeFileSends
        {
            AsyncFileSend* current = eventLoop.internal.activeFileSends.front;
            while (current)
            {
                if (handle == current->socketHandle and (current->flags & Internal::Flag_WatcherSet))
                {
                    return KernelQueuePosix::startSingleWatcherImmediate(eventLoop, current->socketHandle,
                                                                         OUTPUT_EVENTS_MASK);
                }
                current = static_cast<AsyncFileSend*>(current->next);
            }
        }
        // No other async is monitoring this handle, we can stop the watcher
        flags &= ~Internal::Flag_WatcherSet;
        return KernelQueuePosix::stopSingleWatcherImmediate(eventLoop, handle, OUTPUT_EVENTS_MASK);
    }

    /// @brief Stops the write watcher for the given file handle if no other async is monitoring it.
    static Result posixUpdateFileWriteWatcher(AsyncEventLoop& eventLoop, FileDescriptor::Handle handle,
                                              decltype(AsyncRequest::flags)& flags)
    {

        if ((flags & Internal::Flag_WatcherSet) == 0)
        {
            return Result(true);
        }
        AsyncFileWrite* current = eventLoop.internal.activeFileWrites.front;
        while (current)
        {
            if (handle == current->handle && (current->flags & Internal::Flag_WatcherSet))
            {
                // Another async is monitoring the same handle, update the watcher to point to it
                return KernelQueuePosix::startSingleWatcherImmediate(eventLoop, current->handle, OUTPUT_EVENTS_MASK);
            }
            current = static_cast<AsyncFileWrite*>(current->next);
        }
        // No other async is monitoring this handle, we can stop the watcher
        flags &= ~Internal::Flag_WatcherSet;
        return KernelQueuePosix::stopSingleWatcherImmediate(eventLoop, handle, OUTPUT_EVENTS_MASK);
    }

    struct WriteApiPosixWrite
    {
        off_t offset = -1;

        explicit WriteApiPosixWrite(off_t offset) : offset(offset) {};

        ssize_t writeSingle(int fd, const char* data, size_t bytes, size_t totalBytesWritten)
        {
            if (offset <= 0)
            {
                return ::write(fd, data, bytes);
            }
            else
            {
                return ::pwrite(fd, data, bytes, offset + static_cast<off_t>(totalBytesWritten));
            }
        }

        ssize_t writeMultiple(int fd, struct iovec* vec, int remainingVectors, size_t totalBytesWritten)
        {
            if (offset <= 0)
            {
                return ::writev(fd, vec, remainingVectors);
            }
            else
            {
                return ::pwritev(fd, vec, remainingVectors, offset + static_cast<off_t>(totalBytesWritten));
            }
        }
    };

    struct WriteApiPosixSend
    {
        ssize_t writeSingle(int fd, const char* data, size_t bytes, size_t totalBytesWritten)
        {
            SC_COMPILER_UNUSED(totalBytesWritten);
            return ::send(fd, data, bytes, 0);
        }

        ssize_t writeMultiple(int fd, struct iovec* vec, int remainingVectors, size_t totalBytesWritten)
        {
            SC_COMPILER_UNUSED(totalBytesWritten);
            return ::writev(fd, vec, remainingVectors);
        }
    };

    struct WriteApiPosixSendTo
    {
        struct sockaddr* address;
        socklen_t        addressLen;
        WriteApiPosixSendTo(AsyncSocketSendTo& async)
        {
            address    = &async.address.handle.reinterpret_as<struct sockaddr>();
            addressLen = async.address.sizeOfHandle();
        }

        ssize_t writeSingle(int fd, const char* data, size_t bytes, size_t totalBytesWritten)
        {
            SC_COMPILER_UNUSED(totalBytesWritten);
            return ::sendto(fd, data, bytes, 0, address, addressLen);
        }

        ssize_t writeMultiple(int fd, struct iovec* vec, int remainingVectors, size_t totalBytesWritten)
        {
            SC_COMPILER_UNUSED(totalBytesWritten);
            msghdr msgs;
            memset(&msgs, 0, sizeof(msgs));
            msgs.msg_name    = address;
            msgs.msg_namelen = addressLen;
            msgs.msg_iov     = vec;
            msgs.msg_iovlen  = remainingVectors;
            return ::sendmsg(fd, &msgs, 1);
        }
    };

    template <typename T, typename WriteApi>
    [[nodiscard]] static bool posixTryWrite(T& async, size_t totalBytesToSend, WriteApi writeApi)
    {
        while (async.totalBytesWritten < totalBytesToSend)
        {
            ssize_t      numBytesSent   = 0;
            const size_t remainingBytes = totalBytesToSend - async.totalBytesWritten;
            if (async.singleBuffer)
            {
                numBytesSent = writeApi.writeSingle(async.handle, async.buffer.data() + async.totalBytesWritten,
                                                    remainingBytes, async.totalBytesWritten);
            }
            else
            {
                // Span has same underling representation as iovec (void*, size_t)
                static_assert(sizeof(iovec) == sizeof(Span<const char>), "assert");
                iovec*    ioVectors    = reinterpret_cast<iovec*>(async.buffers.data());
                const int numIoVectors = static_cast<int>(async.buffers.sizeInElements());

                // If coming from a previous partial write, find the iovec that was not fully written or
                // just compute the index to first iovec that has not yet been written at all.
                // Modify such iovec to the not-written-yet slice of the original and proceed to write
                // it together with all all io vecs that come after it. Restore the modified iovec (if any).
                size_t fullyWrittenBytes = 0; // Bytes of already fully written io vecs
                size_t indexOfVecToWrite = 0; // Index of first iovec that has not yet been written
                while (indexOfVecToWrite < async.buffers.sizeInElements())
                {
                    const size_t ioVecSize = async.buffers[indexOfVecToWrite].sizeInBytes();
                    if (fullyWrittenBytes + ioVecSize > async.totalBytesWritten)
                    {
                        break;
                    }
                    fullyWrittenBytes += ioVecSize;
                    indexOfVecToWrite++;
                }
                // Number of writes already written of io vector at indexOfVecToWrite
                const size_t partiallyWrittenBytes = async.totalBytesWritten - fullyWrittenBytes;
                const iovec  backup                = ioVectors[indexOfVecToWrite];
                if (partiallyWrittenBytes > 0)
                {
                    ioVectors[indexOfVecToWrite].iov_base =
                        static_cast<char*>(ioVectors[indexOfVecToWrite].iov_base) + partiallyWrittenBytes;
                    ioVectors[indexOfVecToWrite].iov_len -= partiallyWrittenBytes;
                }
                // Write everything from indexOfVecToWrite going forward
                const int remainingVectors = numIoVectors - static_cast<int>(indexOfVecToWrite);

                numBytesSent = writeApi.writeMultiple(async.handle, ioVectors + indexOfVecToWrite, remainingVectors,
                                                      async.totalBytesWritten);
                if (partiallyWrittenBytes > 0)
                {
                    ioVectors[indexOfVecToWrite] = backup;
                }
            }

            if (numBytesSent < 0)
            {
                return false;
            }
            else
            {
                async.totalBytesWritten += static_cast<size_t>(numBytesSent);
            }
        }
        return true;
    }

    template <typename T, typename WriteApi>
    Result posixWriteActivate(AsyncEventLoop& eventLoop, T& async, WriteApi writeApi, bool watchable)
    {
        const size_t totalBytesToSend = Internal::getSummedSizeOfBuffers(async);
        SC_ASSERT_RELEASE((async.flags & Internal::Flag_ManualCompletion) == 0);
        if (not posixTryWrite(async, totalBytesToSend, writeApi))
        {
            // Not all bytes have been written, so if descriptor supports watching
            // start monitoring it, otherwise just return error
            if (watchable)
            {
                async.flags |= Internal::Flag_WatcherSet;
                return Result(setEventWatcher(eventLoop, async, async.handle, OUTPUT_EVENTS_MASK));
            }
            return Result::Error("Error in posixTryWrite");
        }
        // Write has finished synchronously so force a manual invocation of its completion
        async.flags |= Internal::Flag_ManualCompletion;
        return Result(true);
    }

    template <typename T, typename WriteApi>
    static Result posixWriteCompleteAsync(typename T::Result& result, WriteApi writeApi)
    {
        T& async = result.getAsync();
        async.flags &= ~Internal::Flag_ManualCompletion;
        const size_t totalBytesToSend = Internal::getSummedSizeOfBuffers(async);
        if (not posixTryWrite(async, totalBytesToSend, writeApi))
        {
            const auto writeError = errno;
            if (writeError == EWOULDBLOCK || writeError == EAGAIN)
            {
                // Partial write case:
                // Not all bytes have been written, we need to skip user callback and reactivate this request
                // so that setEventWatcher(OUTPUT_EVENTS_MASK) will be called again
                result.shouldCallCallback = false;
                result.reactivateRequest(true);
                return Result(true);
            }
        }
        result.completionData.numBytes = async.totalBytesWritten;
        SC_TRY_MSG(result.completionData.numBytes == totalBytesToSend, "send didn't send all data");
        return Result(true);
    }

    template <typename T>
    static Result posixWriteManualActivateWithSameHandle(AsyncEventLoop& eventLoop, T& async, T* current)
    {
        // Activate all asyncs on the same socket descriptor too
        // TODO: This linear search is not great
        while (current)
        {
            if (current->handle == async.handle)
            {
                SC_ASSERT_RELEASE(current != &async);
                async.flags |= Internal::Flag_ManualCompletion;
                eventLoop.internal.manualCompletions.queueBack(*current);
            }
            current = static_cast<T*>(current->next);
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket SEND
    //-------------------------------------------------------------------------------------------------------

    static Result teardownAsync(AsyncSocketSend*, AsyncTeardown& teardown)
    {
        return posixUpdateSocketWriteWatcher(*teardown.eventLoop, teardown.socketHandle, teardown.flags);
    }

    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketSend& async)
    {
        return posixWriteActivate(eventLoop, async, WriteApiPosixSend{}, true);
    }

    Result cancelAsync(AsyncEventLoop& eventLoop, AsyncSocketSend& async)
    {
        return posixUpdateSocketWriteWatcher(eventLoop, async.handle, async.flags);
    }

    static Result completeAsync(AsyncSocketSend::Result& result)
    {
        AsyncSocketSend& async = result.getAsync();
        if (async.type == AsyncRequest::Type::SocketSendTo)
        {
            AsyncSocketSendTo& asyncSendTo = static_cast<AsyncSocketSendTo&>(async);
            SC_TRY(posixWriteCompleteAsync<AsyncSocketSend>(result, WriteApiPosixSendTo{asyncSendTo}));
        }
        else
        {
            SC_TRY(posixWriteCompleteAsync<AsyncSocketSend>(result, WriteApiPosixSend{}));
        }
        return posixWriteManualActivateWithSameHandle(result.eventLoop, async,
                                                      result.eventLoop.internal.activeSocketSends.front);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket SEND TO
    //-------------------------------------------------------------------------------------------------------
    static Result teardownAsync(AsyncSocketSendTo*, AsyncTeardown& teardown)
    {
        return posixUpdateSocketWriteWatcher(*teardown.eventLoop, teardown.socketHandle, teardown.flags);
    }

    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketSendTo& async)
    {
        return posixWriteActivate(eventLoop, async, WriteApiPosixSendTo{async}, true);
    }

    Result cancelAsync(AsyncEventLoop& eventLoop, AsyncSocketSendTo& async)
    {
        return posixUpdateSocketWriteWatcher(eventLoop, async.handle, async.flags);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket RECEIVE
    //-------------------------------------------------------------------------------------------------------
    Result setupAsync(AsyncEventLoop& eventLoop, AsyncSocketReceive& async)
    {
        return Result(setEventWatcher(eventLoop, async, async.handle, SOCKET_INPUT_EVENTS_MASK));
    }

    static Result teardownAsync(AsyncSocketReceive*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.socketHandle,
                                                            SOCKET_INPUT_EVENTS_MASK);
    }

    Result completeAsync(AsyncSocketReceive::Result& result)
    {
        ssize_t res;
        if (result.getAsync().type == AsyncRequest::Type::SocketReceiveFrom)
        {
            AsyncSocketReceiveFrom& async = static_cast<AsyncSocketReceiveFrom&>(result.getAsync());

            struct sockaddr* address    = &async.address.handle.reinterpret_as<struct sockaddr>();
            socklen_t        addressLen = async.address.sizeOfHandle();

            res = ::recvfrom(async.handle, async.buffer.data(), async.buffer.sizeInBytes(), 0, address, &addressLen);
        }
        else
        {
            AsyncSocketReceive& async = result.getAsync();

            res = ::recv(async.handle, async.buffer.data(), async.buffer.sizeInBytes(), 0);
        }
        SC_TRY_MSG(res >= 0, "error in recv");
        result.completionData.numBytes = static_cast<size_t>(res);

        if (res == 0)
        {
            result.completionData.disconnected = true;
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket RECEIVE FROM
    //-------------------------------------------------------------------------------------------------------
    Result setupAsync(AsyncEventLoop& eventLoop, AsyncSocketReceiveFrom& async)
    {
        return setupAsync(eventLoop, static_cast<AsyncSocketReceive&>(async));
    }

    static Result teardownAsync(AsyncSocketReceiveFrom* ptr, AsyncTeardown& teardown)
    {
        return teardownAsync(static_cast<AsyncSocketReceive*>(ptr), teardown);
    }

    //-------------------------------------------------------------------------------------------------------
    // File READ
    //-------------------------------------------------------------------------------------------------------
    Result setupAsync(AsyncEventLoop& eventLoop, AsyncFileRead& async)
    {
        bool canBeWatched;
        SC_TRY(isDescriptorReadWatchable(async.handle, canBeWatched));
        if (canBeWatched)
        {
            return setEventWatcher(eventLoop, async, async.handle, INPUT_EVENTS_MASK);
        }
        else
        {
            async.flags |= Internal::Flag_ManualCompletion; // on epoll regular files are not watchable
            return Result(true);
        }
    }

    Result completeAsync(AsyncFileRead::Result& result)
    {
#if SC_PLATFORM_LINUX
        if (result.eventIndex > 0)
        {
            epoll_event& event    = events[result.eventIndex];
            const bool   epollHUP = (event.events & EPOLLHUP) != 0;
            const bool   epollERR = (event.events & EPOLLERR) != 0;
            if (epollERR or epollHUP)
            {
                result.completionData.endOfFile = true; // epoll reports EOF on pipes
            }
        }
#endif
        return executeOperation(result.getAsync(), result.completionData);
    }

    static Result cancelAsync(AsyncEventLoop& eventLoop, AsyncFileRead& async)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(eventLoop, async.handle, INPUT_EVENTS_MASK);
    }

    static Result teardownAsync(AsyncFileRead*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.fileHandle,
                                                            INPUT_EVENTS_MASK);
    }

    static Result executeOperation(AsyncFileRead& async, AsyncFileRead::CompletionData& completionData)
    {
        auto    span = async.buffer;
        ssize_t res;
        do
        {
            if (async.useOffset)
            {
                res = ::pread(async.handle, span.data(), span.sizeInBytes(), static_cast<off_t>(async.offset));
            }
            else
            {
                res = ::read(async.handle, span.data(), span.sizeInBytes());
            }
        } while ((res == -1) and (errno == EINTR));

        SC_TRY_MSG(res >= 0, "::read failed");
        completionData.numBytes = static_cast<size_t>(res);
        if (not span.empty() and res == 0)
        {
            completionData.endOfFile = true;
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File WRITE
    //-------------------------------------------------------------------------------------------------------

    static Result setupAsync(AsyncEventLoop&, AsyncFileWrite& async)
    {
        return Result(isDescriptorWriteWatchable(async.handle, async.isWatchable));
    }

    static Result teardownAsync(AsyncFileWrite*, AsyncTeardown& teardown)
    {
        return posixUpdateFileWriteWatcher(*teardown.eventLoop, teardown.fileHandle, teardown.flags);
    }

    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileWrite& async)
    {
        const off_t offset = async.useOffset ? static_cast<off_t>(async.offset) : -1;
        return posixWriteActivate(eventLoop, async, WriteApiPosixWrite{offset}, async.isWatchable);
    }

    static Result cancelAsync(AsyncEventLoop& eventLoop, AsyncFileWrite& async)
    {
        return posixUpdateFileWriteWatcher(eventLoop, async.handle, async.flags);
    }

    static Result completeAsync(AsyncFileWrite::Result& result)
    {
        AsyncFileWrite& async  = result.getAsync();
        const off_t     offset = async.useOffset ? static_cast<off_t>(async.offset) : -1;
        SC_TRY(posixWriteCompleteAsync<AsyncFileWrite>(result, WriteApiPosixWrite{offset}));
        return posixWriteManualActivateWithSameHandle(result.eventLoop, async,
                                                      result.eventLoop.internal.activeFileWrites.front);
    }

    static Result executeOperation(AsyncFileWrite& async, AsyncFileWrite::CompletionData& completionData)
    {
        const size_t totalBytesToSend = Internal::getSummedSizeOfBuffers(async);
        const off_t  offset           = {async.useOffset ? static_cast<off_t>(async.offset) : -1};
        SC_TRY(posixTryWrite(async, totalBytesToSend, WriteApiPosixWrite{offset}));
        completionData.numBytes = async.totalBytesWritten;
        SC_TRY_MSG(completionData.numBytes == totalBytesToSend, "Partial write (disk full or RLIMIT_FSIZE reached)");
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File SEND (sendfile on Linux/macOS)
    //-------------------------------------------------------------------------------------------------------
    struct PosixSendFile
    {
        static ssize_t sendFile(int out_fd, int in_fd, off_t& offset, size_t count, bool& notImplemented)
        {
            if (count == 0)
                return 0;
#if SC_PLATFORM_LINUX
            notImplemented = false;
            ssize_t res;
            do
            {
                res = ::sendfile(out_fd, in_fd, &offset, count);
            } while (res == -1 && errno == EINTR);
            return res;
#elif SC_PLATFORM_APPLE
            notImplemented = false;
            off_t len      = static_cast<off_t>(count);
            int   res;
            do
            {
                // On macOS, the fourth argument is a value-result parameter:
                // - On entry, it specifies the number of bytes to send. (= 0 means send all)
                // - On return, it contains the number of bytes sent.
                //
                // The third argument is the offset
                res = ::sendfile(in_fd, out_fd, offset, &len, nullptr, 0);
            } while (res == -1 && errno == EINTR);

            if (res == 0)
            {
                offset += len;
                return static_cast<ssize_t>(len);
            }
            if (len > 0)
            {
                // If some bytes were sent but the call returned -1 (because of EAGAIN for example),
                // we should still consider it a "success" in terms of bytes transferred.
                // However, the standard behavior for sendfile on macOS returning -1 is setting errno.
                // So if we have partial write, we return partial write.
                offset += len;
                return static_cast<ssize_t>(len);
            }
            return -1;
#else
            notImplemented = true;
            return -1;
#endif
        }
    };

    static Result teardownAsync(AsyncFileSend*, AsyncTeardown& teardown)
    {
        return posixUpdateSocketWriteWatcher(*teardown.eventLoop, teardown.socketHandle, teardown.flags);
    }

    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileSend& async)
    {
        bool notImplemented = false;
        // on macOS and Linux we use sendfile
        ssize_t res =
            PosixSendFile::sendFile(async.socketHandle, async.fileHandle, async.offset, async.length, notImplemented);

        if (notImplemented)

        {
            return Result::Error("sendfile not implemented on this platform");
        }

        if (res >= 0)
        {
            async.bytesSent += static_cast<size_t>(res);
            // Check if we are done
            if (async.bytesSent == async.length)
            {
                async.flags |= Internal::Flag_ManualCompletion; // Ended synchronously
                return Result(true);
            }

            // If we are not done, it means we probably hit the socket buffer limit, so we treat it as blocking (EAGAIN)
        }
        else
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                return Result::Error("sendfile failed");
            }
        }

        // Needs to wait for socket to accept more data
        async.flags |= Internal::Flag_WatcherSet;
        SC_TRY(setEventWatcher(eventLoop, async, async.socketHandle, OUTPUT_EVENTS_MASK));
        return Result(true);
    }

    static Result cancelAsync(AsyncEventLoop& eventLoop, AsyncFileSend& async)
    {
        return posixUpdateSocketWriteWatcher(eventLoop, async.socketHandle, async.flags);
    }

    static Result completeAsync(AsyncFileSend::Result& result)
    {
        AsyncFileSend& async = result.getAsync();

        if (async.bytesSent < async.length)
        {
            bool    notImplemented;
            ssize_t res = PosixSendFile::sendFile(async.socketHandle, async.fileHandle, async.offset,
                                                  async.length - async.bytesSent, notImplemented);

            if (res >= 0)

            {
                async.bytesSent += static_cast<size_t>(res);
            }
            else
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    return Result::Error("sendfile failed");
                }
            }
        }

        if (async.bytesSent == async.length)
        {
            result.completionData.bytesTransferred = async.bytesSent;
            // We are done, we can remove the watcher from this socket (if no one else needs it)
            SC_TRY(posixUpdateSocketWriteWatcher(result.eventLoop, async.socketHandle, async.flags));
        }
        else
        {
            // Not done yet, keep watching
            result.shouldCallCallback = false;
            result.reactivateRequest(true);
        }

        return Result(true);
    }

    static Result executeOperation(AsyncFileSend& async, AsyncFileSend::CompletionData& completionData)
    {
        while (async.bytesSent < async.length)
        {
            bool    notImplemented;
            ssize_t res = PosixSendFile::sendFile(async.socketHandle, async.fileHandle, async.offset,
                                                  async.length - async.bytesSent, notImplemented);

            if (res >= 0)
            {
                async.bytesSent += static_cast<size_t>(res);
            }
            else
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // Wait for writeability
                    struct pollfd pfd;
                    pfd.fd      = async.socketHandle;
                    pfd.events  = POLLOUT;
                    pfd.revents = 0;
                    ::poll(&pfd, 1, -1); // Block indefinitely until writable
                    continue;
                }
                return Result::Error("sendfile failed");
            }
        }
        completionData.bytesTransferred = async.bytesSent;
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File POLL
    //-------------------------------------------------------------------------------------------------------
    Result setupAsync(AsyncEventLoop& eventLoop, AsyncFilePoll& async)
    {
        return setEventWatcher(eventLoop, async, async.handle, INPUT_EVENTS_MASK);
    }

    static Result teardownAsync(AsyncFilePoll*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.fileHandle,
                                                            INPUT_EVENTS_MASK);
    }

    static bool needsSubmissionWhenReactivating(AsyncFilePoll&) { return false; }

    //-------------------------------------------------------------------------------------------------------
    // Process EXIT
    //-------------------------------------------------------------------------------------------------------
    // Used by kevent backend when Process exits too fast (EV_ERROR / ESRCH) and by the io-uring backend
    static Result completeProcessExitWaitPid(AsyncProcessExit::Result& result)
    {
        int   status = -1;
        pid_t waitPid;
        do
        {
            waitPid = ::waitpid(result.getAsync().handle, &status, 0);
        } while (waitPid == -1 and errno == EINTR);
        if (waitPid == -1)
        {
            return Result::Error("waitPid");
        }
        if (WIFEXITED(status) != 0)
        {
            result.completionData.exitStatus = WEXITSTATUS(status);
        }
        return Result(true);
    }

#if SC_ASYNC_USE_EPOLL
    // On epoll AsyncProcessExit is handled inside KernelQueuePosix (using a signalfd).
#else

    Result setupAsync(AsyncEventLoop& eventLoop, AsyncProcessExit& async)
    {
        return setEventWatcher(eventLoop, async, async.handle, EVFILT_PROC, NOTE_EXIT | NOTE_EXITSTATUS);
    }

    static Result teardownAsync(AsyncProcessExit*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.processHandle, EVFILT_PROC);
    }

    Result completeAsync(AsyncProcessExit::Result& result)
    {
        SC_TRY_MSG(result.eventIndex >= 0, "Invalid event Index");
        const struct kevent event = events[result.eventIndex];
        // If process exits too early it can happen that we get EV_ERROR with ESRCH
        if ((event.flags & EV_ERROR) != 0 and (event.data == ESRCH))
        {
            // In this case we should just do a waitpid
            return completeProcessExitWaitPid(result);
        }
        else if ((event.fflags & (NOTE_EXIT | NOTE_EXITSTATUS)) > 0)
        {
            const uint32_t data = static_cast<uint32_t>(event.data);
            if (WIFEXITED(data) != 0)
            {
                result.completionData.exitStatus = WEXITSTATUS(data);
            }
            return Result(true);
        }
        return Result(false);
    }
#endif

    //-------------------------------------------------------------------------------------------------------
    // Templates
    //-------------------------------------------------------------------------------------------------------

    // clang-format off
    template <typename T> Result setupAsync(AsyncEventLoop&, T&)     { return Result(true); }
    template <typename T> Result activateAsync(AsyncEventLoop&, T&)  { return Result(true); }
    template <typename T> Result cancelAsync(AsyncEventLoop&, T&)    { return Result(true); }
    template <typename T> Result completeAsync(T&)  { return Result(true); }

    template <typename T> static Result teardownAsync(T*, AsyncTeardown&)  { return Result(true); }

    // If False, makes re-activation a no-op, that is a lightweight optimization.
    // More importantly it prevents an assert about being Submitting state when async completes during re-activation run cycle.
    template<typename T> static bool needsSubmissionWhenReactivating(T&)
    {
        return true;
    }
    
    template <typename T, typename P> static Result executeOperation(T&, P&) { return Result::Error("Implement executeOperation"); }
    // clang-format on
};
