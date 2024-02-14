// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"

#include <arpa/inet.h>   // sockaddr_in
#include <stdint.h>      // uint32_t
#include <sys/eventfd.h> // eventfd
#include <sys/poll.h>    // POLLIN
#include <sys/syscall.h> // SYS_pidfd_open
#include <sys/wait.h>    // waitpid

// liburing/barrier.h includes <atomic> in C++ mode so this is my best bet to avoid
// giving up on the lovely `--nostdinc++` flag for just a few functions...
// It's ugly but it (hopefully) works.
static inline void IO_URING_WRITE_ONCE(uint32_t& var, uint32_t val)
{
    // std::atomic_store_explicit(reinterpret_cast<std::atomic<T>*>(&var), val, std::memory_order_relaxed);
    __atomic_store(&var, &val, __ATOMIC_RELAXED);
}

static inline void io_uring_smp_store_release(uint32_t* p, uint32_t v)
{
    // std::atomic_store_explicit(reinterpret_cast<std::atomic<T>*>(p), v, std::memory_order_release);
    __atomic_store(p, &v, __ATOMIC_RELEASE);
}

static inline uint32_t io_uring_smp_load_acquire(const uint32_t* value)
{
    // return std::atomic_load_explicit(reinterpret_cast<const std::atomic<T>*>(p), std::memory_order_acquire);
    uint32_t res;
    __atomic_load(value, &res, __ATOMIC_ACQUIRE);
    return res;
}
#define LIBURING_BARRIER_H
#include <liburing.h>

struct SC::AsyncEventLoop::Internal
{
    static constexpr int QueueDepth = 64;

    bool     ringInited = false;
    io_uring ring;

    AsyncFilePoll  wakeUpPoll;
    FileDescriptor wakeUpEventFd;

    Internal() { memset(&ring, sizeof(ring), 0); }

    ~Internal() { SC_TRUST_RESULT(close()); }

    [[nodiscard]] Result close()
    {
        SC_TRY(wakeUpEventFd.close());
        if (ringInited)
        {
            ringInited = false;
            ::io_uring_queue_exit(&ring);
        }
        return Result(true);
    }

    [[nodiscard]] Result createEventLoop()
    {
        if (ringInited)
        {
            return Result::Error("ring already inited");
        }
        const auto uringFd = ::io_uring_queue_init(QueueDepth, &ring, 0);
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
        wakeUpPoll.callback.bind<&Internal::completeWakeUp>();
        SC_TRY(wakeUpPoll.start(eventLoop, newEventFd));
        return Result(true);
    }

    static void completeWakeUp(AsyncFilePoll::Result& result)
    {
        result.async.eventLoop->executeWakeUps(result);
        result.reactivateRequest(true);
    }

    [[nodiscard]] static AsyncRequest* getAsyncRequest(const io_uring_cqe& completion)
    {
        return reinterpret_cast<AsyncRequest*>(::io_uring_cqe_get_data(&completion));
    }
};

struct SC::AsyncEventLoop::KernelQueue
{
    static constexpr int totalNumEvents = 256;

    int          newEvents = 0;
    io_uring_cqe events[totalNumEvents];

