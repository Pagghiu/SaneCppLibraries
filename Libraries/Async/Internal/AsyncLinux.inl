// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncInternal.h"

#include <arpa/inet.h>   // sockaddr_in
#include <stdint.h>      // uint32_t
#include <sys/eventfd.h> // eventfd
#include <sys/poll.h>    // POLLIN
#include <sys/syscall.h> // SYS_pidfd_open
#include <sys/wait.h>    // waitpid

struct SC::AsyncEventLoop::Internal::KernelQueue
{
    AlignedStorage<320> storage;

    bool isEpoll = true;

    KernelQueue();
    ~KernelQueue();
    KernelQueueIoURing& getUring();
    KernelQueuePosix&   getPosix();

    // On io_uring it doesn't make sense to run operations in a thread pool
    [[nodiscard]] bool makesSenseToRunInThreadPool(AsyncRequest&) { return isEpoll; }

    [[nodiscard]] Result close();
    [[nodiscard]] Result createEventLoop(AsyncEventLoop::Options options);
    [[nodiscard]] Result createSharedWatchers(AsyncEventLoop&);
    [[nodiscard]] Result wakeUpFromExternalThread();
    [[nodiscard]] Result associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
    [[nodiscard]] Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
};

struct SC::AsyncEventLoop::Internal::KernelEvents
{
    bool                  isEpoll = true;
    AlignedStorage<16400> storage;

    KernelEvents(KernelQueue& kernelQueue, AsyncKernelEvents& asyncKernelEvents);
    ~KernelEvents();

    KernelEventsIoURing&       getUring();
    KernelEventsPosix&         getPosix();
    const KernelEventsIoURing& getUring() const;
    const KernelEventsPosix&   getPosix() const;

    [[nodiscard]] uint32_t getNumEvents() const;
    [[nodiscard]] Result   syncWithKernel(AsyncEventLoop&, Internal::SyncMode);
    [[nodiscard]] Result   validateEvent(uint32_t&, bool&);

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t);

    // clang-format off
    template <typename T> [[nodiscard]] Result setupAsync(T&);
    template <typename T> [[nodiscard]] Result activateAsync(T&);
    template <typename T> [[nodiscard]] Result completeAsync(T&);
    template <typename T> [[nodiscard]] Result cancelAsync(T&);

    template <typename T> [[nodiscard]] static Result teardownAsync(T*, AsyncTeardown&);

    // If False, makes re-activation a no-op, that is a lightweight optimization.
    // More importantly it prevents an assert about being Submitting state when async completes during re-activation run cycle.
    template<typename T> static bool needsSubmissionWhenReactivating(T&)
    {
        return true;
    }

    bool needsManualTimersProcessing() { return isEpoll; }

    template <typename T, typename P> [[nodiscard]] static Result executeOperation(T&, P& p);
    // clang-format on
};

#define SC_ASYNC_USE_EPOLL 1 // uses epoll
#include "AsyncPosix.inl"

#include "AsyncLinuxAPI.h"

// TODO: Protect it with a mutex or force passing it during creation
static AsyncLinuxLibURingLoader globalLibURing;

bool SC::AsyncEventLoop::tryLoadingLiburing() { return globalLibURing.init(); }

struct SC::AsyncEventLoop::Internal::KernelQueueIoURing
{
    static constexpr int QueueDepth = 64;

    bool     ringInited = false;
    io_uring ring;

    AsyncFilePoll  wakeUpPoll;
    FileDescriptor wakeUpEventFd;

    KernelQueueIoURing() { memset(&ring, 0, sizeof(ring)); }

    ~KernelQueueIoURing() { SC_TRUST_RESULT(close()); }

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
        SC_TRY(eventLoop.runNoWait()); // Register the read handle before everything else
        // Calls to excludeFromActiveCount() must be after runNoWait()
        // WakeUp (poll) doesn't keep the kernelEvents active
        eventLoop.excludeFromActiveCount(wakeUpPoll);
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
        wakeUpPoll.callback.bind<&KernelQueuePosix::completeWakeUp>();
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

    static Result associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
    static Result associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
};

struct SC::AsyncEventLoop::Internal::KernelEventsIoURing
{
  private:
    io_uring_cqe* events;

    KernelEvents& parentKernelEvents;

    int&      newEvents;
    const int totalNumEvents;

