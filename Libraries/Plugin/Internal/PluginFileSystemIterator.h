// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../PluginError.h"
#include "PluginString.h"

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <dirent.h> // opendir, readdir, closedir
#include <errno.h>
#include <sys/stat.h> // stat
#endif

namespace SC
{
struct PluginFileSystemIterator
{
    struct Entry
    {
        StringSpan name;
        bool       isDirectory;
    };
    PluginFileSystemIterator() = default;
    ~PluginFileSystemIterator() { close(); }

    ResultPlugin init(StringSpan directoryPath)
    {
        if (not directory.assign(directoryPath))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::IteratorAssignDirectory);

        started       = false;
        pathSeparator = SC_NATIVE_STR("/");

#if SC_PLATFORM_WINDOWS
        StringPath searchPath = directory;
        if (not searchPath.append(pathSeparator) or not searchPath.append("*"))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::IteratorBuildSearchPath);
        hFind = ::FindFirstFileW(searchPath.view().getNullTerminatedNative(), &findData);
        if (hFind == INVALID_HANDLE_VALUE)
            return ResultPlugin::withNativeError(PluginError::DirectoryOpenFailed,
                                                 PluginErrorDetail::WindowsDirectoryFindFirst, ::GetLastError());
#else
        dir = ::opendir(directory.view().getNullTerminatedNative());
        if (dir == nullptr)
            return ResultPlugin::withNativeError(PluginError::DirectoryOpenFailed,
                                                 PluginErrorDetail::PosixDirectoryOpen, static_cast<uint32_t>(errno));
#endif
        return ResultPlugin(true);
    }

    void close()
    {
#if SC_PLATFORM_WINDOWS
        if (hFind != INVALID_HANDLE_VALUE)
        {
            ::FindClose(hFind);
            hFind = INVALID_HANDLE_VALUE;
        }
#else
        if (dir)
        {
            ::closedir(dir);
            dir = nullptr;
        }
#endif
    }

    ResultPlugin next(Entry& entry, bool& hasEntry)
    {
        hasEntry = false;
#if SC_PLATFORM_WINDOWS
        if (not started)
        {
            started = true;
        }
        else
        {
            if (::FindNextFileW(hFind, &findData) == FALSE)
            {
                const DWORD error = ::GetLastError();
                if (error == ERROR_NO_MORE_FILES)
                    return ResultPlugin(true);
                return ResultPlugin::withNativeError(PluginError::DirectoryReadFailed,
                                                     PluginErrorDetail::WindowsDirectoryFindNext, error);
            }
        }
        StringSpan nativeName = StringSpan::fromNullTerminated(findData.cFileName, StringEncoding::Utf16);
        if (not currentEntryName.assign(nativeName))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::IteratorBuildEntryPath);
        entry.name        = currentEntryName.view();
        entry.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        hasEntry          = true;
        return ResultPlugin(true);
#else
        errno                  = 0;
        struct dirent* current = ::readdir(dir);
        if (current == nullptr)
        {
            if (errno == 0)
                return ResultPlugin(true);
            return ResultPlugin::withNativeError(PluginError::DirectoryReadFailed,
                                                 PluginErrorDetail::PosixDirectoryRead, static_cast<uint32_t>(errno));
        }
        StringSpan entryName = StringSpan::fromNullTerminated(current->d_name, StringEncoding::Utf8);
        StringPath fullPath  = directory;
        if (not fullPath.append(pathSeparator) or not fullPath.append(entryName))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::IteratorBuildEntryPath);
        struct stat statBuffer;
        const int   statRes = ::stat(fullPath.view().getNullTerminatedNative(), &statBuffer);
        entry.isDirectory   = (statRes == 0 and S_ISDIR(statBuffer.st_mode));
        if (not currentEntryName.assign(entryName))
            return ResultPlugin(PluginError::PathCapacityExceeded, PluginErrorDetail::IteratorBuildEntryPath);
        entry.name = currentEntryName.view();
        hasEntry   = true;
        return ResultPlugin(true);
#endif
    }

    StringSpan pathSeparator;

  private:
    StringPath directory;
    StringPath currentEntryName;
    bool       started = false;

#if SC_PLATFORM_WINDOWS
    HANDLE           hFind    = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW findData = {};
#else
    DIR* dir = nullptr;
#endif
};
} // namespace SC
