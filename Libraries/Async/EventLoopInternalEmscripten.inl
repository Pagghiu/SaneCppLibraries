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
};

struct SC::EventLoop::KernelQueue
{
    int newEvents = 0;
    int events[1] = {0};

    [[nodiscard]] ReturnCode pushStagedAsync(Async& async) { return false; }
    [[nodiscard]] ReturnCode pollAsync(EventLoop& eventLoop, PollMode pollMode) { return false; }
    [[nodiscard]] ReturnCode shouldProcessCompletion(int& event) { return true; }
    template <typename T>
    [[nodiscard]] ReturnCode setupAsync(T&)
    {
        return false;
    }
    template <typename T>
    [[nodiscard]] ReturnCode stopAsync(T&)
    {
        return false;
    }
    template <typename T>
    [[nodiscard]] ReturnCode activateAsync(T&)
    {
        return false;
    }
    template <typename T>
    [[nodiscard]] ReturnCode completeAsync(T&)
    {
        return false;
    }
};

SC::ReturnCode SC::EventLoop::wakeUpFromExternalThread() { return true; }
SC::ReturnCode SC::EventLoop::associateExternallyCreatedTCPSocket(SocketDescriptor& outDescriptor) { return true; }
SC::ReturnCode SC::EventLoop::associateExternallyCreatedFileDescriptor(FileDescriptor& outDescriptor) { return true; }
