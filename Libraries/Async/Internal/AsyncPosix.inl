// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "AsyncPrivate.h"

#include "../../Foundation/Deferred.h"

#if SC_ASYNC_USE_EPOLL

#include <errno.h>        // For error handling
#include <fcntl.h>        // For fcntl function (used for setting non-blocking mode)
#include <signal.h>       // For signal-related functions
#include <sys/epoll.h>    // For epoll functions
#include <sys/signalfd.h> // For signalfd functions
#include <sys/socket.h>   // For socket-related functions
#include <sys/stat.h>

#else

#include <errno.h>     // For error handling
#include <netdb.h>     // socketlen_t/getsocketopt/send/recv
#include <sys/event.h> // kqueue
#include <sys/time.h>  // timespec
#include <sys/wait.h>  // WIFEXITED / WEXITSTATUS
#include <unistd.h>    // read/write/pread/pwrite

#endif

struct SC::AsyncEventLoop::InternalPosix
{
    FileDescriptor loopFd;

    AsyncFilePoll  wakeupPoll;
    PipeDescriptor wakeupPipe;
#if SC_ASYNC_USE_EPOLL
    FileDescriptor signalProcessExitDescriptor;
    AsyncFilePoll  signalProcessExit;
#endif
    InternalPosix() {}
    ~InternalPosix() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] static constexpr bool makesSenseToRunInThreadPool(AsyncRequest&) { return true; }

    const InternalPosix& getPosix() const { return *this; }

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
            return Result::Error("AsyncEventLoop::InternalPosix::createEventLoop() failed");
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
        // Calls to decreaseActiveCount must be after runNoWait()
        eventLoop.privateSelf.decreaseActiveCount(); // WakeUp (poll) doesn't keep the queue active
#if SC_ASYNC_USE_EPOLL
        eventLoop.privateSelf.decreaseActiveCount(); // Process watcher doesn't keep the queue active
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
            Result::Error("AsyncEventLoop::InternalPosix::createSharedWatchers() - AsyncRequest read handle invalid")));
        wakeupPoll.callback.bind<&InternalPosix::completeWakeUp>();
        wakeupPoll.setDebugName("SharedWakeUpPoll");
        SC_TRY(wakeupPoll.start(eventLoop, wakeUpPipeDescriptor));
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
        result.getAsync().eventLoop->privateSelf.executeWakeUps(result);
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
        signalProcessExit.callback.bind<InternalPosix, &InternalPosix::onSIGCHLD>(*this);
        return signalProcessExit.start(loop, signalFd);
    }

    void onSIGCHLD(AsyncFilePoll::Result& result)
    {
        struct signalfd_siginfo siginfo;
        FileDescriptor::Handle  sigHandle;

        const InternalPosix& internal = result.getAsync().eventLoop->internalSelf.getPosix();
        (void)(internal.signalProcessExitDescriptor.get(sigHandle, Result::Error("Invalid signal handle")));
        ssize_t size = ::read(sigHandle, &siginfo, sizeof(siginfo));

        if (size == sizeof(siginfo))
        {
            // Check if the received signal is related to process exit
            if (siginfo.ssi_signo == SIGCHLD)
            {
                // Loop all process handles to find if one of our interest has exited
                AsyncProcessExit* current = result.getAsync().eventLoop->privateSelf.activeProcessExits.front;

                while (current)
                {
                    if (siginfo.ssi_pid == current->handle)
                    {
                        AsyncProcessExit::Result processResult(*current, Result(true));
                        processResult.completionData.exitStatus.status = siginfo.ssi_status;
                        result.getAsync().eventLoop->privateSelf.removeActiveHandle(*current);
                        current->callback(processResult);
                        // TODO: Handle lazy deactivation for signals when no more processes exist
                        result.reactivateRequest(true);
                        break;
                    }
                    current = static_cast<AsyncProcessExit*>(current->next);
                }
            }
        }
    }

    [[nodiscard]] static Result setEventWatcher(AsyncRequest& async, int fileDescriptor, int32_t filter)
    {
        struct epoll_event event = {0};
        event.events             = filter;
        event.data.ptr           = &async; // data.ptr is a user data pointer
        FileDescriptor::Handle loopFd;
        SC_TRY(async.eventLoop->internalSelf.getPosix().loopFd.get(loopFd, Result::Error("loop")));

        int res = ::epoll_ctl(loopFd, EPOLL_CTL_ADD, fileDescriptor, &event);
        if (res == -1)
        {
            return Result::Error("epoll_ctl");
        }
        return Result(true);
    }

