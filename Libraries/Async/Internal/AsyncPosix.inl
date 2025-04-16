// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "AsyncInternal.h"

#include "../../Foundation/Deferred.h"

#if SC_ASYNC_USE_EPOLL

#include <errno.h>        // For error handling
#include <fcntl.h>        // For fcntl function (used for setting non-blocking mode)
#include <signal.h>       // For signal-related functions
#include <sys/epoll.h>    // For epoll functions
#include <sys/signalfd.h> // For signalfd functions
#include <sys/socket.h>   // For socket-related functions
#include <sys/stat.h>
#include <sys/uio.h> // writev, pwritev

#else

#include <errno.h>     // For error handling
#include <netdb.h>     // socketlen_t/getsocketopt/send/recv
#include <sys/event.h> // kqueue
#include <sys/stat.h>
#include <sys/time.h> // timespec
#include <sys/uio.h>  // writev, pwritev
#include <sys/wait.h> // WIFEXITED / WEXITSTATUS
#include <unistd.h>   // read/write/pread/pwrite
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

    [[nodiscard]] static constexpr bool makesSenseToRunInThreadPool(AsyncRequest&) { return true; }

    const KernelQueuePosix& getPosix() const { return *this; }

    [[nodiscard]] Result close()
    {
#if SC_ASYNC_USE_EPOLL
        SC_TRY(signalProcessExitDescriptor.close());
#endif
        SC_TRY(wakeupPipe.readPipe.close());
        SC_TRY(wakeupPipe.writePipe.close());
        return loopFd.close();
    }

    [[nodiscard]] Result createEventLoop(AsyncEventLoop::Options options = AsyncEventLoop::Options())
    {
        if (options.apiType == AsyncEventLoop::Options::ApiType::ForceUseIOURing)
        {
            return Result::Error("createEventLoop: Cannot use io_uring");
        }
#if SC_ASYNC_USE_EPOLL
        const int newQueue = ::epoll_create1(O_CLOEXEC);
#else
        const int     newQueue = ::kqueue();
#endif
        if (newQueue == -1)
        {
            // TODO: Better error handling
            return Result::Error("AsyncEventLoop::KernelQueuePosix::createEventLoop() failed");
        }
        SC_TRY(loopFd.assign(newQueue));
        return Result(true);
    }

    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop& eventLoop)
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

    [[nodiscard]] Result createWakeup(AsyncEventLoop& eventLoop)
    {
        // Create
        SC_TRY(wakeupPipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteNonInheritable));
        SC_TRY(wakeupPipe.readPipe.setBlocking(false));
        SC_TRY(wakeupPipe.writePipe.setBlocking(false));

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
                res = ::read(async.fileDescriptor, fakeBuffer, sizeof(fakeBuffer));
            } while (res < 0 and errno == EINTR);

            if (res >= 0 and (static_cast<size_t>(res) == sizeof(fakeBuffer)))
                continue;

            if (res != -1)
                break;

            if (errno == EWOULDBLOCK or errno == EAGAIN)
                break;
        }
        result.getAsync().eventLoop->internal.executeWakeUps();
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
    [[nodiscard]] Result createProcessSignalWatcher(AsyncEventLoop& loop)
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
        return signalProcessExit.start(loop, signalFd);
    }

    void onSIGCHLD(AsyncFilePoll::Result& result)
    {
        struct signalfd_siginfo siginfo;
        FileDescriptor::Handle  sigHandle;

        const KernelQueuePosix& kernelQueue = result.getAsync().eventLoop->internal.kernelQueue.get().getPosix();
        Result res = kernelQueue.signalProcessExitDescriptor.get(sigHandle, Result::Error("Invalid signal handle"));
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
            AsyncProcessExit* current = result.getAsync().eventLoop->internal.activeProcessExits.front;

            while (current)
            {
                if (pid == current->handle)
                {
                    AsyncProcessExit::Result processResult(*current, Result(true));
                    processResult.completionData.exitStatus.status = WEXITSTATUS(status);
                    result.getAsync().eventLoop->internal.removeActiveHandle(*current);
                    current->callback(processResult);
                    break;
                }
                current = static_cast<AsyncProcessExit*>(current->next);
            }
        }
    }

    [[nodiscard]] static Result setEventWatcher(AsyncRequest& async, int fileDescriptor, int32_t filter)
    {
        struct epoll_event event = {0};
        event.events             = filter;
        event.data.ptr           = &async; // data.ptr is a user data pointer
        FileDescriptor::Handle loopFd;
        SC_TRY(async.eventLoop->internal.kernelQueue.get().getPosix().loopFd.get(loopFd, Result::Error("loop")));

        int res = ::epoll_ctl(loopFd, EPOLL_CTL_ADD, fileDescriptor, &event);
        if (res == -1)
        {
            return Result::Error("epoll_ctl");
        }
        return Result(true);
    }

