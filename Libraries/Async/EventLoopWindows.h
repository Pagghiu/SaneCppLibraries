// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stddef.h> // offsetof

#include "../FileSystem/FileDescriptor.h"
#include "../Foundation/Opaque.h"
#include "EventLoop.h"

namespace SC
{
struct EventLoopWindowsOverlapped;
struct EventLoopWindowsWaitHandle;
} // namespace SC

// We store a user pointer at a fixed offset from overlapped to allow getting back source object
// with results from GetQueuedCompletionStatusEx.
// We must do it because there is no void* userData pointer in the OVERLAPPED struct
struct SC::EventLoopWindowsOverlapped
{
    void*      userData = nullptr;
    OVERLAPPED overlapped;

    EventLoopWindowsOverlapped() { memset(&overlapped, 0, sizeof(overlapped)); }

    template <typename T>
    [[nodiscard]] static T* getUserDataFromOverlapped(LPOVERLAPPED lpOverlapped)
    {
        constexpr size_t offsetOfOverlapped = offsetof(EventLoopWindowsOverlapped, overlapped);
        constexpr size_t offsetOfAsync      = offsetof(EventLoopWindowsOverlapped, userData);
        return *reinterpret_cast<T**>(reinterpret_cast<uint8_t*>(lpOverlapped) - offsetOfOverlapped + offsetOfAsync);
    }
};

namespace SC
{
inline ReturnCode EventLoopWindowsWaitHandleClose(FileDescriptorNative& waitHandle)
{
    if (waitHandle != INVALID_HANDLE_VALUE)
    {
        BOOL res   = UnregisterWaitEx(waitHandle, INVALID_HANDLE_VALUE);
        waitHandle = INVALID_HANDLE_VALUE;
        if (res == FALSE)
        {
            return "UnregisterWaitEx failed"_a8;
        }
    }
    return true;
}
} // namespace SC

struct SC::EventLoopWindowsWaitHandle : public UniqueTaggedHandle<FileDescriptorNative, FileDescriptorNativeInvalid,
                                                                  ReturnCode, EventLoopWindowsWaitHandleClose>
{
};
