// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include <Windows.h>

#include "EventLoop.h"
#include "EventLoopWindows.h"

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;
    Async          wakeUpAsync;

    EventLoopWindowsOverlapped wakeUpOverlapped = {&wakeUpAsync};

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
        wakeUpAsync.callback.bind<Internal, &Internal::runCompletionForWakeUp>(this);
        // No need to register it with EventLoop as we're calling PostQueuedCompletionStatus manually
        return true;
    }

    [[nodiscard]] Async* getAsync(OVERLAPPED_ENTRY& event) const
    {
        return EventLoopWindowsOverlapped::getUserDataFromOverlapped<Async>(event.lpOverlapped);
    }

    [[nodiscard]] void* getUserData(OVERLAPPED_ENTRY& event) const
    {
        return reinterpret_cast<void*>(event.lpCompletionKey);
    }

    void runCompletionForWakeUp(AsyncResult& result) { result.eventLoop.runCompletionForNotifiers(); }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread()
{
    Internal&            self = internal.get();
    FileDescriptorNative loopNativeDescriptor;
    SC_TRY_IF(self.loopFd.handle.get(loopNativeDescriptor, "watchInputs - Invalid Handle"_a8));

    if (PostQueuedCompletionStatus(loopNativeDescriptor, 0, 0, &self.wakeUpOverlapped.overlapped) == FALSE)
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
        case Async::Operation::Type::Timeout: eventLoop.activeTimers.queueBack(*async); break;
        case Async::Operation::Type::Read:
            addReadWatcher(eventLoop.internal.get().loopFd, async->operation.fields.read.fileDescriptor);
            eventLoop.stagedHandles.queueBack(*async);
            break;
        case Async::Operation::Type::WakeUp: eventLoop.activeWakeUps.queueBack(*async); break;
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
            GetQueuedCompletionStatusEx(loopNativeDescriptor, events, static_cast<ULONG>(SizeOfArray(events)),
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
