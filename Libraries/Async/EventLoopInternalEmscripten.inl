// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "EventLoop.h"

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] ReturnCode close() { return loopFd.handle.close(); }
    [[nodiscard]] ReturnCode createEventLoop() { return true; }
    [[nodiscard]] ReturnCode createWakeup(EventLoop&) { return true; }
    [[nodiscard]] bool       isWakeUp(const int& event) const { return false; }
};
struct SC::EventLoop::KernelQueue
{
    int newEvents = 0;
    int events[1] = {0};

    [[nodiscard]] ReturnCode pushAsync(EventLoop& eventLoop, Async* async) { return false; }
    [[nodiscard]] bool       isFull() { return false; }
    [[nodiscard]] ReturnCode commitQueue(EventLoop& self) { return false; }
    [[nodiscard]] ReturnCode addReadWatcher(FileDescriptor& loopFd, FileDescriptorNative fileDescriptor)
    {
        return false;
    }
    [[nodiscard]] ReturnCode poll(EventLoop& eventLoop, const TimeCounter* nextTimer) { return false; }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread() { return true; }
