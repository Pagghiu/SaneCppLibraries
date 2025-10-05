// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Deferred.h"

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <fcntl.h>    // open
#include <stdio.h>    // remove
#include <sys/stat.h> // stat
#include <unistd.h>   // close
#endif

namespace SC
{
struct PluginFileSystem
{
    static Result readAbsoluteFile(StringSpan path, IGrowableBuffer&& buffer)
    {
#if SC_PLATFORM_WINDOWS
        HANDLE hFile = ::CreateFileW(path.getNullTerminatedNative(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        SC_TRY_MSG(hFile != INVALID_HANDLE_VALUE, "Failed to open file");

        auto deferClose = MakeDeferred([&]() { CloseHandle(hFile); });

        LARGE_INTEGER fileSize;
        SC_TRY_MSG(::GetFileSizeEx(hFile, &fileSize) == TRUE, "Failed to get file size");
        SC_TRY_MSG(buffer.resizeWithoutInitializing(static_cast<size_t>(fileSize.QuadPart)), "Failed to grow buffer");
        DWORD bytesRead = static_cast<DWORD>(fileSize.QuadPart);
        SC_TRY_MSG(::ReadFile(hFile, buffer.data(), bytesRead, &bytesRead, nullptr) == TRUE, "Read failed");
        SC_TRY_MSG(bytesRead == static_cast<DWORD>(fileSize.QuadPart), "Read incomplete");
        return Result(true);
#else
        int fd = ::open(path.getNullTerminatedNative(), O_RDONLY);
        SC_TRY_MSG(fd != -1, "Failed to open file");
        auto deferClose = MakeDeferred([&]() { ::close(fd); });

        struct stat fileStat;
        SC_TRY_MSG(::fstat(fd, &fileStat) != -1, "Failed to get file stat");
        SC_TRY_MSG(buffer.resizeWithoutInitializing(static_cast<size_t>(fileStat.st_size)), "Failed to grow buffer");
        ssize_t bytesRead = ::read(fd, buffer.data(), static_cast<size_t>(fileStat.st_size));
        SC_TRY_MSG(bytesRead != -1, "Read failed");
        SC_TRY_MSG(static_cast<size_t>(bytesRead) == static_cast<size_t>(fileStat.st_size), "Read incomplete");
        return Result(true);
#endif
    }

#if SC_PLATFORM_WINDOWS
    static bool existsAndIsFileAbsolute(StringSpan path)
    {
#if SC_PLATFORM_WINDOWS
        DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
        SC_TRY(res != INVALID_FILE_ATTRIBUTES);
        return (res & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
        struct stat path_stat;
        SC_TRY(::stat(path.getNullTerminatedNative(), &path_stat) != 0);
        return S_ISREG(path_stat.st_mode);
#endif
    }
#endif

    static Result removeFileAbsolute(StringSpan path)
    {
#if SC_PLATFORM_WINDOWS
        SC_TRY_MSG(::DeleteFileW(path.getNullTerminatedNative()), "Failed to remove file");
#else
        SC_TRY_MSG(::remove(path.getNullTerminatedNative()) == 0, "Failed to remove file");
#endif
        return Result(true);
    }
};

} // namespace SC