    [[nodiscard]] Result getNewSubmission(AsyncRequest& async, io_uring_sqe*& newSubmission)
    {
        io_uring& ring = async.eventLoop->internal.get().ring;
        // Request a new submission slot
        io_uring_sqe* kernelSubmission = ::io_uring_get_sqe(&ring);
        if (kernelSubmission == nullptr)
        {
            // No space in the submission queue, let's try to flush submissions and try again
            SC_TRY(flushSubmissions(*async.eventLoop, SyncMode::NoWait));
            kernelSubmission = ::io_uring_get_sqe(&ring);
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
        newEvents = ::io_uring_peek_batch_cqe(&ring, &eventPointers[0], totalNumEvents);
        for (int idx = 0; idx < newEvents; ++idx)
        {
            events[idx] = *eventPointers[idx];
        }
        ::io_uring_cq_advance(&ring, newEvents);
    }

    [[nodiscard]] Result syncWithKernel(AsyncEventLoop& eventLoop, SyncMode syncMode)
    {
        SC_TRY(flushSubmissions(eventLoop, syncMode));
        copyReadyCompletions(eventLoop.internal.get().ring);
        return Result(true);
    }

    [[nodiscard]] Result flushSubmissions(AsyncEventLoop& eventLoop, SyncMode syncMode)
    {
        io_uring& ring = eventLoop.internal.get().ring;
        while (true)
        {
            int res = -1;
            switch (syncMode)
            {
            case SyncMode::NoWait: {
                res = ::io_uring_submit(&ring);
                break;
            }
            case SyncMode::ForcedForwardProgress: {
                res = ::io_uring_submit_and_wait(&ring, 1);
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
                        eventLoop.runStepExecuteCompletions(*this);
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

    [[nodiscard]] Result validateEvent(io_uring_cqe& completion, bool& continueProcessing)
    {
        // Cancellation completions have nullptr user_data
        continueProcessing = Internal::getAsyncRequest(completion) != nullptr;
        if (continueProcessing and completion.res < 0)
        {
            const AsyncRequest* request = Internal::getAsyncRequest(completion);
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
        ::io_uring_prep_timeout(submission, ts, 0, IORING_TIMEOUT_ABS);
        ::io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result cancelAsync(AsyncLoopTimeout& async)
    {
        // Note: Expired timeouts are reported with ETIME error (see validateEvent)
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        ::io_uring_prep_timeout_remove(submission, reinterpret_cast<__u64>(&async), 0);
        return Result(true);
    }

    // WAKEUP
    [[nodiscard]] static bool setupAsync(AsyncLoopWakeUp& async)
    {
        async.eventLoop->activeWakeUps.queueBack(async);
        return true;
    }

    [[nodiscard]] static bool teardownAsync(AsyncLoopWakeUp& async)
    {
        async.eventLoop->activeWakeUps.remove(async);
        return true;
    }

    // Socket ACCEPT
    [[nodiscard]] bool activateAsync(AsyncSocketAccept& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        struct sockaddr* sockAddr = &async.sockAddrHandle.reinterpret_as<struct sockaddr>();
        async.sockAddrLen         = sizeof(struct sockaddr);
        ::io_uring_prep_accept(submission, async.handle, sockAddr, &async.sockAddrLen, SOCK_CLOEXEC);
        ::io_uring_sqe_set_data(submission, &async);
        return true;
    }

    [[nodiscard]] bool completeAsync(AsyncSocketAccept::Result& res)
    {
        return res.acceptedClient.assign(events[res.async.eventIndex].res);
    }

    // Socket CONNECT
    [[nodiscard]] bool activateAsync(AsyncSocketConnect& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        struct sockaddr* sockAddr = &async.ipAddress.handle.reinterpret_as<struct sockaddr>();
        ::io_uring_prep_connect(submission, async.handle, sockAddr, async.ipAddress.sizeOfHandle());
        ::io_uring_sqe_set_data(submission, &async);
        return true;
    }

    [[nodiscard]] bool completeAsync(AsyncSocketConnect::Result& res)
    {
        res.returnCode = Result(true);
        return Result(true);
    }

    // Socket SEND
    [[nodiscard]] bool activateAsync(AsyncSocketSend& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        ::io_uring_prep_send(submission, async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        ::io_uring_sqe_set_data(submission, &async);
        return true;
    }

    [[nodiscard]] bool completeAsync(AsyncSocketSend::Result& result)
    {
        const size_t numBytes = static_cast<size_t>(events[result.async.eventIndex].res);
        return Result(numBytes == result.async.data.sizeInBytes());
    }

    // Socket RECEIVE
    [[nodiscard]] bool activateAsync(AsyncSocketReceive& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        ::io_uring_prep_recv(submission, async.handle, async.data.data(), async.data.sizeInBytes(), 0);
        ::io_uring_sqe_set_data(submission, &async);
        return true;
    }

    [[nodiscard]] bool completeAsync(AsyncSocketReceive::Result& result)
    {
        const size_t numBytes = static_cast<size_t>(events[result.async.eventIndex].res);
        return Result(result.async.data.sliceStartLength(0, numBytes, result.readData));
    }

    // Socket CLOSE
    [[nodiscard]] bool activateAsync(AsyncSocketClose& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        ::io_uring_prep_close(submission, async.handle);
        ::io_uring_sqe_set_data(submission, &async);
        return true;
    }

    [[nodiscard]] bool completeAsync(AsyncSocketClose::Result& result)
    {
        result.returnCode = Result(true);
        return Result(true);
    }

    // File READ
    [[nodiscard]] bool activateAsync(AsyncFileRead& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        ::io_uring_prep_read(submission, async.fileDescriptor, async.readBuffer.data(), async.readBuffer.sizeInBytes(),
                             async.offset);
        ::io_uring_sqe_set_data(submission, &async);
        return true;
    }

    [[nodiscard]] bool completeAsync(AsyncFileRead::Result& result)
    {
        const size_t numBytes = static_cast<size_t>(events[result.async.eventIndex].res);
        return Result(result.async.readBuffer.sliceStartLength(0, numBytes, result.readData));
    }

    // File WRITE
    [[nodiscard]] bool activateAsync(AsyncFileWrite& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        ::io_uring_prep_write(submission, async.fileDescriptor, async.writeBuffer.data(),
                              async.writeBuffer.sizeInBytes(), 0);
        ::io_uring_sqe_set_data(submission, &async);
        return true;
    }

    [[nodiscard]] bool completeAsync(AsyncFileWrite::Result& result)
    {
        const size_t numBytes = static_cast<size_t>(events[result.async.eventIndex].res);
        result.writtenBytes   = numBytes;
        return Result(numBytes == result.async.writeBuffer.sizeInBytes());
    }

    // File CLOSE
    [[nodiscard]] bool activateAsync(AsyncFileClose& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        ::io_uring_prep_close(submission, async.fileDescriptor);
        ::io_uring_sqe_set_data(submission, &async);
        return true;
    }

    [[nodiscard]] bool completeAsync(AsyncFileClose::Result& result)
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
        ::io_uring_prep_poll_add(submission, async.fileDescriptor, POLLIN);
        ::io_uring_sqe_set_data(submission, &async);
        return Result(true);
    }

    [[nodiscard]] Result cancelAsync(AsyncFilePoll& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(async, submission));
        ::io_uring_prep_poll_remove(submission, &async);
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
        ::io_uring_prep_poll_add(submission, pidFd, POLLIN);
        ::io_uring_sqe_set_data(submission, &async);
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
        ::io_uring_prep_cancel(submission, &async, 0);
        // Intentionally not calling io_uring_sqe_set_data here, as we don't care being notified about the removal
        return Result(true);
    }

    // clang-format off
    template <typename T> [[nodiscard]] bool setupAsync(T&)     { return true; }
    template <typename T> [[nodiscard]] bool teardownAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool activateAsync(T&)  { return true; }
    template <typename T> [[nodiscard]] bool completeAsync(T&)  { return true; }
    // clang-format on
};

SC::Result SC::AsyncEventLoop::wakeUpFromExternalThread()
{
    int eventFd;
    SC_TRY(internal.get().wakeUpEventFd.get(eventFd, Result::Error("writePipe handle")));
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
SC::Result SC::AsyncEventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor&) { return Result(true); }
SC::Result SC::AsyncEventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor&) { return Result(true); }
