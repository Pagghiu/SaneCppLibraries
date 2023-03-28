// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include <Windows.h>

#include "EventLoop.h"

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    OVERLAPPED wakeupOverlapped = {0};

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] ReturnCode close() { return loopFd.handle.close(); }

    [[nodiscard]] ReturnCode createEventLoop()
    {
        HANDLE newQueue = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
        if (newQueue == INVALID_HANDLE_VALUE)
        {
            // TODO: Better CreateIoCompletionPort error handling
            return "EventLoop::Internal::createEventLoop() - CreateIoCompletionPort"_a8;
        }
        SC_TRY_IF(loopFd.handle.assign(newQueue));
        return true;
    }

    [[nodiscard]] ReturnCode createWakeup(EventLoop& loop)
    {
        // Not needed, we can just use PostQueuedCompletionStatus directly
        return true;
    }

    [[nodiscard]] bool isWakeUp(const OVERLAPPED_ENTRY& event) const { return event.lpOverlapped == &wakeupOverlapped; }
    [[nodiscard]] Async* getAsync(const OVERLAPPED_ENTRY& event) const { return nullptr; }
    void                 runCompletionForWakeUp(Async& async) {}
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread()
{
    Internal&            self = internal.get();
    FileDescriptorNative loopNativeDescriptor;
    SC_TRY_IF(self.loopFd.handle.get(loopNativeDescriptor, "watchInputs - Invalid Handle"_a8));

    if (not PostQueuedCompletionStatus(loopNativeDescriptor, 0, 0, &self.wakeupOverlapped))
    {
        return "EventLoop::wakeUpFromExternalThread() - PostQueuedCompletionStatus"_a8;
    }
    return true;
}

struct SC::EventLoop::KernelQueue
{
    static constexpr int totalNumEvents = 128;
    OVERLAPPED_ENTRY     events[totalNumEvents];
    ULONG                newEvents = 0;

    [[nodiscard]] ReturnCode pushAsync(EventLoop& eventLoop, Async* async)
    {
        switch (async->operation.type)
        {
        case Async::Operation::Type::Timeout: {
            eventLoop.activeTimers.queueBack(*async);
        }
        break;
        case Async::Operation::Type::Read: {
            addReadWatcher(eventLoop.internal.get().loopFd, async->operation.fields.read.fileDescriptor);
            eventLoop.stagedHandles.queueBack(*async);
        }
        break;
        }
        return true;
    }

    [[nodiscard]] bool isFull() const { return newEvents >= totalNumEvents; }

    void addReadWatcher(FileDescriptor& loopFd, FileDescriptorNative fileDescriptor) {}

    [[nodiscard]] ReturnCode poll(EventLoop& self, const TimeCounter* nextTimer)
    {
        FileDescriptorNative loopNativeDescriptor;
        SC_TRY_IF(self.internal.get().loopFd.handle.get(loopNativeDescriptor,
                                                        "EventLoop::Internal::poll() - Invalid Handle"_a8));
        IntegerMilliseconds timeout;
        if (nextTimer)
        {
            if (nextTimer->isLaterThanOrEqualTo(self.loopTime))
            {
                timeout = nextTimer->subtractApproximate(self.loopTime).inRoundedUpperMilliseconds();
            }
        }
        const BOOL success =
            GetQueuedCompletionStatusEx(loopNativeDescriptor, events, static_cast<ULONG>(ConstantArraySize(events)),
                                        &newEvents, nextTimer ? static_cast<ULONG>(timeout.ms) : INFINITE, FALSE);
        if (not success and GetLastError() != WAIT_TIMEOUT)
        {
            // TODO: GetQueuedCompletionStatusEx error handling
            return "KernelQueue::poll() - GetQueuedCompletionStatusEx error"_a8;
        }
        return true;
    }

    [[nodiscard]] ReturnCode commitQueue(EventLoop& self) { return true; }

  private:
};