#endif

    [[nodiscard]] static Result stopSingleWatcherImmediate(AsyncEventLoop& eventLoop, SocketDescriptor::Handle handle,
                                                           int32_t filter)
    {
        FileDescriptor::Handle loopFd;
        SC_TRY(eventLoop.internal.kernelQueue.get().getPosix().loopFd.get(
            loopFd, Result::Error("AsyncEventLoop::KernelQueuePosix::syncWithKernel() - Invalid Handle")));
#if SC_ASYNC_USE_EPOLL
        struct epoll_event event;
        event.events   = filter;
        event.data.ptr = nullptr;
        const int res  = ::epoll_ctl(loopFd, EPOLL_CTL_DEL, handle, &event);
#else
        struct kevent kev;
        EV_SET(&kev, handle, filter, EV_DELETE, 0, 0, nullptr);
        const int res = ::kevent(loopFd, &kev, 1, 0, 0, nullptr);
#endif
        if (res == 0 or (errno == EBADF or errno == ENOENT))
        {
            return Result(true);
        }
        return Result::Error("stopSingleWatcherImmediate failed");
    }

    [[nodiscard]] static Result associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
    [[nodiscard]] static Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
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

    static constexpr int32_t INPUT_EVENTS_MASK  = EPOLLIN;
    static constexpr int32_t OUTPUT_EVENTS_MASK = EPOLLOUT;

    // In epoll (differently from kqueue) the watcher is immediately added
    [[nodiscard]] static Result setEventWatcher(AsyncRequest& async, int fileDescriptor, int32_t filter)
    {
        return KernelQueuePosix::setEventWatcher(async, fileDescriptor, filter);
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

    [[nodiscard]] Result validateEvent(uint32_t idx, bool& continueProcessing)
    {
        const epoll_event& event = events[idx];
        continueProcessing       = true;

        if ((event.events & EPOLLERR) != 0 || (event.events & EPOLLHUP) != 0)
        {
            continueProcessing = false;
            return Result::Error("Error in processing event (epoll EPOLLERR or EPOLLHUP)");
        }
        return Result(true);
    }

#else
    static constexpr short INPUT_EVENTS_MASK = EVFILT_READ;
    static constexpr short OUTPUT_EVENTS_MASK = EVFILT_WRITE;

    [[nodiscard]] Result setEventWatcher(AsyncRequest& async, int fileDescriptor, short filter,
                                         unsigned int options = 0)
    {
        EV_SET(events + newEvents, fileDescriptor, filter, EV_ADD | EV_ENABLE, options, 0, &async);
        newEvents += 1;
        if (newEvents >= totalNumEvents)
        {
            SC_TRY(flushQueue(*async.eventLoop));
        }
        return Result(true);
    }

    [[nodiscard]] Result flushQueue(AsyncEventLoop& eventLoop)
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

    [[nodiscard]] Result validateEvent(uint32_t idx, bool& continueProcessing)
    {
        const struct kevent& event = events[idx];
        continueProcessing = (event.flags & EV_DELETE) == 0;
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

    static struct timespec timerToRelativeTimespec(const Time::Absolute& loopTime, const Time::Absolute* nextTimer)
    {
        struct timespec specTimeout;
        if (nextTimer)
        {
            if (nextTimer->isLaterThanOrEqualTo(loopTime))
            {
                const Time::Milliseconds diff = nextTimer->subtractExact(loopTime);

                specTimeout.tv_sec  = diff.ms / 1000;
                specTimeout.tv_nsec = (diff.ms % 1000) * 1000 * 1000;
                return specTimeout;
            }
        }
        specTimeout.tv_sec  = 0;
        specTimeout.tv_nsec = 0;
        return specTimeout;
    }

    [[nodiscard]] Result syncWithKernel(AsyncEventLoop& eventLoop, Internal::SyncMode syncMode)
    {
        AsyncLoopTimeout*     loopTimeout = nullptr;
        const Time::Absolute* nextTimer   = nullptr;
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
            res = ::epoll_pwait2(loopFd, events, totalNumEvents, spec, 0);
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
    Result activateAsync(AsyncLoopTimeout& async)
    {
        async.expirationTime = async.eventLoop->getLoopTime().offsetBy(async.relativeTimeout);
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
    [[nodiscard]] Result setupAsync(AsyncSocketAccept& async)
    {
        return setEventWatcher(async, async.handle, INPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result teardownAsync(AsyncSocketAccept*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.socketHandle,
                                                            INPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketAccept::Result& result)
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
    [[nodiscard]] Result setupAsync(AsyncSocketConnect& async)
    {
        return setEventWatcher(async, async.handle, OUTPUT_EVENTS_MASK);
    }

    static Result teardownAsync(AsyncSocketConnect*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.socketHandle,
                                                            OUTPUT_EVENTS_MASK);
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
        AsyncSocketConnect& async = result.getAsync();

        int       errorCode;
        socklen_t errorSize = sizeof(errorCode);
        const int socketRes = ::getsockopt(async.handle, SOL_SOCKET, SO_ERROR, &errorCode, &errorSize);

        // TODO: This is making a syscall for each connected socket, we should probably aggregate them
        // And additionally it's stupid as probably WRITE will be subscribed again anyway
        // But probably this means to review the entire process of async stop
        AsyncEventLoop& eventLoop = *result.getAsync().eventLoop;
        SC_TRUST_RESULT(KernelQueuePosix::stopSingleWatcherImmediate(eventLoop, async.handle, OUTPUT_EVENTS_MASK));
        if (socketRes == 0)
        {
            SC_TRY_MSG(errorCode == 0, "connect SO_ERROR");
            return Result(true);
        }
        return Result::Error("connect getsockopt failed");
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket SEND
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncSocketSend& async)
    {
        return Result(setEventWatcher(async, async.handle, OUTPUT_EVENTS_MASK));
    }

    [[nodiscard]] static Result teardownAsync(AsyncSocketSend*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.socketHandle,
                                                            OUTPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketSend::Result& result)
    {
        AsyncSocketSend& async = result.getAsync();

        const size_t totalBytesToSend = Internal::getSummedSizeOfBuffers(async);
        while (async.totalBytesSent < totalBytesToSend)
        {
            ssize_t      numBytesSent   = 0;
            const size_t remainingBytes = totalBytesToSend - async.totalBytesSent;
            if (async.singleBuffer)
            {
                numBytesSent = ::write(async.handle, async.buffer.data() + async.totalBytesSent, remainingBytes);
            }
            else
            {
                // Span has same underling representation as iovec (void*, size_t)
                static_assert(sizeof(iovec) == sizeof(Span<const char>), "assert");
                iovec*    ioVectors    = reinterpret_cast<iovec*>(async.buffers.data());
                const int numIoVectors = static_cast<int>(async.buffers.sizeInElements());

                // If coming from a previous partial write, find the iovec that was not fully written or
                // just compute the index to first iovec that was not yet written at all.
                // Modify such iovec to the not-written-yet slice of the original and proceed to write
                // it together with all all iovecs that come after it. Restore the modified iovec (if any).
                size_t fullyWrittenBytes = 0; // Bytes of already fully written io vecs
                size_t indexOfVecToWrite = 0; // Index of first iovec that has not yet been written
                while (indexOfVecToWrite < async.buffers.sizeInElements())
                {
                    const size_t ioVecSize = async.buffers[indexOfVecToWrite].sizeInBytes();
                    if (fullyWrittenBytes + ioVecSize > async.totalBytesSent)
                    {
                        break;
                    }
                    fullyWrittenBytes += ioVecSize;
                }
                // Number of writes already written of io vector at indexOfVecToWrite
                const size_t partiallyWrittenBytes = async.totalBytesSent - fullyWrittenBytes;
                const iovec  backup                = ioVectors[indexOfVecToWrite];
                if (partiallyWrittenBytes > 0)
                {
                    ioVectors[indexOfVecToWrite].iov_base =
                        static_cast<char*>(ioVectors[indexOfVecToWrite].iov_base) + partiallyWrittenBytes;
                    ioVectors[indexOfVecToWrite].iov_len -= partiallyWrittenBytes;
                }
                // Write everything from indexOfVecToWrite going forward
                numBytesSent = ::writev(async.handle, ioVectors + indexOfVecToWrite,
                                        numIoVectors - static_cast<int>(indexOfVecToWrite));
                if (partiallyWrittenBytes > 0)
                {
                    ioVectors[indexOfVecToWrite] = backup;
                }
            }

            if (numBytesSent < 0)
            {
                const auto sendError = errno;
                if (sendError == EWOULDBLOCK || sendError == EAGAIN)
                {
                    // Partial write case:
                    // Socket is not writable right now, we should wait for it to be writable again, to finish
                    // writing the remaining part of the data.
                    // This would typically involve waiting for next EVFILT_WRITE or EPOLLOUT event again.
                    // To achieve that let's skip user callback and manually re-activate this request.
                    result.shouldCallCallback = false;
                    result.reactivateRequest(true);
                    return Result(true);
                }
                else
                {
                    break; // Error, cannot send all data
                }
            }
            else
            {
                async.totalBytesSent += static_cast<size_t>(numBytesSent);
            }
        }

        result.completionData.numBytes = async.totalBytesSent;
        SC_TRY_MSG(result.completionData.numBytes == totalBytesToSend, "send didn't send all data");
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket RECEIVE
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncSocketReceive& async)
    {
#if SC_ASYNC_USE_EPOLL
        return Result(setEventWatcher(async, async.handle, EPOLLIN | EPOLLRDHUP));
#else
        return Result(setEventWatcher(async, async.handle, EVFILT_READ));
#endif
    }

    [[nodiscard]] static Result teardownAsync(AsyncSocketReceive*, AsyncTeardown& teardown)
    {
#if SC_ASYNC_USE_EPOLL
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.socketHandle,
                                                            EPOLLIN | EPOLLRDHUP);
#else
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.socketHandle, EVFILT_READ);
#endif
    }

    [[nodiscard]] Result completeAsync(AsyncSocketReceive::Result& result)
    {
        AsyncSocketReceive& async = result.getAsync();
        const ssize_t       res   = ::recv(async.handle, async.buffer.data(), async.buffer.sizeInBytes(), 0);
        SC_TRY_MSG(res >= 0, "error in recv");
        result.completionData.numBytes = static_cast<size_t>(res);
        if (res == 0)
        {
            result.completionData.disconnected = true;
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket CLOSE
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncSocketClose& async)
    {
        // TODO: Allow running close on thread pool
        async.flags |= Internal::Flag_ManualCompletion;
        async.code = ::close(async.handle);
        SC_TRY_MSG(async.code == 0, "Close returned error");
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File READ
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncFileRead& async)
    {
        bool canBeWatched;
        SC_TRY(isDescriptorReadWatchable(async.fileDescriptor, canBeWatched));
        if (canBeWatched)
        {
            return setEventWatcher(async, async.fileDescriptor, INPUT_EVENTS_MASK);
        }
        else
        {
            async.flags |= Internal::Flag_ManualCompletion; // on epoll regular files are not watchable
            return Result(true);
        }
    }

    [[nodiscard]] Result completeAsync(AsyncFileRead::Result& result)
    {
        return executeOperation(result.getAsync(), result.completionData);
    }

    [[nodiscard]] static Result cancelAsync(AsyncFileRead& async)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*async.eventLoop, async.fileDescriptor, INPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result teardownAsync(AsyncFileRead*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.fileHandle,
                                                            INPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result executeOperation(AsyncFileRead& async, AsyncFileRead::CompletionData& completionData)
    {
        auto    span = async.buffer;
        ssize_t res;
        do
        {
            if (async.useOffset)
            {
                res = ::pread(async.fileDescriptor, span.data(), span.sizeInBytes(), static_cast<off_t>(async.offset));
            }
            else
            {
                res = ::read(async.fileDescriptor, span.data(), span.sizeInBytes());
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
    [[nodiscard]] Result setupAsync(AsyncFileWrite& async)
    {
        bool canBeWatched;
        SC_TRY(isDescriptorWriteWatchable(async.fileDescriptor, canBeWatched));
        if (canBeWatched)
        {
            return setEventWatcher(async, async.fileDescriptor, OUTPUT_EVENTS_MASK);
        }
        else
        {
            async.flags |= Internal::Flag_ManualCompletion; // on epoll regular files are not watchable
            return Result(true);
        }
    }

    [[nodiscard]] static Result completeAsync(AsyncFileWrite::Result& result)
    {
        return executeOperation(result.getAsync(), result.completionData);
    }

    [[nodiscard]] static Result cancelAsync(AsyncFileWrite& async)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*async.eventLoop, async.fileDescriptor, OUTPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result teardownAsync(AsyncFileWrite*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.fileHandle,
                                                            OUTPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result executeOperation(AsyncFileWrite& async, AsyncFileWrite::CompletionData& completionData)
    {
        auto    span = async.buffer;
        ssize_t res;
        do
        {
            if (async.useOffset)
            {
                res = ::pwrite(async.fileDescriptor, span.data(), span.sizeInBytes(), static_cast<off_t>(async.offset));
            }
            else
            {
                res = ::write(async.fileDescriptor, span.data(), span.sizeInBytes());
            }
        } while ((res == -1) and (errno == EINTR));
        SC_TRY_MSG(res >= 0, "::write failed");
        completionData.numBytes = static_cast<size_t>(res);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File POLL
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncFilePoll& async)
    {
        return setEventWatcher(async, async.fileDescriptor, INPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result teardownAsync(AsyncFilePoll*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.fileHandle,
                                                            INPUT_EVENTS_MASK);
    }

    static bool needsSubmissionWhenReactivating(AsyncFilePoll&) { return false; }

    static bool needsManualTimersProcessing() { return true; }

    //-------------------------------------------------------------------------------------------------------
    // File CLOSE
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncFileClose& async)
    {
        // TODO: Allow running close on thread pool
        async.flags |= Internal::Flag_ManualCompletion;
        async.code = ::close(async.fileDescriptor);
        SC_TRY_MSG(async.code == 0, "Close returned error");
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Process EXIT
    //-------------------------------------------------------------------------------------------------------
    // Used by kevent backend when Process exits too fast (EV_ERROR / ESRCH) and by the io-uring backend
    [[nodiscard]] static Result completeProcessExitWaitPid(AsyncProcessExit::Result& result)
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
            result.completionData.exitStatus.status = WEXITSTATUS(status);
        }
        return Result(true);
    }

#if SC_ASYNC_USE_EPOLL
    // On epoll AsyncProcessExit is handled inside KernelQueuePosix (using a signalfd).
#else

    [[nodiscard]] Result setupAsync(AsyncProcessExit& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_PROC, NOTE_EXIT | NOTE_EXITSTATUS);
    }

    [[nodiscard]] static Result teardownAsync(AsyncProcessExit*, AsyncTeardown& teardown)
    {
        return KernelQueuePosix::stopSingleWatcherImmediate(*teardown.eventLoop, teardown.processHandle, EVFILT_PROC);
    }

    [[nodiscard]] Result completeAsync(AsyncProcessExit::Result& result)
    {
        SC_TRY_MSG(result.getAsync().eventIndex >= 0, "Invalid event Index");
        const struct kevent event = events[result.getAsync().eventIndex];
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
                result.completionData.exitStatus.status = WEXITSTATUS(data);
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
    template <typename T> [[nodiscard]] Result setupAsync(T&)     { return Result(true); }
    template <typename T> [[nodiscard]] Result activateAsync(T&)  { return Result(true); }
    template <typename T> [[nodiscard]] Result completeAsync(T&)  { return Result(true); }
    template <typename T> [[nodiscard]] Result cancelAsync(T&)    { return Result(true); }

    template <typename T> [[nodiscard]] static Result teardownAsync(T*, AsyncTeardown&)  { return Result(true); }

    // If False, makes re-activation a no-op, that is a lightweight optimization.
    // More importantly it prevents an assert about being Submitting state when async completes during re-activation run cycle.
    template<typename T> static bool needsSubmissionWhenReactivating(T&)
    {
        return true;
    }
    
    template <typename T, typename P> [[nodiscard]] static Result executeOperation(T&, P&) { return Result::Error("Implement executeOperation"); }
    // clang-format on
};
