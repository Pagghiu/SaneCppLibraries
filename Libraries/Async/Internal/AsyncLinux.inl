// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Async/Internal/AsyncInternal.h"
#include "../../Async/Internal/AsyncLinuxAPI.h"
#include "../../Async/Internal/AsyncLinuxKernelEvents.h"
#include "../../Async/Internal/AsyncPosix.inl" // This is only to provide clangd completion (guarded by #pragma once)
#include "../../Common/Assert.h"

#include <arpa/inet.h>    // sockaddr_in
#include <errno.h>        // errno
#include <poll.h>         // POLLIN
#include <signal.h>       // sigset_t
#include <stdint.h>       // uint32_t
#include <stdio.h>        // snprintf
#include <sys/eventfd.h>  // eventfd
#include <sys/sendfile.h> // sendfile
#include <sys/signalfd.h> // signalfd
#include <sys/stat.h>     // fstat
#include <sys/syscall.h>  // SYS_pidfd_open
#include <sys/wait.h>     // waitpid

bool SC::AsyncEventLoop::tryProbingIOUring() { return AsyncLinuxIOUring::probe(); }

struct SC::AsyncEventLoop::Internal::KernelQueueIoURing
{
    static constexpr int QueueDepth = 64;

    bool ringInited = false;
    bool timerIsSet = false;

    AsyncLinuxIOUring ring;
    char              createErrorMessage[64] = {};

    AsyncFilePoll  wakeUpPoll;
    FileDescriptor wakeUpEventFd;

    KernelQueueIoURing() { memset(&ring, 0, sizeof(ring)); }

    ~KernelQueueIoURing() { SC_TRUST_RESULT(close()); }

    Result close()
    {
        SC_TRY(wakeUpEventFd.close());
        if (ringInited)
        {
            ringInited = false;
            ring.close();
        }
        return Result(true);
    }

    Result createEventLoop()
    {
        if (ringInited)
        {
            return Result::Error("ring already inited");
        }
        const int uringResult = ring.create(QueueDepth);
        if (uringResult < 0)
        {
            const int written = ::snprintf(createErrorMessage, sizeof(createErrorMessage),
                                           "io_uring setup failed (errno=%d)", -uringResult);
            if (written <= 0 or static_cast<size_t>(written) >= sizeof(createErrorMessage))
            {
                return Result::Error("io_uring setup failed");
            }
            return Result::FromStableCharPointer(createErrorMessage);
        }
        ringInited = true;
        return Result(true);
    }

    Result createSharedWatchers(AsyncEventLoop& eventLoop)
    {
        SC_TRY(createWakeup(eventLoop));
        SC_TRY(eventLoop.runNoWait()); // Register the read handle before everything else
        // Calls to excludeFromActiveCount() must be after runNoWait()
        // WakeUp (poll) doesn't keep the kernelEvents active
        eventLoop.excludeFromActiveCount(wakeUpPoll);
        wakeUpPoll.flags |= Flag_Internal;
        return Result(true);
    }

    Result createWakeup(AsyncEventLoop& eventLoop)
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

    static Result associateExternallyCreatedSocketHandle(SocketDescriptor::Handle) { return Result(true); }
    static Result associateExternallyCreatedFileDescriptorHandle(FileDescriptor::Handle) { return Result(true); }
};

struct SC::AsyncEventLoop::Internal::KernelEventsIoURing
{
  private:
    KernelEvents& parentKernelEvents;

    io_uring_cqe*  events;
    io_uring_cqe** eventPointers;

    char lastErrorMessage[96] = {};

    int&      newEvents;
    const int totalNumEvents;

  public:
    KernelEventsIoURing(KernelEvents& kq, AsyncKernelEvents& kernelEvents)
        : parentKernelEvents(kq), newEvents(kernelEvents.numberOfEvents),
          totalNumEvents(static_cast<int>(kernelEvents.eventsMemory.sizeInBytes() /
                                          (sizeof(io_uring_cqe) + sizeof(io_uring_cqe*))))
    {
        // First part of events memory is dedicated to eventPointers, second part to actual events
        eventPointers = reinterpret_cast<decltype(eventPointers)>(kernelEvents.eventsMemory.data());
        events        = reinterpret_cast<decltype(events)>(kernelEvents.eventsMemory.data() +
                                                           totalNumEvents * sizeof(io_uring_cqe*));
    }

