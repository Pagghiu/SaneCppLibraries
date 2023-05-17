// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "EventLoop.h"

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] ReturnCode close() { return loopFd.close(); }
    [[nodiscard]] ReturnCode createEventLoop() { return true; }
    [[nodiscard]] ReturnCode createWakeup(EventLoop&) { return true; }
    [[nodiscard]] Async*     getAsync(const int& event) const { return nullptr; }
    [[nodiscard]] void*      getUserData(const int& event) { return nullptr; }

    [[nodiscard]] ReturnCode canRunCompletionFor(Async& async, int& event) { return true; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Timeout& result, int&) { return false; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Read& result, int&) { return true; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Write& result, int&) { return true; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::WakeUp& result, int&) { return true; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::ProcessExit& result, int&) { return true; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Accept& result, int&) { return true; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Connect& result, int&) { return true; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Send& result, int&) { return true; }
    [[nodiscard]] ReturnCode runCompletionFor(AsyncResult::Receive& result, int&) { return true; }
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
