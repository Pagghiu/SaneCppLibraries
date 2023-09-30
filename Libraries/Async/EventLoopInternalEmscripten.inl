// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "EventLoop.h"

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] ReturnCode close() { return loopFd.close(); }
    [[nodiscard]] ReturnCode createEventLoop() { return ReturnCode(true); }
    [[nodiscard]] ReturnCode createWakeup(EventLoop&) { return ReturnCode(true); }
    [[nodiscard]] Async*     getAsync(const int& event) const { return nullptr; }
};

struct SC::EventLoop::KernelQueue
{
    int newEvents = 0;
    int events[1] = {0};

    [[nodiscard]] ReturnCode pushNewSubmission(Async& async) { return ReturnCode(false); }
    [[nodiscard]] ReturnCode pollAsync(EventLoop& eventLoop, PollMode pollMode) { return ReturnCode(false); }
    [[nodiscard]] ReturnCode validateEvent(int& event, bool& continueProcessing) { return ReturnCode(true); }
    template <typename T>
    [[nodiscard]] ReturnCode setupAsync(T&)
    {
        return ReturnCode(false);
    }
    template <typename T>
    [[nodiscard]] ReturnCode stopAsync(T&)
    {
        return ReturnCode(false);
    }
    template <typename T>
    [[nodiscard]] ReturnCode activateAsync(T&)
    {
        return ReturnCode(false);
    }
    template <typename T>
    [[nodiscard]] ReturnCode completeAsync(T&)
    {
        return ReturnCode(false);
    }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread() { return ReturnCode(true); }
SC::ReturnCode SC::EventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor)
{
    return ReturnCode(true);
}
SC::ReturnCode SC::EventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor)
{
    return ReturnCode(true);
}
