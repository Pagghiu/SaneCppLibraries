// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stddef.h> // offsetof
// Undefine Windows API functions that conflict with our AsyncFileSystemOperation enumeration...
#ifdef CopyFile
#undef CopyFile
#endif
#ifdef RemoveDirectory
#undef RemoveDirectory
#endif
// We store a user pointer at a fixed offset from overlapped to allow getting back source object
// with results from GetQueuedCompletionStatusEx.
// We must do it because there is no void* userData pointer in the OVERLAPPED struct
struct SC::detail::AsyncWinOverlapped
{
    void*      userData = nullptr;
    OVERLAPPED overlapped;

    AsyncWinOverlapped() { memset(&overlapped, 0, sizeof(overlapped)); }

    template <typename T>
    [[nodiscard]] static T* getUserDataFromOverlapped(LPOVERLAPPED lpOverlapped)
    {
        constexpr size_t offsetOfOverlapped = offsetof(AsyncWinOverlapped, overlapped);
        constexpr size_t offsetOfUserData   = offsetof(AsyncWinOverlapped, userData);
        return *reinterpret_cast<T**>(reinterpret_cast<uint8_t*>(lpOverlapped) - offsetOfOverlapped + offsetOfUserData);
    }
};
