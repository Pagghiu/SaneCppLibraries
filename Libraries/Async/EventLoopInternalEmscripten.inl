// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "EventLoop.h"
struct SC::Async::ProcessExitInternal
{
    bool dummy;
};
struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] ReturnCode close() { return loopFd.handle.close(); }
    [[nodiscard]] ReturnCode createEventLoop() { return true; }
    [[nodiscard]] ReturnCode createWakeup(EventLoop&) { return true; }
    [[nodiscard]] Async*     getAsync(const int& event) const { return nullptr; }
    [[nodiscard]] void*      getUserData(const int& event) { return nullptr; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult& asyncResult, int&) { return true; }
};
struct SC::EventLoop::KernelQueue
{
    int newEvents = 0;
    int events[1] = {0};

    [[nodiscard]] ReturnCode stageAsync(EventLoop& eventLoop, Async& async) { return false; }
    [[nodiscard]] ReturnCode rearmAsync(EventLoop& eventLoop, Async& async) { return false; }
    [[nodiscard]] ReturnCode flushQueue(EventLoop& self) { return false; }
    [[nodiscard]] ReturnCode poll(EventLoop& eventLoop, const TimeCounter* nextTimer) { return false; }
    [[nodiscard]] bool       isFull() { return false; }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread() { return true; }
