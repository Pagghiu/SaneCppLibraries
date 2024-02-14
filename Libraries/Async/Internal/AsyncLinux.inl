// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"

#include <arpa/inet.h>   // sockaddr_in
#include <stdint.h>      // uint32_t
#include <sys/eventfd.h> // eventfd
#include <sys/poll.h>    // POLLIN
#include <sys/syscall.h> // SYS_pidfd_open
#include <sys/wait.h>    // waitpid

struct SC::AsyncEventLoop::Internal
{
    AlignedStorage<304> storage;
    bool                isEpoll = true;

    Internal();
    ~Internal();
    InternalIoURing& getUring();
    InternalPosix&   getPosix();

    [[nodiscard]] Result close();
    [[nodiscard]] Result createEventLoop(AsyncEventLoop::Options options);
    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop&);
    [[nodiscard]] Result wakeUpFromExternalThread();
    [[nodiscard]] Result associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
    [[nodiscard]] Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
};

struct SC::AsyncEventLoop::KernelQueue
{
    bool                  isEpoll = true;
    AlignedStorage<16400> storage;

    KernelQueue(Internal& internal);
    ~KernelQueue();

    KernelQueueIoURing&       getUring();
    KernelQueuePosix&         getPosix();
    const KernelQueueIoURing& getUring() const;
    const KernelQueuePosix&   getPosix() const;

    [[nodiscard]] uint32_t getNumEvents() const;
    [[nodiscard]] Result   syncWithKernel(AsyncEventLoop&, SyncMode);
    [[nodiscard]] Result   validateEvent(uint32_t&, bool&);

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t);

    // clang-format off
    template <typename T> [[nodiscard]] Result setupAsync(T&);
    template <typename T> [[nodiscard]] Result teardownAsync(T&);
    template <typename T> [[nodiscard]] Result activateAsync(T&);
    template <typename T> [[nodiscard]] Result completeAsync(T&);
    template <typename T> [[nodiscard]] Result cancelAsync(T&);
    // clang-format on
};

#define SC_ASYNC_USE_EPOLL 1 // uses epoll
#include "AsyncPosix.inl"

#include "AsyncLinuxAPI.h"

// TODO: Protect it with a mutex or force passing it during creation
static AsyncLinuxLibURingLoader globalLibURing;

struct SC::AsyncEventLoop::InternalIoURing
{
    static constexpr int QueueDepth = 64;

    bool     ringInited = false;
    io_uring ring;

    AsyncFilePoll  wakeUpPoll;
    FileDescriptor wakeUpEventFd;

    InternalIoURing() { memset(&ring, sizeof(ring), 0); }

    ~InternalIoURing() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] Result close()
    {
        SC_TRY(wakeUpEventFd.close());
        if (ringInited)
        {
            ringInited = false;
            globalLibURing.io_uring_queue_exit(&ring);
        }
        return Result(true);
    }

    [[nodiscard]] Result createEventLoop()
    {
        if (not globalLibURing.init())
        {
            return Result::Error(
                "Cannot load liburing.so. Run \"sudo apt install liburing-dev\" or equivalent for your distro.");
        }
        if (ringInited)
        {
            return Result::Error("ring already inited");
        }
        const auto uringFd = globalLibURing.io_uring_queue_init(QueueDepth, &ring, 0);
        if (uringFd < 0)
        {
            return Result::Error("io_uring_setup failed");
        }
        ringInited = true;
        return Result(true);
    }

    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop& eventLoop)
    {
        SC_TRY(createWakeup(eventLoop));
        SC_TRY(eventLoop.runNoWait());   // Register the read handle before everything else
        eventLoop.decreaseActiveCount(); // Avoids wakeup (read) keeping the queue up. Must be after runNoWait().
        return Result(true);
    }

    [[nodiscard]] Result createWakeup(AsyncEventLoop& eventLoop)
    {
        // Create the non-blocking event file descriptor
        FileDescriptor::Handle newEventFd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (newEventFd < 0)
        {
            return Result::Error("eventfd");
        }
        SC_TRY(wakeUpEventFd.assign(newEventFd));

        // Register
        wakeUpPoll.callback.bind<&InternalIoURing::completeWakeUp>();
        SC_TRY(wakeUpPoll.start(eventLoop, newEventFd));
        return Result(true);
    }

    Result wakeUpFromExternalThread()
    {
        int eventFd;
        SC_TRY(wakeUpEventFd.get(eventFd, Result::Error("writePipe handle")));
        ssize_t eventValue;
        do
        {
            eventValue = ::eventfd_write(eventFd, 1);
        } while (eventValue == -1 && errno == EINTR);

        if (eventValue < 0)
        {
            return Result::Error("AsyncEventLoop::wakeUpFromExternalThread - Error in write");
        }
        return Result(true);
    }

    static void completeWakeUp(AsyncFilePoll::Result& result)
    {
        result.async.eventLoop->executeWakeUps(result);
        result.reactivateRequest(true);
    }

    static Result associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
    static Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
};