  public:
    KernelEventsIoURing(KernelEvents& kq, AsyncKernelEvents& kernelEvents)
        : parentKernelEvents(kq), newEvents(kernelEvents.numberOfEvents),
          totalNumEvents(static_cast<int>(kernelEvents.eventsMemory.sizeInBytes() / sizeof(events[0])))
    {
        events = reinterpret_cast<decltype(events)>(kernelEvents.eventsMemory.data());
    }

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t idx)
    {
        io_uring_cqe& completion = events[idx];
        return reinterpret_cast<AsyncRequest*>(globalLibURing.io_uring_cqe_get_data(&completion));
    }

    uint32_t getNumEvents() const { return static_cast<uint32_t>(newEvents); }

    static io_uring& getRing(AsyncEventLoop& eventLoop) { return eventLoop.internal.kernelQueue.get().getUring().ring; }

    [[nodiscard]] Result getNewSubmission(AsyncRequest& async, io_uring_sqe*& newSubmission)
    {
        io_uring& ring = getRing(*async.eventLoop);
        // Request a new submission slot
        io_uring_sqe* kernelSubmission = globalLibURing.io_uring_get_sqe(&ring);
        if (kernelSubmission == nullptr)
        {
            // No space in the submission kernelEvents, let's try to flush submissions and try again
            SC_TRY(flushSubmissions(*async.eventLoop, Internal::SyncMode::NoWait));
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

    [[nodiscard]] Result syncWithKernel(AsyncEventLoop& eventLoop, Internal::SyncMode syncMode)
    {
        SC_TRY(flushSubmissions(eventLoop, syncMode));
        copyReadyCompletions(getRing(eventLoop));
        return Result(true);
    }

    [[nodiscard]] Result flushSubmissions(AsyncEventLoop& eventLoop, Internal::SyncMode syncMode)
    {
        io_uring& ring = getRing(eventLoop);
        while (true)
        {
            int res = -1;
            switch (syncMode)
            {
            case Internal::SyncMode::NoWait: {
                res = globalLibURing.io_uring_submit(&ring);
                break;
            }
            case Internal::SyncMode::ForcedForwardProgress: {
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
                    // OMG the completion kernelEvents is full, so we can't submit
                    // anything until we free some of the completions slots :-|
                    copyReadyCompletions(ring);
                    if (newEvents > 0)
                    {
                        // We've freed some slots, let's try again
                        eventLoop.internal.runStepExecuteCompletions(parentKernelEvents);
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

    //-------------------------------------------------------------------------------------------------------
    // TIMEOUT
    //-------------------------------------------------------------------------------------------------------
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

        async.expirationTime = async.eventLoop->getLoopTime().offsetBy(async.relativeTimeout);

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
        return res.completionData.acceptedClient.assign(events[res.getAsync().eventIndex].res);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket CONNECT
    //-------------------------------------------------------------------------------------------------------
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

    //-------------------------------------------------------------------------------------------------------
    // Socket SEND
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result activateAsync(AsyncSocketSend& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_send(submission, async.handle, async.buffer.data(), async.buffer.sizeInBytes(), 0);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncSocketSend::Result& result)
    {
        result.completionData.numBytes = static_cast<size_t>(events[result.getAsync().eventIndex].res);
        SC_TRY_MSG(result.completionData.numBytes == result.getAsync().buffer.sizeInBytes(),
                   "send didn't send all data");
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket RECEIVE
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result activateAsync(AsyncSocketReceive& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_recv(submission, async.handle, async.buffer.data(), async.buffer.sizeInBytes(), 0);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncSocketReceive::Result& result)
    {
        io_uring_cqe& completion       = events[result.getAsync().eventIndex];
        result.completionData.numBytes = static_cast<size_t>(completion.res);
        if (completion.res == 0)
        {
            result.completionData.disconnected = true;
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket CLOSE
    //-------------------------------------------------------------------------------------------------------
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

    //-------------------------------------------------------------------------------------------------------
    // File READ
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result activateAsync(AsyncFileRead& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_read(submission, async.fileDescriptor, async.buffer.data(),
                                          async.buffer.sizeInBytes(), async.useOffset ? async.offset : -1);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncFileRead::Result& result)
    {
        io_uring_cqe& completion       = events[result.getAsync().eventIndex];
        result.completionData.numBytes = static_cast<size_t>(completion.res);
        if (completion.res == 0)
        {
            result.completionData.endOfFile = true;
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File WRITE
    //-------------------------------------------------------------------------------------------------------
    [[nodiscard]] Result activateAsync(AsyncFileWrite& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        globalLibURing.io_uring_prep_write(submission, async.fileDescriptor, async.buffer.data(),
                                           async.buffer.sizeInBytes(), async.useOffset ? async.offset : -1);
        globalLibURing.io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result completeAsync(AsyncFileWrite::Result& result)
    {
        const size_t numBytes          = static_cast<size_t>(events[result.getAsync().eventIndex].res);
        result.completionData.numBytes = numBytes;
        return Result(numBytes == result.getAsync().buffer.sizeInBytes());
    }

    //-------------------------------------------------------------------------------------------------------
    // File CLOSE
    //-------------------------------------------------------------------------------------------------------
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

    //-------------------------------------------------------------------------------------------------------
    // File POLL
    //-------------------------------------------------------------------------------------------------------
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

    //-------------------------------------------------------------------------------------------------------
    // Process EXIT
    //-------------------------------------------------------------------------------------------------------
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
        return KernelEventsPosix::completeProcessExitWaitPid(result);
    }

    [[nodiscard]] static Result teardownAsync(AsyncProcessExit*, AsyncTeardown& teardown)
    {
        // pidfd is copied to fileHandle inside prepareTeardown
        return Result(::close(teardown.fileHandle) == 0);
    }

    //-------------------------------------------------------------------------------------------------------
    // Templates
    //-------------------------------------------------------------------------------------------------------
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
    template <typename T> [[nodiscard]] Result activateAsync(T&)  { return Result(true); }
    template <typename T> [[nodiscard]] Result completeAsync(T&)  { return Result(true); }

    template <typename T> [[nodiscard]] static Result teardownAsync(T*, AsyncTeardown&)  { return Result(true); }
    // clang-format on
};

//----------------------------------------------------------------------------------------
// AsyncEventLoop::KernelQueue
//----------------------------------------------------------------------------------------
SC::AsyncEventLoop::Internal::KernelQueue::KernelQueue()
{
    (void)globalLibURing.init();
    isEpoll = not globalLibURing.isValid();
    isEpoll ? placementNew(storage.reinterpret_as<KernelQueuePosix>())
            : placementNew(storage.reinterpret_as<KernelQueueIoURing>());
}

SC::AsyncEventLoop::Internal::KernelQueue::~KernelQueue()
{
    isEpoll ? storage.reinterpret_as<KernelQueuePosix>().~KernelQueuePosix()
            : storage.reinterpret_as<KernelQueueIoURing>().~KernelQueueIoURing();
}

SC::AsyncEventLoop::Internal::KernelQueueIoURing& SC::AsyncEventLoop::Internal::KernelQueue::getUring()
{
    return storage.reinterpret_as<KernelQueueIoURing>();
}

SC::AsyncEventLoop::Internal::KernelQueuePosix& SC::AsyncEventLoop::Internal::KernelQueue::getPosix()
{
    return storage.reinterpret_as<KernelQueuePosix>();
}

SC::Result SC::AsyncEventLoop::Internal::KernelQueue::close()
{
    return isEpoll ? getPosix().close() : getUring().close();
}

SC::Result SC::AsyncEventLoop::Internal::KernelQueue::createEventLoop(AsyncEventLoop::Options options)
{
    if (options.apiType == AsyncEventLoop::Options::ApiType::ForceUseEpoll and not isEpoll)
    {
        storage.reinterpret_as<KernelQueueIoURing>().~KernelQueueIoURing();
        isEpoll = true;
        placementNew(storage.reinterpret_as<KernelQueuePosix>());
    }
    else if (options.apiType == AsyncEventLoop::Options::ApiType::ForceUseEpoll and isEpoll)
    {
        storage.reinterpret_as<KernelQueuePosix>().~KernelQueuePosix();
        isEpoll = false;
        placementNew(storage.reinterpret_as<KernelQueueIoURing>());
    }
    return isEpoll ? getPosix().createEventLoop() : getUring().createEventLoop();
}

SC::Result SC::AsyncEventLoop::Internal::KernelQueue::createSharedWatchers(AsyncEventLoop& eventLoop)
{
    return isEpoll ? getPosix().createSharedWatchers(eventLoop) : getUring().createSharedWatchers(eventLoop);
}

SC::Result SC::AsyncEventLoop::Internal::KernelQueue::wakeUpFromExternalThread()
{
    return isEpoll ? getPosix().wakeUpFromExternalThread() : getUring().wakeUpFromExternalThread();
}

//----------------------------------------------------------------------------------------
// AsyncEventLoop::Internal::KernelEvents
//----------------------------------------------------------------------------------------

SC::AsyncEventLoop::Internal::KernelEvents::KernelEvents(KernelQueue& kernelQueue, AsyncKernelEvents& asyncKernelEvents)
{
    isEpoll = kernelQueue.isEpoll;
    isEpoll ? placementNew(storage.reinterpret_as<KernelEventsPosix>(), *this, asyncKernelEvents)
            : placementNew(storage.reinterpret_as<KernelEventsIoURing>(), *this, asyncKernelEvents);
}

SC::AsyncEventLoop::Internal::KernelEvents::~KernelEvents()
{
    isEpoll ? storage.reinterpret_as<KernelEventsPosix>().~KernelEventsPosix()
            : storage.reinterpret_as<KernelEventsIoURing>().~KernelEventsIoURing();
}

SC::AsyncEventLoop::Internal::KernelEventsIoURing& SC::AsyncEventLoop::Internal::KernelEvents::getUring()
{
    return storage.reinterpret_as<KernelEventsIoURing>();
}

SC::AsyncEventLoop::Internal::KernelEventsPosix& SC::AsyncEventLoop::Internal::KernelEvents::getPosix()
{
    return storage.reinterpret_as<KernelEventsPosix>();
}

const SC::AsyncEventLoop::Internal::KernelEventsIoURing& SC::AsyncEventLoop::Internal::KernelEvents::getUring() const
{
    return storage.reinterpret_as<const KernelEventsIoURing>();
}

const SC::AsyncEventLoop::Internal::KernelEventsPosix& SC::AsyncEventLoop::Internal::KernelEvents::getPosix() const
{
    return storage.reinterpret_as<const KernelEventsPosix>();
}

SC::uint32_t SC::AsyncEventLoop::Internal::KernelEvents::getNumEvents() const
{
    return isEpoll ? getPosix().getNumEvents() : getUring().getNumEvents();
}

SC::Result SC::AsyncEventLoop::Internal::KernelEvents::syncWithKernel(AsyncEventLoop&    eventLoop,
                                                                      Internal::SyncMode syncMode)
{
    return isEpoll ? getPosix().syncWithKernel(eventLoop, syncMode) : getUring().syncWithKernel(eventLoop, syncMode);
}

SC::Result SC::AsyncEventLoop::Internal::KernelEvents::validateEvent(uint32_t& idx, bool& continueProcessing)
{
    return isEpoll ? getPosix().validateEvent(idx, continueProcessing)
                   : getUring().validateEvent(idx, continueProcessing);
}

SC::AsyncRequest* SC::AsyncEventLoop::Internal::KernelEvents::getAsyncRequest(uint32_t idx)
{
    return isEpoll ? getPosix().getAsyncRequest(idx) : getUring().getAsyncRequest(idx);
}

// clang-format off
template <typename T>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::setupAsync(T& async)    { return isEpoll ? getPosix().setupAsync(async) : getUring().setupAsync(async); }
template <typename T>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::activateAsync(T& async) { return isEpoll ? getPosix().activateAsync(async) : getUring().activateAsync(async); }
template <typename T>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::completeAsync(T& async) { return isEpoll ? getPosix().completeAsync(async) : getUring().completeAsync(async); }
template <typename T>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::cancelAsync(T& async)   { return isEpoll ? getPosix().cancelAsync(async) : getUring().cancelAsync(async); }

template <typename T, typename P>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::executeOperation(T& async, P& param)   { return KernelEventsPosix::executeOperation(async, param); }
// clang-format on

template <typename T>
SC::Result SC::AsyncEventLoop::Internal::KernelEvents::teardownAsync(T* async, AsyncTeardown& teardown)
{
    switch (teardown.eventLoop->internal.createOptions.apiType)
    {
    case Options::ApiType::Automatic:
        return not globalLibURing.isValid() ? KernelEventsPosix::teardownAsync(async, teardown)
                                            : KernelEventsIoURing::teardownAsync(async, teardown);
        break;
    case Options::ApiType::ForceUseIOURing: return KernelEventsIoURing::teardownAsync(async, teardown); break;
    case Options::ApiType::ForceUseEpoll: return KernelEventsPosix::teardownAsync(async, teardown); break;
    }
    Assert::unreachable();
}
