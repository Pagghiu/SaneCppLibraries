// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include <Windows.h>

#include "EventLoop.h"

namespace SC
{
struct EventLoopWindowsOverlapped;
}

// We store a user pointer at a fixed offset from overlapped to allow getting back source object
// with results from GetQueuedCompletionStatusEx.
// We must do it because there is no void* userData pointer in the OVERLAPPED struct
struct SC::EventLoopWindowsOverlapped
{
    void*      userData   = nullptr;
    OVERLAPPED overlapped = {0};

    template <typename T>
    [[nodiscard]] static T* getUserDataFromOverlapped(LPOVERLAPPED lpOverlapped)
    {
        constexpr size_t offsetOfOverlapped = offsetof(EventLoopWindowsOverlapped, overlapped);
        constexpr size_t offsetOfAsync      = offsetof(EventLoopWindowsOverlapped, userData);
        return *reinterpret_cast<T**>(reinterpret_cast<uint8_t*>(lpOverlapped) - offsetOfOverlapped + offsetOfAsync);
    }
};
