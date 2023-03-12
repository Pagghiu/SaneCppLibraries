// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include <errno.h>
#include <sys/event.h> // kqueue
#include <sys/time.h>  // timespec
#include <unistd.h>

struct SC::Loop::Internal
{
    bool           inited = false;
    FileDescriptor loopFd;

    Vector<FileDescriptorNative> watchersQueue;

    FileDescriptorPipe async;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] ReturnCode close() { return loopFd.handle.close(); }

    [[nodiscard]] ReturnCode createLoop()
    {
        const int newQueue = kqueue();
        if (newQueue == -1)
        {
            // TODO: Better kqueue error handling
            return "Loop::Internal::createLoop() - kqueue failed"_a8;
        }
        SC_TRY_IF(loopFd.handle.assign(newQueue));
        return true;
    }

    [[nodiscard]] ReturnCode createLoopAsyncWakeup()
    {
        // Create Async
        SC_TRY_IF(async.createPipe(FileDescriptorPipe::ReadNonInheritable, FileDescriptorPipe::WriteNonInheritable));
        SC_TRY_IF(async.readPipe.setBlocking(false));
        SC_TRY_IF(async.writePipe.setBlocking(false));
        // Register Async
        FileDescriptorNative asyncHandle;
        SC_TRY_IF(async.readPipe.handle.get(asyncHandle,
                                            "Loop::Internal::createLoopAsyncWakeup() - Async read handle invalid"_a8));
        SC_TRY_IF(watchersQueue.push_back(asyncHandle));
        return true;
    }

    struct KernelQueue
    {
        static constexpr int totalNumEvents         = 1024;
        struct kevent        events[totalNumEvents] = {0};
        int                  newEvents              = 0;

        [[nodiscard]] ReturnCode addReadWatcher(FileDescriptor& loopFd, FileDescriptorNative fileDescriptor)
        {
            int fflags = 0;
            EV_SET(events + newEvents, fileDescriptor, EVFILT_READ, EV_ADD, fflags, 0, 0);
            newEvents += 1;
            if (newEvents == totalNumEvents)
            {
                SC_TRY_IF(commitQueue(loopFd));
            }
            return true;
        }

        [[nodiscard]] ReturnCode poll(FileDescriptor& loopFd, IntegerMilliseconds* actualTimeout)
        {
            FileDescriptorNative loopNativeDescriptor;
            SC_TRY_IF(loopFd.handle.get(loopNativeDescriptor, "Loop::Internal::poll() - Invalid Handle"_a8));
            struct timespec specTimeout;
            if (actualTimeout)
            {
                convertToTimespec(*actualTimeout, specTimeout);
            }

            size_t res;
            do
            {
                res = kevent(loopNativeDescriptor, events, newEvents, events, totalNumEvents,
                             actualTimeout ? &specTimeout : nullptr);
            } while (res == -1 && errno == EINTR);
            if (res == -1)
            {
                return "Loop::Internal::poll() - kevent failed"_a8;
            }
            newEvents = static_cast<int>(res);
            return true;
        }

      private:
        static void convertToTimespec(IntegerMilliseconds expiration, struct timespec& spec)
        {
            constexpr uint32_t secondsToNanoseconds  = 1000000;
            constexpr uint32_t secondsToMilliseconds = 1000;

            spec.tv_sec  = expiration.ms / secondsToMilliseconds;
            spec.tv_nsec = (expiration.ms % secondsToMilliseconds) * secondsToNanoseconds;
        }

        [[nodiscard]] ReturnCode commitQueue(FileDescriptor& loopFd)
        {
            FileDescriptorNative loopNativeDescriptor;
            SC_TRY_IF(loopFd.handle.get(loopNativeDescriptor, "Loop::Internal::commitQueue() - Invalid Handle"_a8));

            int res;
            do
            {
                res = kevent(loopNativeDescriptor, events, newEvents, nullptr, 0, nullptr);
            } while (res == -1 && errno == EINTR);
            if (res != 0)
            {
                return "Loop::Internal::commitQueue() - kevent failed"_a8;
            }
            newEvents = 0;
            return true;
        }
    };
};

SC::ReturnCode SC::Loop::wakeUpFromExternalThread()
{
    Internal& self = internal.get();
    // TODO: We need an atomic bool swap to wait until next run
    const void* fakeBuffer;
    ssize_t     numBytes;
    int         asyncFd;
    ssize_t     writtenBytes;
    SC_TRY_IF(self.async.writePipe.handle.get(asyncFd, "writePipe handle"_a8));
    fakeBuffer = "";
    numBytes   = 1;
    do
    {
        writtenBytes = ::write(asyncFd, fakeBuffer, numBytes);
    } while (writtenBytes == -1 && errno == EINTR);

    if (writtenBytes != numBytes)
    {
        return "Loop::wakeUpFromExternalThread - Error in write"_a8;
    }
    return true;
}