#endif

    [[nodiscard]] static Result stopSingleWatcherImmediate(AsyncRequest& async, SocketDescriptor::Handle handle,
                                                           int32_t filter)
    {
        FileDescriptor::Handle loopFd;
        SC_TRY(async.eventLoop->internalSelf.getPosix().loopFd.get(
            loopFd, Result::Error("AsyncEventLoop::InternalPosix::syncWithKernel() - Invalid Handle")));
#if SC_ASYNC_USE_EPOLL
        struct epoll_event event;
        event.events   = filter;
        event.data.ptr = &async;
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

struct SC::AsyncEventLoop::KernelQueuePosix
{
  private:
    static constexpr int totalNumEvents = 1024;

#if SC_ASYNC_USE_EPOLL
    epoll_event events[totalNumEvents];
#else
    struct kevent events[totalNumEvents];
#endif
    int newEvents = 0;

    KernelQueue& parentKernelQueue;

  public:
    KernelQueuePosix(KernelQueue& kq) : parentKernelQueue(kq) { memset(events, 0, sizeof(events)); }
#if SC_PLATFORM_APPLE
    KernelQueuePosix(InternalPosix&) : parentKernelQueue(*this) { memset(events, 0, sizeof(events)); }
#endif
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
        return InternalPosix::setEventWatcher(async, fileDescriptor, filter);
    }

    [[nodiscard]] static bool isDescriptorWatchable(int fd, bool& canBeWatched)
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
        SC_TRY(eventLoop.internalSelf.loopFd.get(loopFd, Result::Error("flushQueue() - Invalid Handle")));

        int res;
        do
        {
            res = ::kevent(loopFd, events, newEvents, nullptr, 0, nullptr);
        } while (res == -1 && errno == EINTR);
        if (res != 0)
        {
            return Result::Error("AsyncEventLoop::InternalPosix::flushQueue() - kevent failed");
        }
        newEvents = 0;
        return Result(true);
    }

    [[nodiscard]] static constexpr bool isDescriptorWatchable(int, bool& canBeWatched)
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
            return Result::Error("Error in processing event (kqueue EV_ERROR)");
        }
        return Result(true);
    }