    [[nodiscard]] AsyncRequest* getAsyncRequest(uint32_t idx)
    {
        io_uring_cqe& completion = events[idx];
        return reinterpret_cast<AsyncRequest*>(AsyncLinuxIOUring::getData(&completion));
    }

    Result makeIoUringCompletionError(int completionResult)
    {
        const int written = ::snprintf(lastErrorMessage, sizeof(lastErrorMessage),
                                       "Error in processing event (io_uring, errno=%d)", -completionResult);
        if (written <= 0 or static_cast<size_t>(written) >= sizeof(lastErrorMessage))
        {
            return Result::Error("Error in processing event (io_uring)");
        }
        return Result::FromStableCharPointer(lastErrorMessage);
    }

    uint32_t getNumEvents() const { return static_cast<uint32_t>(newEvents); }

    static KernelQueueIoURing& getKernelQueue(AsyncEventLoop& eventLoop)
    {
        return eventLoop.internal.kernelQueue.get().getUring();
    }

    static __u64 getOffsetForReadWrite(int fd, bool useOffset, uint64_t offset)
    {
        if (useOffset)
        {
            return offset;
        }

        if (not isRegularDescriptor(fd))
        {
            return 0;
        }
        return static_cast<__u64>(-1);
    }

    static bool isRegularDescriptor(int fd)
    {
        struct stat descriptorStatus;
        return ::fstat(fd, &descriptorStatus) == 0 and S_ISREG(descriptorStatus.st_mode);
    }

    Result getNewSubmission(AsyncEventLoop& eventLoop, io_uring_sqe*& newSubmission)
    {
        AsyncLinuxIOUring& ring = getKernelQueue(eventLoop).ring;
        // Request a new submission slot
        io_uring_sqe* kernelSubmission = ring.getSubmission();
        if (kernelSubmission == nullptr)
        {
            // No space in the submission kernelEvents, let's try to flush submissions and try again
            SC_TRY(flushSubmissions(eventLoop, Internal::SyncMode::NoWait, nullptr));
            kernelSubmission = ring.getSubmission();
            if (kernelSubmission == nullptr)
            {
                // Not much we can do at this point, we can't really submit
                return Result::Error("io_uring get submission failed");
            }
        }
        newSubmission = kernelSubmission;
        return Result(true);
    }

    void copyReadyCompletions(AsyncEventLoop& eventLoop, const TimeMs* nextTimer)
    {
        KernelQueueIoURing& kq = getKernelQueue(eventLoop);
        // Read up to totalNumEvents completions, copy them into a local array and
        // advance the ring buffer pointers to free ring slots.
        newEvents = kq.ring.peekBatchCompletions(&eventPointers[0], totalNumEvents);

        int writeIdx = 0;
        int readIdx  = 0;

        while (readIdx < newEvents)
        {
            const io_uring_cqe cqe = *eventPointers[readIdx];
            if (cqe.user_data == reinterpret_cast<__u64>(&kq.timerIsSet))
            {
                kq.timerIsSet = false;
                // Sanity check: expired timeouts are reported with ETIME errno
                SC_ASSERT_RELEASE(cqe.res == -ETIME or cqe.res == -ECANCELED);
            }
            else
            {
                events[writeIdx] = cqe;
                writeIdx++;
            }
            readIdx++;
        }
        kq.ring.advanceCompletions(newEvents);

        if (nextTimer)
        {
            if (readIdx != writeIdx)
            {
                // A custom timeout timer was set and it has expired
                eventLoop.internal.runTimers = true;
            }
        }
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
        SC_TRY(flushSubmissions(eventLoop, syncMode, nextTimer));
        copyReadyCompletions(eventLoop, nextTimer);
        return Result(true);
    }