struct SC::AsyncEventLoop::KernelQueueIoURing
{
  private:
    static constexpr int totalNumEvents = 256;

    int          newEvents = 0;
    io_uring_cqe events[totalNumEvents];

    KernelQueue& parentKernelQueue;

  public:
    KernelQueueIoURing(KernelQueue& kq) : parentKernelQueue(kq) {}

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t idx)
    {
        io_uring_cqe& completion = events[idx];
        return reinterpret_cast<AsyncRequest*>(globalLibURing.io_uring_cqe_get_data(&completion));
    }

    uint32_t getNumEvents() const { return static_cast<uint32_t>(newEvents); }

    static io_uring& getRing(AsyncEventLoop& eventLoop) { return eventLoop.internal.get().getUring().ring; }

    [[nodiscard]] Result getNewSubmission(AsyncRequest& async, io_uring_sqe*& newSubmission)
    {
        io_uring& ring = getRing(*async.eventLoop);
        // Request a new submission slot
        io_uring_sqe* kernelSubmission = globalLibURing.io_uring_get_sqe(&ring);
        if (kernelSubmission == nullptr)
        {
            // No space in the submission queue, let's try to flush submissions and try again
            SC_TRY(flushSubmissions(*async.eventLoop, SyncMode::NoWait));
            kernelSubmission = globalLibURing.io_uring_get_sqe(&ring);
            if (kernelSubmission == nullptr)
            {
                // Not much we can do at this point, we can't really submit
                return Result::Error("io_uring_get_sqe");
            }
        }
        newSubmission = kernelSubmission;
        return Result(true);
    }

    void copyReadyCompletions(io_uring& ring)
    {
        // Read up to totalNumEvents completions, copy them into a local array and
        // advance the ring buffer pointers to free ring slots.
        io_uring_cqe* eventPointers[totalNumEvents];
        newEvents = globalLibURing.io_uring_peek_batch_cqe(&ring, &eventPointers[0], totalNumEvents);
        for (int idx = 0; idx < newEvents; ++idx)
        {
            events[idx] = *eventPointers[idx];
        }
        globalLibURing.io_uring_cq_advance(&ring, newEvents);
    }

    [[nodiscard]] Result syncWithKernel(AsyncEventLoop& eventLoop, SyncMode syncMode)
    {
        SC_TRY(flushSubmissions(eventLoop, syncMode));
        copyReadyCompletions(getRing(eventLoop));
        return Result(true);
    }

    [[nodiscard]] Result flushSubmissions(AsyncEventLoop& eventLoop, SyncMode syncMode)
    {
        io_uring& ring = getRing(eventLoop);
        while (true)
        {
            int res = -1;
            switch (syncMode)
            {
            case SyncMode::NoWait: {
                res = globalLibURing.io_uring_submit(&ring);
                break;
            }
            case SyncMode::ForcedForwardProgress: {
                res = globalLibURing.io_uring_submit_and_wait(&ring, 1);
                break;
            }
            }
            if (res < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                if (errno == EAGAIN or errno == EBUSY)
                {
                    // OMG the completion queue is full, so we can't submit
                    // anything until we free some of the completions slots :-|
                    copyReadyCompletions(ring);
                    if (newEvents > 0)
                    {
                        // We've freed some slots, let's try again
                        eventLoop.runStepExecuteCompletions(parentKernelQueue);
                        continue;
                    }
                    else
                    {
                        return Result::Error("io_uring_submit EAGAIN / EBUSY");
                    }
                }
                else
                {
                    return Result::Error("io_uring_submit");
                }
            }

            break;
        }

        return Result(true);
    }

    [[nodiscard]] Result validateEvent(uint32_t idx, bool& continueProcessing)
    {
        io_uring_cqe& completion = events[idx];
        // Cancellation completions have nullptr user_data
        continueProcessing = completion.user_data != 0;
        if (continueProcessing and completion.res < 0)
        {
            const AsyncRequest* request = getAsyncRequest(idx);
            // Expired LoopTimeout are reported with ETIME errno, but we do not consider it an error...
            if (request->type != AsyncRequest::Type::LoopTimeout or completion.res != -ETIME)
            {
                continueProcessing = false;
                return Result::Error("Error in processing event");
            }
        }
        return Result(true);
    }

    // TIMEOUT
    [[nodiscard]] Result activateAsync(AsyncLoopTimeout& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));

        // The issue with io_uring_prep_timeout is that the timespec passed with the offset must be valid
        // for the entire duration of the enter ring syscall.
        // This means that we either place it somewhere in the AsyncLoopTimeout or we take advantage of the
        // fact that the binary layout of Time::HighResolutionCounter is the same as __kernel_timespec.
        // Choosing the latter solution but with a few asserts that shield us from any eventual changes
        // in HighResolutionCounter binary layouts.
        static_assert(sizeof(__kernel_timespec::tv_sec) == sizeof(Time::HighResolutionCounter::part1), "");
        static_assert(sizeof(__kernel_timespec::tv_nsec) == sizeof(Time::HighResolutionCounter::part1), "");
        static_assert(__builtin_offsetof(__kernel_timespec, tv_sec) ==
                          __builtin_offsetof(Time::HighResolutionCounter, part1),
                      "Time::HighResolutionCounter layout changed!");
        static_assert(__builtin_offsetof(__kernel_timespec, tv_nsec) ==
                          __builtin_offsetof(Time::HighResolutionCounter, part2),
                      "Time::HighResolutionCounter layout changed!");
        struct __kernel_timespec* ts = reinterpret_cast<struct __kernel_timespec*>(&async.expirationTime);
        globalLibURing.io_uring_prep_timeout(submission, ts, 0, IORING_TIMEOUT_ABS);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result cancelAsync(AsyncLoopTimeout& async)
    {
        // Note: Expired timeouts are reported with ETIME error (see validateEvent)
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_timeout_remove(submission, reinterpret_cast<__u64>(&async), 0);
        return Result(true);
    }

    // WAKEUP
    [[nodiscard]] static Result setupAsync(AsyncLoopWakeUp& async)
    {
        async.eventLoop->activeWakeUps.queueBack(async);
        return Result(true);
    }

    [[nodiscard]] static Result teardownAsync(AsyncLoopWakeUp& async)
    {
        async.eventLoop->activeWakeUps.remove(async);
        return Result(true);
    }

    // Socket ACCEPT
    [[nodiscard]] Result activateAsync(AsyncSocketAccept& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        struct sockaddr* sockAddr = &async.sockAddrHandle.reinterpret_as<struct sockaddr>();
        async.sockAddrLen         = sizeof(struct sockaddr);
        globalLibURing.io_uring_prep_accept(submission, async.handle, sockAddr, &async.sockAddrLen, SOCK_CLOEXEC);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncSocketAccept::Result& res)
    {
        return res.acceptedClient.assign(events[res.async.eventIndex].res);
    }

    // Socket CONNECT
    [[nodiscard]] Result activateAsync(AsyncSocketConnect& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        struct sockaddr* sockAddr = &async.ipAddress.handle.reinterpret_as<struct sockaddr>();
        globalLibURing.io_uring_prep_connect(submission, async.handle, sockAddr, async.ipAddress.sizeOfHandle());
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncSocketConnect::Result& res)
    {
        res.returnCode = Result(true);
        return Result(true);
    }

    // Socket SEND
    [[nodiscard]] Result activateAsync(AsyncSocketSend& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_send(submission, async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncSocketSend::Result& result)
    {
        const size_t numBytes = static_cast<size_t>(events[result.async.eventIndex].res);
        return Result(numBytes == result.async.data.sizeInBytes());
    }

    // Socket RECEIVE
    [[nodiscard]] Result activateAsync(AsyncSocketReceive& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_recv(submission, async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncSocketReceive::Result& result)
    {
        const size_t numBytes = static_cast<size_t>(events[result.async.eventIndex].res);
        return Result(result.async.data.sliceStartLength(0, numBytes, result.readData));
    }

    // Socket CLOSE
    [[nodiscard]] Result activateAsync(AsyncSocketClose& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_close(submission, async.handle);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncSocketClose::Result& result)
    {
        result.returnCode = Result(true);
        return Result(true);
    }

    // File READ
    [[nodiscard]] Result activateAsync(AsyncFileRead& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_read(submission, async.fileDescriptor, async.readBuffer.data(),
                                          async.readBuffer.sizeInBytes(), async.offset);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncFileRead::Result& result)
    {
        const size_t numBytes = static_cast<size_t>(events[result.async.eventIndex].res);
        return Result(result.async.readBuffer.sliceStartLength(0, numBytes, result.readData));
    }

    // File WRITE
    [[nodiscard]] Result activateAsync(AsyncFileWrite& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_write(submission, async.fileDescriptor, async.writeBuffer.data(),
                                           async.writeBuffer.sizeInBytes(), 0);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncFileWrite::Result& result)
    {
        const size_t numBytes = static_cast<size_t>(events[result.async.eventIndex].res);
        result.writtenBytes   = numBytes;
        return Result(numBytes == result.async.writeBuffer.sizeInBytes());
    }

    // File CLOSE
    [[nodiscard]] Result activateAsync(AsyncFileClose& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_close(submission, async.fileDescriptor);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncFileClose::Result& result)
    {
        result.returnCode = Result(true);
        return Result(true);
    }

    // File POLL
    [[nodiscard]] Result activateAsync(AsyncFilePoll& async)
    {
        // Documentation says:
        // "Unlike poll or epoll without EPOLLONESHOT, this interface always works in one-shot mode. That is, once the
        // poll operation is completed, it will have to be resubmitted."
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_poll_add(submission, async.fileDescriptor, POLLIN);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result cancelAsync(AsyncFilePoll& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_poll_remove(submission, &async);
        // Intentionally not calling io_uring_sqe_set_data here, as we don't care being notified about the removal
        return Result(true);
    }

    // Process EXIT
    [[nodiscard]] Result setupAsync(AsyncProcessExit& async)
    {
        const int pidFd = ::syscall(SYS_pidfd_open, async.handle, SOCK_NONBLOCK); // == PIDFD_NONBLOCK
        if (pidFd < 0)
        {
            return Result::Error("pidfd_open failed");
        }
        SC_TRY(async.pidFd.assign(pidFd));
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_poll_add(submission, pidFd, POLLIN);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncProcessExit::Result& result)
    {
        int   status = -1;
        pid_t waitPid;
        do
        {
            waitPid = ::waitpid(result.async.handle, &status, 0);
        } while (waitPid == -1 and errno == EINTR);
        if (waitPid == -1)
        {
            return Result::Error("waitPid");
        }
        if (WIFEXITED(status) != 0)
        {
            result.exitStatus.status = WEXITSTATUS(status);
        }
        return Result(true);
    }

    [[nodiscard]] Result teardownAsync(AsyncProcessExit& async) { return async.pidFd.close(); }

    // Templates

    template <typename T>
    [[nodiscard]] Result cancelAsync(T& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_cancel(submission, &async, 0);
        // Intentionally not calling io_uring_sqe_set_data here, as we don't care being notified about the removal
        return Result(true);
    }

    // clang-format off
    template <typename T> [[nodiscard]] Result setupAsync(T&)     { return Result(true); }
    template <typename T> [[nodiscard]] Result teardownAsync(T&)  { return Result(true); }
    template <typename T> [[nodiscard]] Result activateAsync(T&)  { return Result(true); }
    template <typename T> [[nodiscard]] Result completeAsync(T&)  { return Result(true); }
    // clang-format on
};

//----------------------------------------------------------------------------------------
// AsyncEventLoop::Internal
//----------------------------------------------------------------------------------------
SC::AsyncEventLoop::Internal::Internal()
{
    (void)globalLibURing.init();
    isEpoll = not globalLibURing.isValid();
    isEpoll ? placementNew(storage.reinterpret_as<InternalPosix>())
            : placementNew(storage.reinterpret_as<InternalIoURing>());
}

SC::AsyncEventLoop::Internal::~Internal()
{
    isEpoll ? storage.reinterpret_as<InternalPosix>().~InternalPosix()
            : storage.reinterpret_as<InternalIoURing>().~InternalIoURing();
}

SC::AsyncEventLoop::InternalIoURing& SC::AsyncEventLoop::Internal::getUring()
{
    return storage.reinterpret_as<InternalIoURing>();
}

SC::AsyncEventLoop::InternalPosix& SC::AsyncEventLoop::Internal::getPosix()
{
    return storage.reinterpret_as<InternalPosix>();
}

SC::Result SC::AsyncEventLoop::Internal::close() { return isEpoll ? getPosix().close() : getUring().close(); }

SC::Result SC::AsyncEventLoop::Internal::createEventLoop(AsyncEventLoop::Options options)
{
    if (options.apiType == AsyncEventLoop::Options::ApiType::ForceUseEpoll and not isEpoll)
    {
        storage.reinterpret_as<InternalIoURing>().~InternalIoURing();
        isEpoll = true;
        placementNew(storage.reinterpret_as<InternalPosix>());
    }
    else if (options.apiType == AsyncEventLoop::Options::ApiType::ForceUseEpoll and isEpoll)
    {
        storage.reinterpret_as<InternalPosix>().~InternalPosix();
        isEpoll = false;
        placementNew(storage.reinterpret_as<InternalIoURing>());
    }
    return isEpoll ? getPosix().createEventLoop() : getUring().createEventLoop();
}

SC::Result SC::AsyncEventLoop::Internal::createSharedWatchers(AsyncEventLoop& eventLoop)
{
    return isEpoll ? getPosix().createSharedWatchers(eventLoop) : getUring().createSharedWatchers(eventLoop);
}

SC::Result SC::AsyncEventLoop::Internal::wakeUpFromExternalThread()
{
    return isEpoll ? getPosix().wakeUpFromExternalThread() : getUring().wakeUpFromExternalThread();
}

//----------------------------------------------------------------------------------------
// AsyncEventLoop::KernelQueue
//----------------------------------------------------------------------------------------

SC::AsyncEventLoop::KernelQueue::KernelQueue(Internal& internal)
{
    isEpoll = internal.isEpoll;
    isEpoll ? placementNew(storage.reinterpret_as<KernelQueuePosix>(), *this)
            : placementNew(storage.reinterpret_as<KernelQueueIoURing>(), *this);
}

SC::AsyncEventLoop::KernelQueue::~KernelQueue()
{
    isEpoll ? storage.reinterpret_as<KernelQueuePosix>().~KernelQueuePosix()
            : storage.reinterpret_as<KernelQueueIoURing>().~KernelQueueIoURing();
}

SC::AsyncEventLoop::KernelQueueIoURing& SC::AsyncEventLoop::KernelQueue::getUring()
{
    return storage.reinterpret_as<KernelQueueIoURing>();
}

SC::AsyncEventLoop::KernelQueuePosix& SC::AsyncEventLoop::KernelQueue::getPosix()
{
    return storage.reinterpret_as<KernelQueuePosix>();
}

const SC::AsyncEventLoop::KernelQueueIoURing& SC::AsyncEventLoop::KernelQueue::getUring() const
{
    return storage.reinterpret_as<const KernelQueueIoURing>();
}

const SC::AsyncEventLoop::KernelQueuePosix& SC::AsyncEventLoop::KernelQueue::getPosix() const
{
    return storage.reinterpret_as<const KernelQueuePosix>();
}
SC::uint32_t SC::AsyncEventLoop::KernelQueue::getNumEvents() const
{
    return isEpoll ? getPosix().getNumEvents() : getUring().getNumEvents();
}

SC::Result SC::AsyncEventLoop::KernelQueue::syncWithKernel(AsyncEventLoop& eventLoop, SyncMode syncMode)
{
    return isEpoll ? getPosix().syncWithKernel(eventLoop, syncMode) : getUring().syncWithKernel(eventLoop, syncMode);
}

SC::Result SC::AsyncEventLoop::KernelQueue::validateEvent(uint32_t& idx, bool& continueProcessing)
{
    return isEpoll ? getPosix().validateEvent(idx, continueProcessing)
                   : getUring().validateEvent(idx, continueProcessing);
}

SC::AsyncRequest* SC::AsyncEventLoop::KernelQueue::getAsyncRequest(uint32_t idx)
{
    return isEpoll ? getPosix().getAsyncRequest(idx) : getUring().getAsyncRequest(idx);
}

// clang-format off
template <typename T>  SC::Result SC::AsyncEventLoop::KernelQueue::setupAsync(T& async)    { return isEpoll ? getPosix().setupAsync(async) : getUring().setupAsync(async); }
template <typename T>  SC::Result SC::AsyncEventLoop::KernelQueue::teardownAsync(T& async) { return isEpoll ? getPosix().teardownAsync(async) : getUring().teardownAsync(async); }
template <typename T>  SC::Result SC::AsyncEventLoop::KernelQueue::activateAsync(T& async) { return isEpoll ? getPosix().activateAsync(async) : getUring().activateAsync(async); }
template <typename T>  SC::Result SC::AsyncEventLoop::KernelQueue::completeAsync(T& async) { return isEpoll ? getPosix().completeAsync(async) : getUring().completeAsync(async); }
template <typename T>  SC::Result SC::AsyncEventLoop::KernelQueue::cancelAsync(T& async)   { return isEpoll ? getPosix().cancelAsync(async) : getUring().cancelAsync(async); }
// clang-format on
