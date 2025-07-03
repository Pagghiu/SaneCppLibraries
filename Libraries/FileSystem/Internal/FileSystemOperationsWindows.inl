// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Deferred.h"
#include "../FileSystemOperations.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wchar.h>

struct SC::FileSystemOperations::Internal
{
    static Result validatePath(StringViewData path)
    {
        if (path.sizeInBytes() == 0)
            return Result::Error("Path is empty");
        if (path.getEncoding() != StringEncoding::Utf16)
            return Result::Error("Path is not native (UTF16)");
        return Result(true);
    }

    static Result copyFile(StringViewData source, StringViewData destination, FileSystemCopyFlags options,
                           bool isDirectory = false);

    static Result copyDirectoryRecursive(const wchar_t* source, const wchar_t* destination, FileSystemCopyFlags flags);

    static Result removeDirectoryRecursiveInternal(const wchar_t* path);
};

#define SC_TRY_WIN32(func, msg)                                                                                        \
    {                                                                                                                  \
        if (func == FALSE)                                                                                             \
        {                                                                                                              \
            return Result::Error(msg);                                                                                 \
        }                                                                                                              \
    }

SC::Result SC::FileSystemOperations::createSymbolicLink(StringViewData sourceFileOrDirectory, StringViewData linkFile)
{
    SC_TRY_MSG(Internal::validatePath(sourceFileOrDirectory), "createSymbolicLink: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(linkFile), "createSymbolicLink: Invalid link path");

    DWORD dwFlags = existsAndIsDirectory(sourceFileOrDirectory) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
    dwFlags |= SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    SC_TRY_WIN32(::CreateSymbolicLinkW(linkFile.getNullTerminatedNative(),
                                       sourceFileOrDirectory.getNullTerminatedNative(), dwFlags),
                 "createSymbolicLink: Failed to create symbolic link");
    return Result(true);
}

SC::Result SC::FileSystemOperations::makeDirectory(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectory: Invalid path");
    SC_TRY_WIN32(::CreateDirectoryW(path.getNullTerminatedNative(), nullptr),
                 "makeDirectory: Failed to create directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::exists(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "exists: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    return Result(res != INVALID_FILE_ATTRIBUTES);
}

SC::Result SC::FileSystemOperations::existsAndIsDirectory(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsDirectory: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

SC::Result SC::FileSystemOperations::existsAndIsFile(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsFile: Invalid path");
    const DWORD res = GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

SC::Result SC::FileSystemOperations::existsAndIsLink(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsLink: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
}

SC::Result SC::FileSystemOperations::removeEmptyDirectory(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeEmptyDirectory: Invalid path");
    SC_TRY_WIN32(::RemoveDirectoryW(path.getNullTerminatedNative()),
                 "removeEmptyDirectory: Failed to remove directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::moveDirectory(StringViewData source, StringViewData destination)
{
    SC_TRY_MSG(Internal::validatePath(source), "moveDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "moveDirectory: Invalid destination path");
    SC_TRY_WIN32(::MoveFileExW(source.getNullTerminatedNative(), destination.getNullTerminatedNative(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED),
                 "moveDirectory: Failed to move directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::removeFile(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeFile: Invalid path");
    SC_TRY_WIN32(::DeleteFileW(path.getNullTerminatedNative()), "removeFile: Failed to remove file");
    return Result(true);
}

SC::Result SC::FileSystemOperations::getFileStat(StringViewData path, FileSystemStat& fileStat)
{
    SC_TRY_MSG(Internal::validatePath(path), "getFileStat: Invalid path");

    HANDLE hFile = ::CreateFileW(path.getNullTerminatedNative(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return Result::Error("getFileStat: Failed to open file");
    }
    auto deferClose = MakeDeferred([&]() { CloseHandle(hFile); });

    FILETIME creationTime, lastAccessTime, modifiedTime;
    if (!::GetFileTime(hFile, &creationTime, &lastAccessTime, &modifiedTime))
    {
        return Result::Error("getFileStat: Failed to get file times");
    }

    ULARGE_INTEGER fileTimeValue;
    fileTimeValue.LowPart  = modifiedTime.dwLowDateTime;
    fileTimeValue.HighPart = modifiedTime.dwHighDateTime;
    fileTimeValue.QuadPart -= 116444736000000000ULL;
    fileStat.modifiedTime = Time::Realtime(fileTimeValue.QuadPart / 10000ULL);

    LARGE_INTEGER fileSize;
    if (!::GetFileSizeEx(hFile, &fileSize))
    {
        return Result::Error("getFileStat: Failed to get file size");
    }
    fileStat.fileSize = static_cast<size_t>(fileSize.QuadPart);
    return Result(true);
}

SC::Result SC::FileSystemOperations::setLastModifiedTime(StringViewData path, Time::Realtime time)
{
    SC_TRY_MSG(Internal::validatePath(path), "setLastModifiedTime: Invalid path");

    HANDLE hFile = ::CreateFileW(path.getNullTerminatedNative(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return Result::Error("setLastModifiedTime: Failed to open file");
    }
    auto deferClose = MakeDeferred([&]() { CloseHandle(hFile); });

    FILETIME creationTime, lastAccessTime;
    if (!::GetFileTime(hFile, &creationTime, &lastAccessTime, nullptr))
    {
        return Result::Error("setLastModifiedTime: Failed to get file times");
    }

    FILETIME       modifiedTime;
    ULARGE_INTEGER fileTimeValue;
    fileTimeValue.QuadPart      = time.getMillisecondsSinceEpoch() * 10000ULL + 116444736000000000ULL;
    modifiedTime.dwLowDateTime  = fileTimeValue.LowPart;
    modifiedTime.dwHighDateTime = fileTimeValue.HighPart;

    SC_TRY_WIN32(::SetFileTime(hFile, &creationTime, &lastAccessTime, &modifiedTime),
                 "setLastModifiedTime: Failed to set file time");
    return Result(true);
}

SC::Result SC::FileSystemOperations::rename(StringViewData path, StringViewData newPath)
{
    SC_TRY_MSG(Internal::validatePath(path), "rename: Invalid path");
    SC_TRY_MSG(Internal::validatePath(newPath), "rename: Invalid new path");
    SC_TRY_WIN32(::MoveFileW(path.getNullTerminatedNative(), newPath.getNullTerminatedNative()),
                 "rename: Failed to rename");
    return Result(true);
}

SC::Result SC::FileSystemOperations::copyFile(StringViewData source, StringViewData destination,
                                              FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(source), "copyFile: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "copyFile: Invalid destination path");

    DWORD copyFlags = COPY_FILE_FAIL_IF_EXISTS;
    if (flags.overwrite)
        copyFlags &= ~COPY_FILE_FAIL_IF_EXISTS;

    SC_TRY_WIN32(CopyFileExW(source.getNullTerminatedNative(), destination.getNullTerminatedNative(), nullptr, nullptr,
                             nullptr, copyFlags),
                 "copyFile: Failed to copy file");
    return Result(true);
}

SC::Result SC::FileSystemOperations::copyDirectory(StringViewData source, StringViewData destination,
                                                   FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(source), "copyDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "copyDirectory: Invalid destination path");

    if (flags.overwrite == false and existsAndIsDirectory(destination))
    {
        return Result::Error("copyDirectory: Destination directory already exists");
    }

    return Internal::copyDirectoryRecursive(source.getNullTerminatedNative(), destination.getNullTerminatedNative(),
                                            flags);
}

SC::Result SC::FileSystemOperations::removeDirectoryRecursive(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeDirectoryRecursive: Invalid path");
    return Internal::removeDirectoryRecursiveInternal(path.getNullTerminatedNative());
}

SC::Result SC::FileSystemOperations::Internal::copyDirectoryRecursive(const wchar_t* source, const wchar_t* destination,
                                                                      FileSystemCopyFlags flags)
{
    // Create destination directory if it doesn't exist
    if (::CreateDirectoryW(destination, nullptr) == FALSE)
    {
        if (::GetLastError() != ERROR_ALREADY_EXISTS)
        {
            return Result::Error("copyDirectoryRecursive: Failed to create destination directory");
        }
    }

    // Prepare search pattern
    wchar_t searchPattern[MAX_PATH];
    if (::swprintf_s(searchPattern, MAX_PATH, L"%s\\*", source) == -1)
    {
        return Result::Error("copyDirectoryRecursive: Path too long");
    }

    WIN32_FIND_DATAW findData;

    HANDLE hFind = ::FindFirstFileW(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return Result::Error("copyDirectoryRecursive: Failed to enumerate directory");
    }
    auto deferClose = MakeDeferred([&]() { ::FindClose(hFind); });

    do
    {
        // Skip . and .. entries
        if (::wcscmp(findData.cFileName, L".") == 0 || ::wcscmp(findData.cFileName, L"..") == 0)
            continue;

        // Build full paths
        wchar_t sourcePath[MAX_PATH];
        wchar_t destPath[MAX_PATH];
        if (::swprintf_s(sourcePath, MAX_PATH, L"%s\\%s", source, findData.cFileName) == -1 ||
            ::swprintf_s(destPath, MAX_PATH, L"%s\\%s", destination, findData.cFileName) == -1)
        {
            return Result::Error("copyDirectoryRecursive: Path too long");
        }

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Recursively copy subdirectory
            SC_TRY(copyDirectoryRecursive(sourcePath, destPath, flags));
        }
        else
        {
            // Copy file
            DWORD copyFlags = COPY_FILE_FAIL_IF_EXISTS;
            if (flags.overwrite)
                copyFlags &= ~COPY_FILE_FAIL_IF_EXISTS;

            if (::CopyFileExW(sourcePath, destPath, nullptr, nullptr, nullptr, copyFlags) == FALSE)
            {
                return Result::Error("copyDirectoryRecursive: Failed to copy file");
            }
        }
    } while (::FindNextFileW(hFind, &findData));

    if (::GetLastError() != ERROR_NO_MORE_FILES)
    {
        return Result::Error("copyDirectoryRecursive: Failed to enumerate directory");
    }

    return Result(true);
}

SC::Result SC::FileSystemOperations::Internal::removeDirectoryRecursiveInternal(const wchar_t* path)
{
    // Prepare search pattern
    wchar_t searchPattern[MAX_PATH];
    if (::swprintf_s(searchPattern, MAX_PATH, L"%s\\*", path) == -1)
    {
        return Result::Error("removeDirectoryRecursive: Path too long");
    }

    WIN32_FIND_DATAW findData;

    HANDLE hFind = ::FindFirstFileW(searchPattern, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return Result::Error("removeDirectoryRecursive: Failed to enumerate directory");
    }
    auto deferClose = MakeDeferred([&]() { ::FindClose(hFind); });

    do
    {
        // Skip . and .. entries
        if (::wcscmp(findData.cFileName, L".") == 0 || ::wcscmp(findData.cFileName, L"..") == 0)
            continue;

        // Build full path
        wchar_t fullPath[MAX_PATH];
        if (swprintf_s(fullPath, MAX_PATH, L"%s\\%s", path, findData.cFileName) == -1)
        {
            return Result::Error("removeDirectoryRecursive: Path too long");
        }

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Recursively remove subdirectory
            SC_TRY(removeDirectoryRecursiveInternal(fullPath));
        }
        else
        {
            // Remove file
            if (::DeleteFileW(fullPath) == FALSE)
            {
                return Result::Error("removeDirectoryRecursive: Failed to delete file");
            }
        }
    } while (::FindNextFileW(hFind, &findData));

    if (::GetLastError() != ERROR_NO_MORE_FILES)
    {
        return Result::Error("removeDirectoryRecursive: Failed to enumerate directory");
    }

    // Remove the now-empty directory
    if (::RemoveDirectoryW(path) == FALSE)
    {
        return Result::Error("removeDirectoryRecursive: Failed to remove directory");
    }

    return Result(true);
}

SC::StringViewData SC::FileSystemOperations::getExecutablePath(StringPath& executablePath)
{
    // Use GetModuleFileNameW to get the executable path in UTF-16
    DWORD length = ::GetModuleFileNameW(nullptr, executablePath.path, static_cast<DWORD>(StringPath::MaxPath));
    if (length == 0 || length >= StringPath::MaxPath)
    {
        executablePath.length = 0;
        return {};
    }
    executablePath.length = length; // length does not include null terminator
    return executablePath;
}

SC::StringViewData SC::FileSystemOperations::getApplicationRootDirectory(StringPath& applicationRootDirectory)
{
    StringViewData exeView = getExecutablePath(applicationRootDirectory);
    if (exeView.isEmpty())
        return {};
    // Find the last path separator (either '\\' or '/')
    ssize_t lastSeparator = -1;
    for (size_t i = 0; i < applicationRootDirectory.length; ++i)
    {
        if (applicationRootDirectory.path[i] == L'\\' || applicationRootDirectory.path[i] == L'/')
            lastSeparator = static_cast<ssize_t>(i);
    }
    if (lastSeparator < 0)
    {
        // No separator found, return empty
        applicationRootDirectory.length = 0;
        ::memset(applicationRootDirectory.path, 0, StringPath::MaxPath * sizeof(wchar_t));
        return {};
    }
    const size_t copyLen = static_cast<size_t>(lastSeparator);

    applicationRootDirectory.path[copyLen] = 0;
    applicationRootDirectory.length        = copyLen;
    // null terminate the path
    ::memset(applicationRootDirectory.path + copyLen + 1, 0, (StringPath::MaxPath - copyLen - 1) * sizeof(wchar_t));
    return applicationRootDirectory;
}