#endif

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

    [[nodiscard]] Result syncWithKernel(AsyncEventLoop& eventLoop, Private::SyncMode syncMode)
    {
        const Time::HighResolutionCounter* nextTimer =
            syncMode == Private::SyncMode::ForcedForwardProgress ? eventLoop.privateSelf.findEarliestTimer() : nullptr;
        FileDescriptor::Handle loopFd;
        SC_TRY(
            eventLoop.internalSelf.getPosix().loopFd.get(loopFd, Result::Error("syncWithKernel() - Invalid Handle")));

        struct timespec specTimeout;
        // when nextTimer is null, specTimeout is initialized to 0, so that SyncMode::NoWait
        specTimeout = timerToTimespec(eventLoop.privateSelf.loopTime, nextTimer);
        int res;
        do
        {
            auto spec = nextTimer or syncMode == Private::SyncMode::NoWait ? &specTimeout : nullptr;
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
                    eventLoop.privateSelf.updateTime();
                    specTimeout = timerToTimespec(eventLoop.privateSelf.loopTime, nextTimer);
                }
                continue;
            }
            break;
        } while (true);
        if (res == -1)
        {
            return Result::Error("AsyncEventLoop::InternalPosix::poll() - failed");
        }
        newEvents = static_cast<int>(res);
        if (nextTimer)
        {
            eventLoop.privateSelf.executeTimers(parentKernelQueue, *nextTimer);
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // TIMEOUT
    //-------------------------------------------------------------------------------------------------------
    // Nothing to do :)

    //-------------------------------------------------------------------------------------------------------
    // WAKEUP
    //-------------------------------------------------------------------------------------------------------
    // Nothing to do :)

    //-------------------------------------------------------------------------------------------------------
    // Socket ACCEPT
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncSocketAccept& async)
    {
        return setEventWatcher(async, async.handle, INPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result teardownAsync(AsyncSocketAccept& async)
    {
        return InternalPosix::stopSingleWatcherImmediate(async, async.handle, INPUT_EVENTS_MASK);
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

    static Result teardownAsync(AsyncSocketConnect& async)
    {
        return InternalPosix::stopSingleWatcherImmediate(async, async.handle, OUTPUT_EVENTS_MASK);
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
        SC_TRUST_RESULT(InternalPosix::stopSingleWatcherImmediate(result.getAsync(), async.handle, OUTPUT_EVENTS_MASK));
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

    [[nodiscard]] static Result teardownAsync(AsyncSocketSend& async)
    {
        return InternalPosix::stopSingleWatcherImmediate(async, async.handle, OUTPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketSend::Result& result)
    {
        AsyncSocketSend& async = result.getAsync();
        const ssize_t    res   = ::send(async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        SC_TRY_MSG(res >= 0, "error in send");
        result.completionData.writtenBytes = static_cast<size_t>(res);
        SC_TRY_MSG(result.completionData.writtenBytes == async.data.sizeInBytes(), "send didn't send all data");
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

    [[nodiscard]] static Result teardownAsync(AsyncSocketReceive& async)
    {
#if SC_ASYNC_USE_EPOLL
        return InternalPosix::stopSingleWatcherImmediate(async, async.handle, EPOLLIN | EPOLLRDHUP);
#else
        return InternalPosix::stopSingleWatcherImmediate(async, async.handle, EVFILT_READ);
#endif
    }

    [[nodiscard]] static Result completeAsync(AsyncSocketReceive::Result& result)
    {
        AsyncSocketReceive& async = result.getAsync();
        const ssize_t       res   = ::recv(async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        SC_TRY_MSG(res >= 0, "error in recv");
        result.completionData.readBytes = static_cast<size_t>(res);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket CLOSE
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncSocketClose& async)
    {
        // TODO: Allow running close on thread pool
        async.flags |= AsyncRequest::Flag_ManualCompletion;
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
        SC_TRY(isDescriptorWatchable(async.fileDescriptor, canBeWatched));
        if (canBeWatched)
        {
            return setEventWatcher(async, async.fileDescriptor, INPUT_EVENTS_MASK);
        }
        else
        {
            async.flags |= AsyncRequest::Flag_ManualCompletion; // on epoll regular files are not watchable
            return Result(true);
        }
    }

    [[nodiscard]] static Result completeAsync(AsyncFileRead::Result& result)
    {
        return executeOperation(result.getAsync(), result.completionData);
    }

    [[nodiscard]] static Result cancelAsync(AsyncFileRead& async)
    {
        return InternalPosix::stopSingleWatcherImmediate(async, async.fileDescriptor, INPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result executeOperation(AsyncFileRead& async, AsyncFileRead::CompletionData& completionData)
    {
        auto    span = async.readBuffer;
        ssize_t res;
        do
        {
            if (async.offset == 0)
            {
                res = ::read(async.fileDescriptor, span.data(), span.sizeInBytes());
            }
            else
            {
                res = ::pread(async.fileDescriptor, span.data(), span.sizeInBytes(), static_cast<off_t>(async.offset));
            }
        } while ((res == -1) and (errno == EINTR));

        SC_TRY_MSG(res >= 0, "::read failed");
        completionData.readBytes = static_cast<size_t>(res);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File WRITE
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncFileWrite& async)
    {
        bool canBeWatched;
        SC_TRY(isDescriptorWatchable(async.fileDescriptor, canBeWatched));
        if (canBeWatched)
        {
            return setEventWatcher(async, async.fileDescriptor, OUTPUT_EVENTS_MASK);
        }
        else
        {
            async.flags |= AsyncRequest::Flag_ManualCompletion; // on epoll regular files are not watchable
            return Result(true);
        }
    }

    [[nodiscard]] static Result completeAsync(AsyncFileWrite::Result& result)
    {
        return executeOperation(result.getAsync(), result.completionData);
    }

    [[nodiscard]] static Result cancelAsync(AsyncFileWrite& async)
    {
        return InternalPosix::stopSingleWatcherImmediate(async, async.fileDescriptor, OUTPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result executeOperation(AsyncFileWrite& async, AsyncFileWrite::CompletionData& completionData)
    {
        auto    span = async.writeBuffer;
        ssize_t res;
        do
        {
            if (async.offset == 0)
            {
                res = ::write(async.fileDescriptor, span.data(), span.sizeInBytes());
            }
            else
            {
                res = ::pwrite(async.fileDescriptor, span.data(), span.sizeInBytes(), static_cast<off_t>(async.offset));
            }
        } while ((res == -1) and (errno == EINTR));
        SC_TRY_MSG(res >= 0, "::write failed");
        completionData.writtenBytes = static_cast<size_t>(res);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File POLL
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncFilePoll& async)
    {
        return setEventWatcher(async, async.fileDescriptor, INPUT_EVENTS_MASK);
    }

    [[nodiscard]] static Result teardownAsync(AsyncFilePoll& async)
    {
        return InternalPosix::stopSingleWatcherImmediate(async, async.fileDescriptor, INPUT_EVENTS_MASK);
    }

    //-------------------------------------------------------------------------------------------------------
    // File CLOSE
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result setupAsync(AsyncFileClose& async)
    {
        // TODO: Allow running close on thread pool
        async.flags |= AsyncRequest::Flag_ManualCompletion;
        async.code = ::close(async.fileDescriptor);
        SC_TRY_MSG(async.code == 0, "Close returned error");
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Process EXIT
    //-------------------------------------------------------------------------------------------------------
#if SC_ASYNC_USE_EPOLL
    // On epoll AsyncProcessExit is handled inside InternalPosix (using a signalfd).
#else

    [[nodiscard]] Result setupAsync(AsyncProcessExit& async)
    {
        return setEventWatcher(async, async.handle, EVFILT_PROC, NOTE_EXIT | NOTE_EXITSTATUS);
    }

    [[nodiscard]] static Result teardownAsync(AsyncProcessExit& async)
    {
        return InternalPosix::stopSingleWatcherImmediate(async, async.handle, EVFILT_PROC);
    }

    [[nodiscard]] Result completeAsync(AsyncProcessExit::Result& result)
    {
        SC_TRY_MSG(result.getAsync().eventIndex >= 0, "Invalid event Index");
        const struct kevent event = events[result.getAsync().eventIndex];
        if ((event.fflags & (NOTE_EXIT | NOTE_EXITSTATUS)) > 0)
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
    template <typename T> [[nodiscard]] Result teardownAsync(T&)  { return Result(true); }
    template <typename T> [[nodiscard]] Result activateAsync(T&)  { return Result(true); }
    template <typename T> [[nodiscard]] Result completeAsync(T&)  { return Result(true); }
    template <typename T> [[nodiscard]] Result cancelAsync(T&)    { return Result(true); }
    // clang-format on
};
