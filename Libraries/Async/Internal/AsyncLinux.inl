// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Async/Internal/AsyncInternal.h"
#include "../../Async/Internal/AsyncLinuxAPI.h"
#include "../../Async/Internal/AsyncLinuxKernelEvents.h"
#include "../../Async/Internal/AsyncPosix.inl" // This is only to provide clangd completion (guarded by #pragma once)

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

    enum class TimerState : uint8_t
    {
        Idle,
        Armed,
        Removing,
    };

    bool       ringInited = false;
    TimerState timerState = TimerState::Idle;

    AsyncLinuxIOUring ring;

    AsyncFileReadiness wakeUpPoll;
    FileDescriptor     wakeUpEventFd;

    KernelQueueIoURing() { memset(&ring, 0, sizeof(ring)); }

    ~KernelQueueIoURing() { SC_ASYNC_TRUST_RESULT(close()); }

    Result close()
    {
        SC_TRY(wakeUpEventFd.close());
        if (ringInited)
        {
            ringInited = false;
            ring.close();
        }
        timerState = TimerState::Idle;
        return Result(true);
    }

    Result createEventLoop()
    {
        if (ringInited)
        {
            return Result::Error(AsyncResultCategory, AsyncError::AlreadyInitialized);
        }
        const int uringResult = ring.create(QueueDepth);
        if (uringResult < 0)
        {
            return Result::Error(AsyncResultCategory, AsyncError::EventLoopCreationFailed);
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
            return Result::Error(AsyncResultCategory, AsyncError::WakeUpInitializationFailed);
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
        SC_TRY(wakeUpEventFd.get(eventFd, Result::Error(AsyncResultCategory, AsyncError::InvalidWakeUpHandle)));
        ssize_t eventValue;
        do
        {
            eventValue = ::eventfd_write(eventFd, 1);
        } while (eventValue == -1 && errno == EINTR);

        if (eventValue < 0)
        {
            return Result::Error(AsyncResultCategory, AsyncError::WakeUpFailed);
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
        return reinterpret_cast<AsyncRequest*>(reinterpret_cast<uintptr_t>(AsyncLinuxIOUring::getData(&completion)) &
                                               ~uintptr_t(1));
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
                return Result::Error(AsyncResultCategory, AsyncError::SubmissionCapacityExhausted);
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
            if (cqe.user_data == reinterpret_cast<__u64>(&kq.timerState))
            {
                kq.timerState = KernelQueueIoURing::TimerState::Idle;
                // Sanity check: expired timeouts are reported with ETIME errno
                SC_ASYNC_ASSERT_RELEASE(cqe.res == -ETIME or cqe.res == -ECANCELED);
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
                if (nextTimer and kq.timerState != KernelQueueIoURing::TimerState::Removing)
                {
                    io_uring_sqe* sqe = kq.ring.getSubmission();
                    if (sqe == nullptr)
                    {
                        // TODO: is it correct returning if failing to get a new sqe?
                        return Result::Error(AsyncResultCategory, AsyncError::SubmissionCapacityExhausted);
                    }
                    auto timespec = KernelEventsPosix::timerToRelativeTimespec(eventLoop.internal.loopTime, nextTimer);
                    kts.tv_sec    = timespec.tv_sec;
                    kts.tv_nsec   = timespec.tv_nsec;
                    if (kq.timerState == KernelQueueIoURing::TimerState::Armed)
                    {
                        // Timer was already added earlier let's just update it
                        const __u64 userData = reinterpret_cast<__u64>(&kq.timerState);
                        AsyncLinuxIOUring::prepTimeoutUpdate(sqe, &kts, userData, 0);
                    }
                    else
                    {
                        // We need to add a new timeout
                        AsyncLinuxIOUring::prepTimeout(sqe, &kts, 0, 0);
                        AsyncLinuxIOUring::setData(sqe, &kq.timerState);
                        kq.timerState = KernelQueueIoURing::TimerState::Armed;
                    }
                }
                else if (kq.timerState == KernelQueueIoURing::TimerState::Armed)
                {
                    // Timer was set earlier, but it's not anymore needed, and it must be removed
                    io_uring_sqe* sqe = kq.ring.getSubmission();
                    if (sqe == nullptr)
                    {
                        // TODO: is it correct returning if failing to get a new sqe?
                        return Result::Error(AsyncResultCategory, AsyncError::SubmissionCapacityExhausted);
                    }
                    const __u64 userData = reinterpret_cast<__u64>(&kq.timerState);
                    AsyncLinuxIOUring::prepTimeoutRemove(sqe, userData, 0);
                    kq.timerState = KernelQueueIoURing::TimerState::Removing;
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
                        return Result::Error(AsyncResultCategory, AsyncError::SubmissionCapacityExhausted);
                    }
                }
                else
                {
                    return Result::Error(AsyncResultCategory, AsyncError::SubmissionFailed);
                }
            }

            break;
        }

        return Result(true);
    }

    static void setRequestData(io_uring_sqe* submission, AsyncRequest& async)
    {
        async.flags |= Internal::Flag_KernelOperationPending;
        AsyncLinuxIOUring::setData(submission, &async);
    }

    Result validateEvent(uint32_t idx, bool& continueProcessing)
    {
        io_uring_cqe& completion = events[idx];
        continueProcessing       = completion.user_data != 0;
        if (not continueProcessing)
            return Result(true);
        AsyncRequest& async = *getAsyncRequest(idx);
        if ((completion.user_data & 1) != 0)
        {
            // Even a failed cancellation acknowledgement must quiesce before user_data can be reused.
            async.flags &= ~Internal::Flag_KernelCancelPending;
            continueProcessing = false;
        }
        else
        {
            async.flags &= ~Internal::Flag_KernelOperationPending;
            if (async.state == AsyncRequest::State::Cancelling)
            {
                continueProcessing = false;
                // A suppressed successful accept/open still created a descriptor owned by this completion.
                if (completion.res >= 0 and (async.type == AsyncRequest::Type::SocketAccept or
                                             (async.type == AsyncRequest::Type::FileSystemOperation and
                                              static_cast<AsyncFileSystemOperation&>(async).operation ==
                                                  AsyncFileSystemOperation::Operation::Open)))
                    ::close(completion.res);
            }
        }
        if (not continueProcessing)
        {
            if ((async.flags & (Internal::Flag_KernelOperationPending | Internal::Flag_KernelCancelPending)) == 0)
                async.flags &= ~Internal::Flag_WaitingKernelCancel;
            return Result(true);
        }
        if (completion.res < 0)
        {
            if (completion.res == -EINTR and
                (async.type == AsyncRequest::Type::FileRead or async.type == AsyncRequest::Type::FileWrite))
                return Result(true); // Preserve the existing transparent file-I/O retry.
            continueProcessing = false;
            if (completion.res != -ECANCELED)
                return AsyncCompletionDetail::operationFailure(async.type);
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
        setRequestData(submission, async);
        return Result(true);
    }

    Result completeAsync(AsyncSocketAccept::Result& res)
    {
        if (not res.completionData.acceptedClient.assign(events[res.eventIndex].res))
            return Result::Error(AsyncResultCategory, AsyncError::SocketAcceptFinalizationFailed);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Socket CONNECT
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncSocketConnect& async)
    {
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        struct sockaddr* sockAddr = &async.address.handle.reinterpret_as<struct sockaddr>();
        AsyncLinuxIOUring::prepConnect(submission, async.handle, sockAddr, async.address.sizeOfHandle());
        setRequestData(submission, async);
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
        setRequestData(submission, async);
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
        if (result.completionData.numBytes != totalBytes)
            return Result::Error(AsyncResultCategory, AsyncError::SocketSendIncomplete);
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
        setRequestData(submission, async);
        return Result(true);
    }

    Result completeAsync(AsyncSocketReceive::Result& result)
    {
        io_uring_cqe& completion       = events[result.eventIndex];
        result.completionData.numBytes = static_cast<size_t>(completion.res);
        if (result.getAsync().getType() == AsyncRequest::Type::SocketReceiveFrom)
        {
            AsyncSocketReceiveFrom& async   = static_cast<AsyncSocketReceiveFrom&>(result.getAsync());
            struct msghdr&          msg     = async.typeErasedMsgHdr.reinterpret_as<struct msghdr>();
            async.address.nativeSize        = msg.msg_namelen;
            result.completionData.truncated = (msg.msg_flags & MSG_TRUNC) != 0;
        }
        if (completion.res == 0 and result.getAsync().getType() != AsyncRequest::Type::SocketReceiveFrom)
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
        setRequestData(submission, async);
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
                return Result::Error(AsyncResultCategory, AsyncError::FileReadFailed);
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
        setRequestData(submission, async);
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
        if (result.completionData.numBytes != Internal::getSummedSizeOfBuffers(result.getAsync()))
            return Result::Error(AsyncResultCategory, AsyncError::FileWriteIncomplete);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File SEND (sendfile)
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileSend& async)
    {
        if (not async.splicePipe.readPipe.isValid())
        {
            PipeOptions options;
            options.blocking = false;
            SC_TRY(async.splicePipe.createPipe(options));
        }

        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        int pipeHandle;
        if (async.spliceBytesPending == 0)
        {
            SC_TRY(async.splicePipe.writePipe.get(pipeHandle,
                                                  Result::Error(AsyncResultCategory, AsyncError::InvalidTransferPipe)));
            const size_t remaining = async.length - async.bytesSent;
            const size_t chunk     = remaining > 64 * 1024 ? 64 * 1024 : remaining;
            AsyncLinuxIOUring::prepSplice(submission, async.fileHandle, async.offset, pipeHandle, -1,
                                          static_cast<unsigned int>(chunk), 0);
        }
        else
        {
            SC_TRY(async.splicePipe.readPipe.get(pipeHandle,
                                                 Result::Error(AsyncResultCategory, AsyncError::InvalidTransferPipe)));
            AsyncLinuxIOUring::prepSplice(submission, pipeHandle, -1, async.socketHandle, -1,
                                          static_cast<unsigned int>(async.spliceBytesPending), 0);
        }
        // Observe each stage: the file can supply fewer bytes than the requested range at EOF.
        setRequestData(submission, async);
        return Result(true);
    }

    Result completeAsync(AsyncFileSend::Result& result)
    {
        AsyncFileSend& async = result.getAsync();

        result.completionData.bytesTransferred = async.bytesSent;
        result.completionData.usedZeroCopy     = async.bytesSent > 0;
        const int32_t res                      = events[result.eventIndex].res;
        if (res < 0)
            return Result::Error(AsyncResultCategory, AsyncError::FileSendFailed);
        if (res == 0)
            return Result::Error(AsyncResultCategory, AsyncError::FileSendIncomplete);

        const size_t transferred = static_cast<size_t>(res);
        if (async.spliceBytesPending == 0)
        {
            async.spliceBytesPending = transferred;
        }
        else
        {
            SC_ASYNC_ASSERT_RELEASE(transferred <= async.spliceBytesPending);
            async.spliceBytesPending -= transferred;
            async.bytesSent += transferred;
            async.offset += transferred;
            result.completionData.bytesTransferred = async.bytesSent;
            result.completionData.usedZeroCopy     = true;
            if (async.bytesSent == async.length)
                return Result(true);
        }
        result.shouldCallCallback = false;
        result.reactivateRequest(true);
        return Result(true);
    }

    //-------------------------------------------------------------------------------------------------------
    // File READINESS
    //-------------------------------------------------------------------------------------------------------
    Result activateAsync(AsyncEventLoop& eventLoop, AsyncFileReadiness& async)
    {
        // Documentation says:
        // "Unlike poll or epoll without EPOLLONESHOT, this interface always works in one-shot mode. That is, once the
        // poll operation is completed, it will have to be resubmitted."
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepPollAdd(submission, async.handle, POLLIN);
        setRequestData(submission, async);
        return Result(true);
    }

    Result cancelAsync(AsyncEventLoop& eventLoop, AsyncFileReadiness& async)
    {
        return cancelSubmission(eventLoop, async, true);
    }

    //-------------------------------------------------------------------------------------------------------
    // Process EXIT
    //-------------------------------------------------------------------------------------------------------
    Result setupAsync(AsyncEventLoop& eventLoop, AsyncProcessExit& async)
    {
        const int pidFd = ::syscall(SYS_pidfd_open, async.handle, SOCK_NONBLOCK); // == PIDFD_NONBLOCK
        if (pidFd < 0)
        {
            return Result::Error(AsyncResultCategory, AsyncError::ProcessWatcherCreationFailed);
        }
        SC_ASYNC_ASSERT_RELEASE(async.pidFd.assign(pidFd));
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepPollAdd(submission, pidFd, POLLIN);
        setRequestData(submission, async);
        return Result(true);
    }

    Result completeAsync(AsyncProcessExit::Result& result)
    {
        return KernelEventsPosix::completeProcessExitWaitPid(result);
    }

    static Result teardownAsync(AsyncProcessExit*, AsyncTeardown& teardown)
    {
        // pidfd is copied to fileHandle inside prepareTeardown
        if (::close(teardown.fileHandle) != 0)
            return Result::Error(AsyncResultCategory, AsyncError::ProcessWatcherRemovalFailed);
        return Result(true);
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
        if (sigFd < 0)
            return Result::Error(AsyncResultCategory, AsyncError::SignalWatcherCreationFailed);
        SC_ASYNC_ASSERT_RELEASE(async.signalFd.assign(sigFd));
        async.signalFdHandle = sigFd;

        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        AsyncLinuxIOUring::prepPollAdd(submission, sigFd, POLLIN);
        setRequestData(submission, async);
        return Result(true);
    }

    Result completeAsync(AsyncSignal::Result& result)
    {
        struct signalfd_siginfo fdsi;
        ssize_t                 s = ::read(result.getAsync().signalFdHandle, &fdsi, sizeof(struct signalfd_siginfo));
        if (s != sizeof(struct signalfd_siginfo))
            return Result::Error(AsyncResultCategory, AsyncError::SignalReadFailed);
        result.completionData.signalNumber  = static_cast<int>(fdsi.ssi_signo);
        result.completionData.deliveryCount = 1;
        return Result(true);
    }

    static Result teardownAsync(AsyncSignal*, AsyncTeardown& teardown)
    {
        // signalFd is copied to fileHandle inside prepareTeardown
        if (::close(teardown.fileHandle) != 0)
            return Result::Error(AsyncResultCategory, AsyncError::SignalWatcherRemovalFailed);
        return Result(true);
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
        setRequestData(submission, async);
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
        msg.msg_namelen = sizeof(async.address.handle);

        // Setup receive buffer

        // iovec is binary compatible with Span
        static_assert(sizeof(iovec) == sizeof(Span<const char>), "assert");
        msg.msg_iov    = reinterpret_cast<struct iovec*>(&async.buffer);
        msg.msg_iovlen = 1;

        AsyncLinuxIOUring::prepRecvMsg(submission, async.handle, &msg, 0);
        setRequestData(submission, async);
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
            setRequestData(submission, async);
        }
        break;
        case AsyncFileSystemOperation::Operation::Close: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepClose(submission, async.closeData.handle);
            setRequestData(submission, async);
        }
        break;
        case AsyncFileSystemOperation::Operation::Read: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepRead(submission, async.readData.handle, async.readData.buffer.data(),
                                        async.readData.buffer.sizeInBytes(), async.readData.offset);
            setRequestData(submission, async);
        }
        break;
        case AsyncFileSystemOperation::Operation::Write: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepWrite(submission, async.writeData.handle, async.writeData.buffer.data(),
                                         async.writeData.buffer.sizeInBytes(), async.writeData.offset);
            setRequestData(submission, async);
        }
        break;
        case AsyncFileSystemOperation::Operation::CopyFile: {
            // TODO: Implement this using two splice submissions with IOSQE_IO_LINK
            return Result::Error(AsyncResultCategory, AsyncError::OperationUnsupported);
        }
        break;
        case AsyncFileSystemOperation::Operation::CopyDirectory: {
            return Result::Error(AsyncResultCategory, AsyncError::OperationUnsupported);
        }
        break;
        case AsyncFileSystemOperation::Operation::Rename: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepRename(submission, async.renameData.path.getNullTerminatedNative(),
                                          async.renameData.newPath.getNullTerminatedNative());
            setRequestData(submission, async);
        }
        break;
        case AsyncFileSystemOperation::Operation::RemoveDirectory: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepUnlink(submission, async.removeData.path.getNullTerminatedNative(), AT_REMOVEDIR);
            setRequestData(submission, async);
        }
        break;
        case AsyncFileSystemOperation::Operation::RemoveFile: {
            io_uring_sqe* submission;
            SC_TRY(getNewSubmission(eventLoop, submission));
            AsyncLinuxIOUring::prepUnlink(submission, async.removeData.path.getNullTerminatedNative(), 0);
            setRequestData(submission, async);
        }
        break;
        case AsyncFileSystemOperation::Operation::None: break;
        default: AsyncAssert::unreachable();
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
            return Result::Error(AsyncResultCategory, AsyncError::OperationUnsupported);
        case AsyncFileSystemOperation::Operation::CopyDirectory:
            return Result::Error(AsyncResultCategory, AsyncError::OperationUnsupported);
        case AsyncFileSystemOperation::Operation::Rename: result.completionData.code = completion.res; break;
        case AsyncFileSystemOperation::Operation::RemoveDirectory: result.completionData.code = completion.res; break;
        case AsyncFileSystemOperation::Operation::RemoveFile: result.completionData.code = completion.res; break;
        case AsyncFileSystemOperation::Operation::None: break;
        default: AsyncAssert::unreachable();
        }
        return Result(true);
    }

    Result activateAsync(AsyncEventLoop&, AsyncExternalCompletion& async)
    {
        async.flags &= ~Internal::Flag_ManualCompletion;
        return Result(true);
    }

    Result completeAsync(AsyncExternalCompletion::Result& result)
    {
        AsyncExternalCompletion& async = result.getAsync();
        if (not async.submissionPending)
        {
            return Result::Error(AsyncResultCategory, AsyncError::NoPendingSubmission);
        }
        result.completionData.bytesTransferred = async.bytesTransferred;
        async.submissionPending                = false;
        async.completionPosted                 = false;
        return Result(true);
    }

    Result cancelAsync(AsyncEventLoop& eventLoop, AsyncExternalCompletion& async)
    {
        if (async.completionPosted and (async.flags & Internal::Flag_ManualCompletion))
        {
            eventLoop.internal.manualCompletions.remove(async);
        }
        async.submissionPending = false;
        async.completionPosted  = false;
        return Result(true);
    }

    static bool needsSubmissionWhenReactivating(AsyncExternalCompletion& async)
    {
        SC_ASYNC_ASSERT_RELEASE(async.submissionPending);
        return true;
    }

    //-------------------------------------------------------------------------------------------------------
    // Templates
    //-------------------------------------------------------------------------------------------------------
    Result cancelSubmission(AsyncEventLoop& eventLoop, AsyncRequest& async, bool pollRemove = false)
    {
        if ((async.flags & Internal::Flag_KernelOperationPending) == 0)
            return Result(true);
        io_uring_sqe* submission;
        SC_TRY(getNewSubmission(eventLoop, submission));
        if (pollRemove)
            AsyncLinuxIOUring::prepPollRemove(submission, reinterpret_cast<__u64>(&async));
        else
            AsyncLinuxIOUring::prepCancel(submission, &async, 0);
        static_assert(alignof(AsyncRequest) >= 2, "Cancellation user_data requires one alignment bit");
        AsyncLinuxIOUring::setData(submission, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&async) | 1));
        async.flags |= Internal::Flag_KernelCancelPending | Internal::Flag_WaitingKernelCancel;
        eventLoop.internal.hasPendingKernelCancellations = true;
        return Result(true);
    }

    template <typename T>
    Result cancelAsync(AsyncEventLoop& eventLoop, T& async)
    {
        return cancelSubmission(eventLoop, async);
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
    AsyncAssert::unreachable();
}
