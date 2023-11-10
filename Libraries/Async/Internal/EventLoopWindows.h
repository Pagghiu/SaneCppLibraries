// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stddef.h> // offsetof

#include "../../File/FileDescriptor.h"
#include "../../Foundation/OpaqueUnique.h"

namespace SC
{
struct EventLoopWinOverlapped;
struct EventLoopWinWaitHandle;
struct EventLoopWinWaitDefinition;
} // namespace SC

// We store a user pointer at a fixed offset from overlapped to allow getting back source object
// with results from GetQueuedCompletionStatusEx.
// We must do it because there is no void* userData pointer in the OVERLAPPED struct
struct SC::EventLoopWinOverlapped
{
    void*      userData = nullptr;
    OVERLAPPED overlapped;

    EventLoopWinOverlapped() { memset(&overlapped, 0, sizeof(overlapped)); }

    template <typename T>
    [[nodiscard]] static T* getUserDataFromOverlapped(LPOVERLAPPED lpOverlapped)
    {
        constexpr size_t offsetOfOverlapped = offsetof(EventLoopWinOverlapped, overlapped);
        constexpr size_t offsetOfAsync      = offsetof(EventLoopWinOverlapped, userData);
        return *reinterpret_cast<T**>(reinterpret_cast<uint8_t*>(lpOverlapped) - offsetOfOverlapped + offsetOfAsync);
    }
};
