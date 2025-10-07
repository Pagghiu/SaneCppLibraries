// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Strings/Path.h"

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
#else
#include <dirent.h>   // opendir, readdir, closedir
#include <sys/stat.h> // stat
#endif

namespace SC
{
struct PluginFileSystemIterator
{
    struct Entry
    {
        StringView name;
        bool       isDirectory;
    };
    PluginFileSystemIterator() = default;
    ~PluginFileSystemIterator() { close(); }

    Result init(StringView directoryPath)
    {
        SC_TRY(directory.assign(directoryPath));

        started       = false;
        pathSeparator = Path::SeparatorStringView();

#if SC_PLATFORM_WINDOWS
        StringPath searchPath = directory;
        SC_TRY(searchPath.append(pathSeparator));
        SC_TRY(searchPath.append("*"));
        hFind = ::FindFirstFileW(searchPath.view().getNullTerminatedNative(), &findData);
        SC_TRY_MSG(hFind != INVALID_HANDLE_VALUE, "FindFirstFileW failed");
#else
        dir = ::opendir(directory.view().getNullTerminatedNative());
        SC_TRY_MSG(dir, "opendir failed");
#endif
        return Result(true);
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

    bool next(Entry& entry)
    {
#if SC_PLATFORM_WINDOWS
        if (not started)
        {
            started = true;
        }
        else
        {
            SC_TRY(::FindNextFileW(hFind, &findData) == TRUE);
        }
        StringSpan nativeName = StringSpan::fromNullTerminated(findData.cFileName, StringEncoding::Utf16);
        SC_TRY(currentEntryName.assign(nativeName));
        entry.name        = currentEntryName.view();
        entry.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        return true;
#else
        struct dirent* current;
        current = ::readdir(dir);
        SC_TRY(current != nullptr);
        StringView entryName = StringView::fromNullTerminated(current->d_name, StringEncoding::Utf8);
        StringPath fullPath  = directory;
        SC_TRY(fullPath.append(pathSeparator));
        SC_TRY(fullPath.append(entryName));
        struct stat statBuffer;
        const int   statRes = ::stat(fullPath.view().getNullTerminatedNative(), &statBuffer);
        entry.isDirectory   = (statRes == 0 and S_ISDIR(statBuffer.st_mode));
        SC_TRY(currentEntryName.assign(entryName));
        entry.name = currentEntryName.view();
        return true;
#endif
    }

    StringView pathSeparator;

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
