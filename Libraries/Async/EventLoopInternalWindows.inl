// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "EventLoop.h"
#include "EventLoopWindows.h"

struct SC::Async::ProcessExitInternal
{
    EventLoopWindowsOverlapped overlapped;
    EventLoopWindowsWaitHandle waitHandle;
};

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
        SC_UNUSED(loop);
        // Optimization: we avoid one indirect function call, see runCompletionFor checking if async == wakeUpAsync
        // wakeUpAsync.callback.bind<Internal, &Internal::runCompletionForWakeUp>(this);
        wakeUpAsync.operation.assignValue(Async::WakeUp());
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

    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult& result, const OVERLAPPED_ENTRY& entry)
    {
        SC_UNUSED(entry);
        switch (result.async.operation.type)
        {
        case Async::Type::Timeout: {
            // This is used for overlapped notifications (like FileSystemWatcher)
            // It would be probably better to create a dedicated enum type for Overlapped Notifications
            break;
        }
        case Async::Type::Read: {
            // TODO: do the actual read operation here
            break;
        }
        case Async::Type::WakeUp: {
            if (&result.async == &wakeUpAsync)
            {
                runCompletionForWakeUp(result);
            }
            break;
        }
        case Async::Type::ProcessExit: {
            // Process has exited
            // Gather exit code
            Async::ProcessExit&         processExit  = result.async.operation.fields.processExit;
            Async::ProcessExitInternal& procInternal = processExit.opaque.get();
            SC_TRY_IF(procInternal.waitHandle.close());
            DWORD processStatus;
            if (GetExitCodeProcess(processExit.handle, &processStatus) == FALSE)
            {
                return "GetExitCodeProcess failed"_a8;
            }
            result.result.fields.processExit.exitStatus.status = static_cast<int32_t>(processStatus);

            break;
        }
        }
        return true;
    }
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
        case Async::Type::Timeout: {
            eventLoop.activeTimers.queueBack(*async);
            break;
        }
        case Async::Type::Read: {
            SC_TRY_IF(addReadWatcher(eventLoop.internal.get().loopFd, async->operation.fields.read.fileDescriptor));
            eventLoop.stagedHandles.queueBack(*async);
            break;
        }
        case Async::Type::WakeUp: {
            eventLoop.activeWakeUps.queueBack(*async);
            break;
        }
        case Async::Type::ProcessExit: {
            SC_TRY_IF(addProcessWatcher(eventLoop.internal.get().loopFd, *async));
            eventLoop.stagedHandles.queueBack(*async);
            break;
        }
        }
        return true;
    }

    [[nodiscard]] bool isFull() const { return newEvents >= totalNumEvents; }

    [[nodiscard]] ReturnCode addReadWatcher(FileDescriptor& loopFd, FileDescriptorNative fileDescriptor)
    {
        SC_UNUSED(loopFd);
        SC_UNUSED(fileDescriptor);
        return true;
    }

    [[nodiscard]] ReturnCode addProcessWatcher(FileDescriptor& loopFd, Async& async)
    {
        SC_UNUSED(loopFd);
        const ProcessNative         processHandle   = async.operation.fields.processExit.handle;
        Async::ProcessExitInternal& processInternal = async.operation.fields.processExit.opaque.get();
        processInternal.overlapped.userData         = &async;
        HANDLE waitHandle;
        BOOL result = RegisterWaitForSingleObject(&waitHandle, processHandle, KernelQueue::processExitCallback, &async,
                                                  INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE);
        if (result == FALSE)
        {
            return "RegisterWaitForSingleObject failed"_a8;
        }
        return processInternal.waitHandle.assign(waitHandle);
    }

    // This is executed on windows thread pool
    static void CALLBACK processExitCallback(void* data, BOOLEAN timeoutOccurred)
    {
        SC_UNUSED(timeoutOccurred);
        Async&               async = *static_cast<Async*>(data);
        FileDescriptorNative loopNativeDescriptor;
        SC_TRUST_RESULT(async.eventLoop->getLoopFileDescriptor(loopNativeDescriptor));
        Async::ProcessExitInternal& internal = async.operation.fields.processExit.opaque.get();

        if (PostQueuedCompletionStatus(loopNativeDescriptor, 0, 0, &internal.overlapped.overlapped) == FALSE)
        {
            // TODO: Report error?
            // return "EventLoop::wakeUpFromExternalThread() - PostQueuedCompletionStatus"_a8;
        }
    }

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

    [[nodiscard]] ReturnCode commitQueue(EventLoop& self)
    {
        SC_UNUSED(self);
        return true;
    }

  private:
};
