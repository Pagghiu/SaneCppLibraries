// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Async.h"

struct SC::EventLoop::Internal
{
    FileDescriptor loopFd;

    ~Internal() { SC_TRUST_RESULT(close()); }
    [[nodiscard]] Result close() { return loopFd.close(); }
    [[nodiscard]] Result createEventLoop() { return Result(true); }
    [[nodiscard]] Result createWakeup(EventLoop&) { return Result(true); }

    [[nodiscard]] AsyncRequest* getAsyncRequest(const int& event) const { return nullptr; }
};

struct SC::EventLoop::KernelQueue
{
    int newEvents = 0;
    int events[1] = {0};

    [[nodiscard]] Result pushNewSubmission(AsyncRequest& async) { return Result(false); }
    [[nodiscard]] Result pollAsync(EventLoop& eventLoop, PollMode pollMode) { return Result(false); }
    [[nodiscard]] Result validateEvent(int& event, bool& continueProcessing) { return Result(true); }
    template <typename T>
    [[nodiscard]] Result setupAsync(T&)
    {
        return Result(false);
    }
    template <typename T>
    [[nodiscard]] Result stopAsync(T&)
    {
        return Result(false);
    }
    template <typename T>
    [[nodiscard]] Result activateAsync(T&)
    {
        return Result(false);
    }
    template <typename T>
    [[nodiscard]] Result completeAsync(T&)
    {
        return Result(false);
    }
};

SC::Result SC::EventLoop::wakeUpFromExternalThread() { return Result(true); }
SC::Result SC::EventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor) { return Result(true); }
SC::Result SC::EventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor)
{
    return Result(true);
}
