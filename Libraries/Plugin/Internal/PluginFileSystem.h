// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/Deferred.h"
#include "../PluginError.h"

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <errno.h>
#include <fcntl.h>    // open
#include <stdio.h>    // remove
#include <sys/stat.h> // stat
#include <unistd.h>   // close
#endif

namespace SC
{
struct PluginFileSystem
{
    static ResultPlugin readAbsoluteFile(StringSpan path, IGrowableBuffer&& buffer)
    {
#if SC_PLATFORM_WINDOWS
        HANDLE hFile = ::CreateFileW(path.getNullTerminatedNative(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return ResultPlugin::withNativeError(PluginError::FileOpenFailed, PluginErrorDetail::WindowsFileCreate,
                                                 ::GetLastError());

        auto deferClose = MakeDeferred([&]() { CloseHandle(hFile); });

        LARGE_INTEGER fileSize;
        if (::GetFileSizeEx(hFile, &fileSize) == FALSE)
            return ResultPlugin::withNativeError(PluginError::FileSizeQueryFailed,
                                                 PluginErrorDetail::WindowsFileGetSize, ::GetLastError());
        if (not buffer.resizeWithoutInitializing(static_cast<size_t>(fileSize.QuadPart)))
            return ResultPlugin::withRequiredBytes(PluginError::FileBufferCapacityExceeded,
                                                   PluginErrorDetail::WindowsFileRead,
                                                   static_cast<uint32_t>(fileSize.QuadPart));
        DWORD bytesRead = static_cast<DWORD>(fileSize.QuadPart);
        if (::ReadFile(hFile, buffer.data(), bytesRead, &bytesRead, nullptr) == FALSE)
            return ResultPlugin::withNativeError(PluginError::FileReadFailed, PluginErrorDetail::WindowsFileRead,
                                                 ::GetLastError());
        if (bytesRead != static_cast<DWORD>(fileSize.QuadPart))
            return ResultPlugin(PluginError::FileReadIncomplete, PluginErrorDetail::WindowsFileRead);
        return ResultPlugin(true);
#else
        int fd = ::open(path.getNullTerminatedNative(), O_RDONLY);
        if (fd == -1)
            return ResultPlugin::withNativeError(PluginError::FileOpenFailed, PluginErrorDetail::PosixFileOpen,
                                                 static_cast<uint32_t>(errno));
        auto deferClose = MakeDeferred([&]() { ::close(fd); });

        struct stat fileStat;
        if (::fstat(fd, &fileStat) == -1)
            return ResultPlugin::withNativeError(PluginError::FileSizeQueryFailed, PluginErrorDetail::PosixFileStat,
                                                 static_cast<uint32_t>(errno));
        if (not buffer.resizeWithoutInitializing(static_cast<size_t>(fileStat.st_size)))
            return ResultPlugin::withRequiredBytes(PluginError::FileBufferCapacityExceeded,
                                                   PluginErrorDetail::PosixFileRead,
                                                   static_cast<uint32_t>(fileStat.st_size));
        ssize_t bytesRead = ::read(fd, buffer.data(), static_cast<size_t>(fileStat.st_size));
        if (bytesRead == -1)
            return ResultPlugin::withNativeError(PluginError::FileReadFailed, PluginErrorDetail::PosixFileRead,
                                                 static_cast<uint32_t>(errno));
        if (static_cast<size_t>(bytesRead) != static_cast<size_t>(fileStat.st_size))
            return ResultPlugin(PluginError::FileReadIncomplete, PluginErrorDetail::PosixFileRead);
        return ResultPlugin(true);
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

    static ResultPlugin removeFileAbsolute(StringSpan path)
    {
#if SC_PLATFORM_WINDOWS
        if (::DeleteFileW(path.getNullTerminatedNative()) == FALSE)
            return ResultPlugin::withNativeError(PluginError::FileRemoveFailed, PluginErrorDetail::WindowsFileDelete,
                                                 ::GetLastError());
#else
        if (::remove(path.getNullTerminatedNative()) != 0)
            return ResultPlugin::withNativeError(PluginError::FileRemoveFailed, PluginErrorDetail::PosixFileRemove,
                                                 static_cast<uint32_t>(errno));
#endif
        return ResultPlugin(true);
    }
};

} // namespace SC