    Result flushSubmissions(AsyncEventLoop& eventLoop, Internal::SyncMode syncMode, const TimeMs* nextTimer)
    {
        KernelQueueIoURing& kq = getKernelQueue(eventLoop);
        while (true)
        {
            int res = -1;
            switch (syncMode)
            {
            case Internal::SyncMode::NoWait: {
                res = kq.ring.submit();
                break;
            }
            case Internal::SyncMode::ForcedForwardProgress: {
                __kernel_timespec kts; // Must stay here to be valid until submit
                if (nextTimer)
                {
                    io_uring_sqe* sqe = kq.ring.getSubmission();
                    if (sqe == nullptr)
                    {
                        // TODO: is it correct returning if failing to get a new sqe?
                        return Result::Error("io_uring get timeout submission failed");
                    }
                    auto timespec = KernelEventsPosix::timerToRelativeTimespec(eventLoop.internal.loopTime, nextTimer);
                    kts.tv_sec    = timespec.tv_sec;
                    kts.tv_nsec   = timespec.tv_nsec;
                    if (kq.timerIsSet)
                    {
                        // Timer was already added earlier let's just update it
                        const __u64 userData = reinterpret_cast<__u64>(&kq.timerIsSet);
                        AsyncLinuxIOUring::prepTimeoutUpdate(sqe, &kts, userData, 0);
                    }
                    else
                    {
                        // We need to add a new timeout
                        AsyncLinuxIOUring::prepTimeout(sqe, &kts, 0, 0);
                        AsyncLinuxIOUring::setData(sqe, &kq.timerIsSet);
                        kq.timerIsSet = true;
                    }
                }
                else if (kq.timerIsSet)
                {
                    // Timer was set earlier, but it's not anymore needed, and it must be removed
                    io_uring_sqe* sqe = kq.ring.getSubmission();
                    if (sqe == nullptr)
                    {
                        // TODO: is it correct returning if failing to get a new sqe?
                        return Result::Error("io_uring get timeout submission failed");
                    }
                    const __u64 userData = reinterpret_cast<__u64>(&kq.timerIsSet);
                    AsyncLinuxIOUring::prepTimeoutRemove(sqe, userData, 0);
                    kq.timerIsSet = false;
                }

                res = kq.ring.submitAndWait(1);
                break;
            }
            }
            if (res < 0)
            {
                const int err = -res;
                if (err == EINTR)
                {
                    continue;
                }
                if (err == EAGAIN or err == EBUSY)
                {
                    // OMG the completion kernelEvents is full, so we can't submit
                    // anything until we free some of the completions slots :-|
                    copyReadyCompletions(eventLoop, nextTimer);
                    if (newEvents > 0)
                    {
                        // We've freed some slots, let's try again
                        eventLoop.internal.runStepExecuteCompletions(eventLoop, parentKernelEvents);
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

    Result validateEvent(uint32_t idx, bool& continueProcessing)
    {
        io_uring_cqe& completion = events[idx];
        // Most cancellation completions have nullptr user_data
        continueProcessing = completion.user_data != 0;
        if (continueProcessing)
        {
            if (completion.res < 0)
            {
                AsyncRequest* async = getAsyncRequest(idx);
                if (completion.res == -EINTR and async != nullptr and
                    (async->type == AsyncRequest::Type::FileRead or async->type == AsyncRequest::Type::FileWrite))
                {
                    // Character devices such as PTYs can transiently surface EINTR on io_uring file I/O.
                    // Match the synchronous POSIX backends and let completeAsync re-submit transparently.
                    continueProcessing = true;
                }
                else
                {
                    continueProcessing = false; // Don't process cancellations
                    if (completion.res != -ECANCELED)
                    {
                        return makeIoUringCompletionError(completion.res);
                    }
                }
            }
            else
            {
                // One exception to the above: AsyncFilePoll is cancelled by matching its
                // user_data that will generate a notification that must still be filtered.
                AsyncRequest* async = getAsyncRequest(idx);
                if (async->state == AsyncRequest::State::Cancelling)
                {
                    continueProcessing = false;
                }
            }
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
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketAccept& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        struct sockaddr* sockAddr     = &async.acceptData->sockAddrHandle.reinterpret_as<struct sockaddr>();
        async.acceptData->sockAddrLen = sizeof(struct sockaddr);
        AsyncLinuxIOUring::prepAccept(submission, async.handle, sockAddr, &async.acceptData->sockAddrLen, SOCK_CLOEXEC);
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    Result completeAsync(AsyncSocketAccept::Result& res)
    {
        return Result(res.completionData.acceptedClient.assign(events[res.eventIndex].res));
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket CONNECT
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketConnect& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        struct sockaddr* sockAddr = &async.ipAddress.handle.reinterpret_as<struct sockaddr>();
        AsyncLinuxIOUring::prepConnect(submission, async.handle, sockAddr, async.ipAddress.sizeOfHandle());
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    Result completeAsync(AsyncSocketConnect::Result& res)
    {
        res.returnCode = Result(true);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket SEND
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketSend& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        if (async.singleBuffer)
        {
            AsyncLinuxIOUring::prepWrite(submission, async.handle, async.buffer.data(), async.buffer.sizeInBytes(), 0);
        }
        else
        {
            // iovec is binary compatible with Span
            static_assert(sizeof(iovec) == sizeof(Span<const char>), "assert");
            const iovec*   vecs  = reinterpret_cast<const iovec*>(async.buffers.data());
            const unsigned nVecs = static_cast<unsigned>(async.buffers.sizeInElements());
            AsyncLinuxIOUring::prepWritev(submission, async.handle, vecs, nVecs, 0);
        }
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    Result completeAsync(AsyncSocketSend::Result& result)
    {
        result.completionData.numBytes = static_cast<size_t>(events[result.eventIndex].res);

        size_t totalBytes = 0;
        if (result.getAsync().singleBuffer)
        {
            totalBytes = result.getAsync().buffer.sizeInBytes();
        }
        else
        {
            for (const auto& buf : result.getAsync().buffers)
            {
                totalBytes += buf.sizeInBytes();
            }
        }
        SC_TRY_MSG(result.completionData.numBytes == totalBytes, "send didn't send all data");
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket RECEIVE
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketReceive& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepRecv(submission, async.handle, async.buffer.data(), async.buffer.sizeInBytes(), 0);
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    Result completeAsync(AsyncSocketReceive::Result& result)
    {
        io_uring_cqe& completion       = events[result.eventIndex];
        result.completionData.numBytes = static_cast<size_t>(completion.res);
        if (completion.res == 0)
        {
            result.completionData.disconnected = true;
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File READ
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileRead& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        if (not async.useOffset and not isRegularDescriptor(async.handle))
        {
            AsyncLinuxIOUring::prepPollAdd(submission, async.handle, POLLIN);
        }
        else
        {
            static_assert(sizeof(iovec) == sizeof(Span<char>), "assert");
            const iovec* vec = reinterpret_cast<const iovec*>(&async.buffer);
            AsyncLinuxIOUring::prepReadv(submission, async.handle, vec, 1,
                                         getOffsetForReadWrite(async.handle, async.useOffset, async.offset));
        }
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    Result completeAsync(AsyncFileRead::Result& result)
    {
        if (not result.getAsync().useOffset and not isRegularDescriptor(result.getAsync().handle))
        {
            ssize_t readBytes;
            do
            {
                readBytes = ::read(result.getAsync().handle, result.getAsync().buffer.data(),
                                   result.getAsync().buffer.sizeInBytes());
            } while (readBytes < 0 and errno == EINTR);

            if (readBytes < 0)
            {
                if (errno == EAGAIN or errno == EWOULDBLOCK)
                {
                    result.shouldCallCallback = false;
                    result.reactivateRequest(true);
                    return Result(true);
                }
                return Result::Error("io_uring file read readiness read failed");
            }

            result.completionData.numBytes = static_cast<size_t>(readBytes);
            if (readBytes == 0)
            {
                result.completionData.endOfFile = true;
            }
            return Result(true);
        }

        io_uring_cqe& completion = events[result.eventIndex];
        if (completion.res == -EINTR)
        {
            result.shouldCallCallback = false;
            result.reactivateRequest(true);
            return Result(true);
        }
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
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileWrite& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        const __u64 off = getOffsetForReadWrite(async.handle, async.useOffset, async.offset);
        if (async.singleBuffer)
        {
            static_assert(sizeof(iovec) == sizeof(Span<const char>), "assert");
            const iovec* vec = reinterpret_cast<const iovec*>(&async.buffer);
            AsyncLinuxIOUring::prepWritev(submission, async.handle, vec, 1, off);
        }
        else
        {
            // iovec is binary compatible with Span
            static_assert(sizeof(iovec) == sizeof(Span<const char>), "assert");
            const iovec*   vecs  = reinterpret_cast<const iovec*>(async.buffers.data());
            const unsigned nVecs = static_cast<unsigned>(async.buffers.sizeInElements());
            AsyncLinuxIOUring::prepWritev(submission, async.handle, vecs, nVecs, off);
        }
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    Result completeAsync(AsyncFileWrite::Result& result)
    {
        io_uring_cqe& completion = events[result.eventIndex];
        if (completion.res == -EINTR)
        {
            result.shouldCallCallback = false;
            result.reactivateRequest(true);
            return Result(true);
        }
        result.completionData.numBytes = static_cast<size_t>(completion.res);
        return Result(result.completionData.numBytes == Internal::getSummedSizeOfBuffers(result.getAsync()));
    }

    //-------------------------------------------------------------------------------------------------------
    // File SEND (sendfile)
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileSend& async)
    {
        if (async.splicePipe.readPipe.isValid() == false)
        {
            PipeOptions options;
            options.blocking = false; // We need non-blocking for splice
            SC_TRY(async.splicePipe.createPipe(options));
            if (async.pipeBufferSize != 0)
            {
                // We likely don't need to resize as default is large enough (16 buffers (64KB))
                // fcntl(async.splicePipe.readPipe.handle, F_SETPIPE_SZ, async.pipeBufferSize);
            }
        }

        io_uring_sqe* submission1;
        io_uring_sqe* submission2;
        SC_TRY(getNewSubmission(eventLoop, submission1));
        SC_TRY(getNewSubmission(eventLoop, submission2));
        // Splice from file to pipe
        const int fdIn = async.fileHandle;
        int       fdPipeW;
        SC_TRY(async.splicePipe.writePipe.get(fdPipeW, Result::Error("Invalid write pipe")));
        AsyncLinuxIOUring::prepSplice(submission1, fdIn, async.offset, fdPipeW, -1,
                                      static_cast<unsigned int>(async.length - async.bytesSent), 0);
        AsyncLinuxIOUring::setData(submission1, nullptr); // Ignore completion of the first part

        submission1->flags |= IOSQE_IO_LINK;

        // Splice from pipe to socket
        int fdPipeR;
        SC_TRY(async.splicePipe.readPipe.get(fdPipeR, Result::Error("Invalid read pipe")));
        const int fdOut = async.socketHandle;
        AsyncLinuxIOUring::prepSplice(submission2, fdPipeR, -1, fdOut, -1,
                                      static_cast<unsigned int>(async.length - async.bytesSent), 0);
        AsyncLinuxIOUring::setData(submission2, &async);

        return Result(true);
    }

    Result completeAsync(AsyncFileSend::Result& result)
    {
        AsyncFileSend& async = result.getAsync();

        // Check for error in the second completion (the one we care about)
        int32_t res = events[result.eventIndex].res;
        if (res < 0)
        {
            return Result::Error("Splice failed");
        }

        const size_t bytesTransferred = static_cast<size_t>(res);
        async.bytesSent += bytesTransferred;
        async.offset += bytesTransferred;

        if (async.bytesSent == async.length)
        {
            result.completionData.bytesTransferred = async.bytesSent;
            return Result(true);
        }
        else
        {
            return Result::Error("Not all data sent in splice");
        }
    }

    //-------------------------------------------------------------------------------------------------------
    // File POLL
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFilePoll& async)
    {
        // Documentation says:
        // "Unlike poll or epoll without EPOLLONESHOT, this interface always works in one-shot mode. That is, once the
        // poll operation is completed, it will have to be resubmitted."
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepPollAdd(submission, async.handle, POLLIN);
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    Result cancelAsync(AsyncEventLoop& eventLoop, AsyncFilePoll& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepPollRemove(submission, reinterpret_cast<__u64>(&async));
        eventLoop.internal.hasPendingKernelCancellations = true;
        // Intentionally not setting user data here, as we don't care being notified about the removal
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Process EXIT
    //-------------------------------------------------------------------------------------------------------
    Result setupAsync(AsyncEventLoop& eventLoop, AsyncProcessExit& async)
    {
#if SC_COMPILER_FILC
        (void)eventLoop;
        (void)async;
        return Result::Error("AsyncProcessExit unsupported under Fil-C on Linux: pidfd_open is unavailable");
#else
        const int pidFd = ::syscall(SYS_pidfd_open, async.handle, SOCK_NONBLOCK); // == PIDFD_NONBLOCK
        if (pidFd < 0)
        {
            return Result::Error("pidfd_open failed");
        }
        SC_ASSERT_RELEASE(async.pidFd.assign(pidFd));
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepPollAdd(submission, pidFd, POLLIN);
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
#endif
    }

    Result completeAsync(AsyncProcessExit::Result& result)
    {
        return KernelEventsPosix::completeProcessExitWaitPid(result);
    }

    static Result teardownAsync(AsyncProcessExit*, AsyncTeardown& teardown)
    {
        // pidfd is copied to fileHandle inside prepareTeardown
        return Result(::close(teardown.fileHandle) == 0);
    }

    //-------------------------------------------------------------------------------------------------------
    // Signal
    //-------------------------------------------------------------------------------------------------------
    Result setupAsync(AsyncEventLoop& eventLoop, AsyncSignal& async)
    {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, async.signalNumber);

        // We must block the signal from the default handler, otherwise the default handler might catch it
        // before signalfd does.
        ::sigprocmask(SIG_BLOCK, &mask, nullptr);

        const int sigFd = ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        SC_TRY_MSG(sigFd >= 0, "signalfd failed");
        SC_ASSERT_RELEASE(async.signalFd.assign(sigFd));
        async.signalFdHandle = sigFd;

        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepPollAdd(submission, sigFd, POLLIN);
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    Result completeAsync(AsyncSignal::Result& result)
    {
        struct signalfd_siginfo fdsi;
        ssize_t                 s = ::read(result.getAsync().signalFdHandle, &fdsi, sizeof(struct signalfd_siginfo));
        SC_TRY_MSG(s == sizeof(struct signalfd_siginfo), "signalfd read failed");
        result.completionData.signalNumber  = static_cast<int>(fdsi.ssi_signo);
        result.completionData.deliveryCount = 1;
        return Result(true);
    }

    static Result teardownAsync(AsyncSignal*, AsyncTeardown& teardown)
    {
        // signalFd is copied to fileHandle inside prepareTeardown
        return Result(::close(teardown.fileHandle) == 0);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket SEND TO
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketSendTo& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));

        struct msghdr& msg = async.typeErasedMsgHdr.reinterpret_as<struct msghdr>();
        memset(&msg, 0, sizeof(msg));

        // Setup message header
        msg.msg_name    = &async.address.handle.reinterpret_as<struct sockaddr>();
        msg.msg_namelen = async.address.sizeOfHandle();

        // iovec is binary compatible with Span
        static_assert(sizeof(iovec) == sizeof(Span<const char>), "assert");
        if (async.singleBuffer)
        {
            msg.msg_iov    = reinterpret_cast<struct iovec*>(&async.buffer);
            msg.msg_iovlen = 1;
        }
        else
        {
            msg.msg_iov    = reinterpret_cast<struct iovec*>(async.buffers.data());
            msg.msg_iovlen = static_cast<int>(async.buffers.sizeInElements());
        }

        AsyncLinuxIOUring::prepSendMsg(submission, async.handle, &msg, 0);
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket RECEIVE FROM
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketReceiveFrom& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));

        struct msghdr& msg = async.typeErasedMsgHdr.reinterpret_as<struct msghdr>();
        memset(&msg, 0, sizeof(msg));

        // Setup message header
        msg.msg_name    = &async.address.handle.reinterpret_as<struct sockaddr>();
        msg.msg_namelen = async.address.sizeOfHandle();

        // Setup receive buffer

        // iovec is binary compatible with Span
        static_assert(sizeof(iovec) == sizeof(Span<const char>), "assert");
        msg.msg_iov    = reinterpret_cast<struct iovec*>(&async.buffer);
        msg.msg_iovlen = 1;

        AsyncLinuxIOUring::prepRecvMsg(submission, async.handle, &msg, 0);
        AsyncLinuxIOUring::setData(submission, &async);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File System Operation
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileSystemOperation& async)
    {
        switch (async.operation)
        {
        case AsyncFileSystemOperation::Operation::Open: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            const int   flags = async.openData.mode.toPosixFlags();
            const int   mode  = async.openData.mode.toPosixAccess();
            const char* path  = async.openData.path.getNullTerminatedNative();
            AsyncLinuxIOUring::prepOpenAt(submission, AT_FDCWD, path, flags, mode);
            AsyncLinuxIOUring::setData(submission, &async);
        }
        break;
        case AsyncFileSystemOperation::Operation::Close: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepClose(submission, async.closeData.handle);
            AsyncLinuxIOUring::setData(submission, &async);
        }
        break;
        case AsyncFileSystemOperation::Operation::Read: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepRead(submission, async.readData.handle, async.readData.buffer.data(),
                                        async.readData.buffer.sizeInBytes(), async.readData.offset);
            AsyncLinuxIOUring::setData(submission, &async);
        }
        break;
        case AsyncFileSystemOperation::Operation::Write: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepWrite(submission, async.writeData.handle, async.writeData.buffer.data(),
                                         async.writeData.buffer.sizeInBytes(), async.writeData.offset);
            AsyncLinuxIOUring::setData(submission, &async);
        }
        break;
        case AsyncFileSystemOperation::Operation::CopyFile: {
            // TODO: Implement this using two splice submissions with IOSQE_IO_LINK
            return Result::Error("AsyncFileSystemOperation::CopyFile - Not implemented");
        }
        break;
        case AsyncFileSystemOperation::Operation::CopyDirectory: {
            return Result::Error("AsyncFileSystemOperation::CopyDirectory - Not implemented");
        }
        break;
        case AsyncFileSystemOperation::Operation::Rename: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepRename(submission, async.renameData.path.getNullTerminatedNative(),
                                          async.renameData.newPath.getNullTerminatedNative());
            AsyncLinuxIOUring::setData(submission, &async);
        }
        break;
        case AsyncFileSystemOperation::Operation::RemoveDirectory: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepUnlink(submission, async.removeData.path.getNullTerminatedNative(), AT_REMOVEDIR);
            AsyncLinuxIOUring::setData(submission, &async);
        }
        break;
        case AsyncFileSystemOperation::Operation::RemoveFile: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepUnlink(submission, async.removeData.path.getNullTerminatedNative(), 0);
            AsyncLinuxIOUring::setData(submission, &async);
        }
        break;
        case AsyncFileSystemOperation::Operation::None: break;
        default: Assert::unreachable();
        }

        return Result(true);
    }

    Result completeAsync(AsyncFileSystemOperation::Result& result)
    {
        io_uring_cqe& completion = events[result.eventIndex];
        switch (result.getAsync().operation)
        {
        case AsyncFileSystemOperation::Operation::Open: result.completionData.handle = completion.res; break;
        case AsyncFileSystemOperation::Operation::Close: result.completionData.code = completion.res; break;
        case AsyncFileSystemOperation::Operation::Read:
            result.completionData.numBytes = static_cast<size_t>(completion.res);
            break;
        case AsyncFileSystemOperation::Operation::Write:
            result.completionData.numBytes = static_cast<size_t>(completion.res);
            break;
        case AsyncFileSystemOperation::Operation::CopyFile:
            return Result::Error("AsyncFileSystemOperation::CopyFile - Not implemented");
        case AsyncFileSystemOperation::Operation::CopyDirectory:
            return Result::Error("AsyncFileSystemOperation::CopyDirectory - Not implemented");
        case AsyncFileSystemOperation::Operation::Rename: result.completionData.code = completion.res; break;
        case AsyncFileSystemOperation::Operation::RemoveDirectory: result.completionData.code = completion.res; break;
        case AsyncFileSystemOperation::Operation::RemoveFile: result.completionData.code = completion.res; break;
        case AsyncFileSystemOperation::Operation::None: break;
        default: Assert::unreachable();
        }
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Templates
    //-------------------------------------------------------------------------------------------------------
    template <typename T>
    Result cancelAsync(AsyncEventLoop& eventLoop, T& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepCancel(submission, &async, 0);
        eventLoop.internal.hasPendingKernelCancellations = true;
        // Intentionally not setting user data here, as we don't care being notified about the removal
        return Result(true);
    }

    // clang-format off
    template <typename T> Result setupAsync(AsyncEventLoop&, T&)     { return Result(true); }
    template <typename T> Result activateAsync(AsyncEventLoop&, T&)  { return Result(true); }
    template <typename T> Result completeAsync(T&)  { return Result(true); }

    template <typename T> static Result teardownAsync(T*, AsyncTeardown&)  { return Result(true); }
    // clang-format on
};

//----------------------------------------------------------------------------------------
// AsyncEventLoop::KernelQueue
//----------------------------------------------------------------------------------------
SC::AsyncEventLoop::Internal::KernelQueue::KernelQueue()
{
    isEpoll = not AsyncLinuxIOUring::probe();
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
    else if (options.apiType == AsyncEventLoop::Options::ApiType::ForceUseIoUring and isEpoll)
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

SC::Result SC::AsyncEventLoop::Internal::KernelQueue::associateExternallyCreatedSocketHandle(
    SocketDescriptor::Handle handle)
{
    return isEpoll ? getPosix().associateExternallyCreatedSocketHandle(handle)
                   : getUring().associateExternallyCreatedSocketHandle(handle);
}

SC::Result SC::AsyncEventLoop::Internal::KernelQueue::associateExternallyCreatedFileDescriptorHandle(
    FileDescriptor::Handle handle)
{
    return isEpoll ? getPosix().associateExternallyCreatedFileDescriptorHandle(handle)
                   : getUring().associateExternallyCreatedFileDescriptorHandle(handle);
}

SC::Result SC::AsyncEventLoop::Internal::KernelQueue::removeAllAssociationsForSocketHandle(
    SocketDescriptor::Handle handle)
{
    return KernelQueuePosix::removeAllAssociationsForSocketHandle(handle);
}

SC::Result SC::AsyncEventLoop::Internal::KernelQueue::removeAllAssociationsForFileDescriptorHandle(
    FileDescriptor::Handle handle)
{
    return KernelQueuePosix::removeAllAssociationsForFileDescriptorHandle(handle);
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
template <typename T>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::setupAsync(AsyncEventLoop& eventLoop, T& async)    { return isEpoll ? getPosix().setupAsync(eventLoop, async) : getUring().setupAsync(eventLoop, async); }
template <typename T>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::activateAsync(AsyncEventLoop& eventLoop, T& async) { return isEpoll ? getPosix().activateAsync(eventLoop, async) : getUring().activateAsync(eventLoop, async); }
template <typename T>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::cancelAsync(AsyncEventLoop& eventLoop, T& async)   { return isEpoll ? getPosix().cancelAsync(eventLoop, async) : getUring().cancelAsync(eventLoop, async); }
template <typename T>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::completeAsync(T& async) { return isEpoll ? getPosix().completeAsync(async) : getUring().completeAsync(async); }

template <typename T, typename P>  SC::Result SC::AsyncEventLoop::Internal::KernelEvents::executeOperation(T& async, P& param)   { return KernelEventsPosix::executeOperation(async, param); }
// clang-format on

template <typename T>
SC::Result SC::AsyncEventLoop::Internal::KernelEvents::teardownAsync(T* async, AsyncTeardown& teardown)
{
    switch (teardown.eventLoop->internal.createOptions.apiType)
    {
    case Options::ApiType::Automatic:
        return teardown.eventLoop->internal.kernelQueue.get().isEpoll
                   ? KernelEventsPosix::teardownAsync(async, teardown)
                   : KernelEventsIoURing::teardownAsync(async, teardown);
        break;
    case Options::ApiType::ForceUseIoUring: return KernelEventsIoURing::teardownAsync(async, teardown); break;
    case Options::ApiType::ForceUseEpoll: return KernelEventsPosix::teardownAsync(async, teardown); break;
    }
    Assert::unreachable();
}
