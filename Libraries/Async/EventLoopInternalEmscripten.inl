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
    [[nodiscard]] ReturnCode close() { return loopFd.close(); }
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
    [[nodiscard]] ReturnCode activateAsync(EventLoop& eventLoop, Async& async) { return false; }
    [[nodiscard]] ReturnCode cancelAsync(EventLoop& eventLoop, Async& async) { return false; }
    [[nodiscard]] ReturnCode pollAsync(EventLoop& eventLoop, PollMode pollMode) { return false; }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread() { return true; }
