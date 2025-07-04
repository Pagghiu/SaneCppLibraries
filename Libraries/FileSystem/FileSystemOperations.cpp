// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystem/FileSystemOperations.h"
#include "../Foundation/Deferred.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wchar.h>

struct SC::FileSystemOperations::Internal
{
    static Result validatePath(StringSpan path)
    {
        if (path.sizeInBytes() == 0)
            return Result::Error("Path is empty");
        if (path.getEncoding() != StringEncoding::Utf16)
            return Result::Error("Path is not native (UTF16)");
        return Result(true);
    }

    static Result copyFile(StringSpan source, StringSpan destination, FileSystemCopyFlags options,
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

SC::Result SC::FileSystemOperations::createSymbolicLink(StringSpan sourceFileOrDirectory, StringSpan linkFile)
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

SC::Result SC::FileSystemOperations::makeDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectory: Invalid path");
    SC_TRY_WIN32(::CreateDirectoryW(path.getNullTerminatedNative(), nullptr),
                 "makeDirectory: Failed to create directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::makeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectoryRecursive: Invalid path");
    const size_t pathLength = path.sizeInBytes() / sizeof(wchar_t);
    if (pathLength < 2)
        return Result::Error("makeDirectoryRecursive: Path is empty");
    wchar_t temp[MAX_PATH];
    // Copy path to temp, ensure null-terminated
    if (pathLength >= MAX_PATH)
        return Result::Error("makeDirectoryRecursive: Path too long");
    ::memcpy(temp, path.bytesWithoutTerminator(), pathLength * sizeof(wchar_t));
    temp[pathLength] = 0; // Ensure null-termination
    // Skip \\\\ or drive letter if present
    size_t idx = 0;
    if (pathLength >= 3)
    {
        if (temp[0] == L'\\' and temp[1] == L'\\')
        {
            // Skip until next backslash or forward slash
            for (idx = 3; idx < pathLength; ++idx)
            {
                if (temp[idx] == L'\\' or temp[idx] == L'/')
                {
                    idx += 1; // Skip the backslash or forward slash
                    break;
                }
            }
        }
        else if (temp[1] == L':' and (temp[2] == L'\\' or temp[2] == L'/'))
        {
            idx = 3; // Skip drive letter and colon and backslash
        }
    }
    // Iterate and create directories
    for (; idx < pathLength; ++idx)
    {
        if (temp[idx] == L'\\' or temp[idx] == L'/')
        {
            if (idx == 0)
                continue; // Skip root
            wchar_t old = temp[idx];
            temp[idx]   = 0;
            if (temp[0] != 0) // skip empty
            {
                if (!::CreateDirectoryW(temp, nullptr))
                {
                    DWORD err = ::GetLastError();
                    if (err != ERROR_ALREADY_EXISTS)
                        return Result::Error("makeDirectoryRecursive: Failed to create parent directory");
                }
            }
            temp[idx] = old;
        }
    }
    // Create the final directory
    if (!::CreateDirectoryW(temp, nullptr))
    {
        DWORD err = ::GetLastError();
        if (err != ERROR_ALREADY_EXISTS)
            return Result::Error("makeDirectoryRecursive: Failed to create directory");
    }
    return Result(true);
}

SC::Result SC::FileSystemOperations::exists(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "exists: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    return Result(res != INVALID_FILE_ATTRIBUTES);
}

SC::Result SC::FileSystemOperations::existsAndIsDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsDirectory: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

SC::Result SC::FileSystemOperations::existsAndIsFile(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsFile: Invalid path");
    const DWORD res = GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_DIRECTORY) == 0);
}

SC::Result SC::FileSystemOperations::existsAndIsLink(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsLink: Invalid path");
    const DWORD res = ::GetFileAttributesW(path.getNullTerminatedNative());
    if (res == INVALID_FILE_ATTRIBUTES)
        return Result(false);
    return Result((res & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
}

SC::Result SC::FileSystemOperations::removeEmptyDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeEmptyDirectory: Invalid path");
    SC_TRY_WIN32(::RemoveDirectoryW(path.getNullTerminatedNative()),
                 "removeEmptyDirectory: Failed to remove directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::moveDirectory(StringSpan source, StringSpan destination)
{
    SC_TRY_MSG(Internal::validatePath(source), "moveDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "moveDirectory: Invalid destination path");
    SC_TRY_WIN32(::MoveFileExW(source.getNullTerminatedNative(), destination.getNullTerminatedNative(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED),
                 "moveDirectory: Failed to move directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::removeFile(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeFile: Invalid path");
    SC_TRY_WIN32(::DeleteFileW(path.getNullTerminatedNative()), "removeFile: Failed to remove file");
    return Result(true);
}

SC::Result SC::FileSystemOperations::getFileStat(StringSpan path, FileSystemStat& fileStat)
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

SC::Result SC::FileSystemOperations::setLastModifiedTime(StringSpan path, Time::Realtime time)
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

SC::Result SC::FileSystemOperations::rename(StringSpan path, StringSpan newPath)
{
    SC_TRY_MSG(Internal::validatePath(path), "rename: Invalid path");
    SC_TRY_MSG(Internal::validatePath(newPath), "rename: Invalid new path");
    SC_TRY_WIN32(::MoveFileW(path.getNullTerminatedNative(), newPath.getNullTerminatedNative()),
                 "rename: Failed to rename");
    return Result(true);
}

SC::Result SC::FileSystemOperations::copyFile(StringSpan source, StringSpan destination, FileSystemCopyFlags flags)
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

SC::Result SC::FileSystemOperations::copyDirectory(StringSpan source, StringSpan destination, FileSystemCopyFlags flags)
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

SC::Result SC::FileSystemOperations::removeDirectoryRecursive(StringSpan path)
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

SC::StringSpan SC::FileSystemOperations::getExecutablePath(StringPath& executablePath)
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

SC::StringSpan SC::FileSystemOperations::getApplicationRootDirectory(StringPath& applicationRootDirectory)
{
    StringSpan exeView = getExecutablePath(applicationRootDirectory);
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
#else

#include <dirent.h>   // DIR, opendir, readdir, closedir
#include <errno.h>    // errno
#include <fcntl.h>    // AT_FDCWD
#include <limits.h>   // PATH_MAX
#include <math.h>     // round
#include <stdio.h>    // rename
#include <string.h>   // strcmp
#include <sys/stat.h> // mkdir
#include <unistd.h>   // rmdir
#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <copyfile.h>
#include <mach-o/dyld.h> // NSGetExecutablePath
#include <removefile.h>
#include <sys/attr.h>
#include <sys/clonefile.h>
#elif SC_PLATFORM_LINUX
#include <sys/sendfile.h>
#endif
struct SC::FileSystemOperations::Internal
{
    static Result validatePath(StringSpan path)
    {
        if (path.sizeInBytes() == 0)
            return Result::Error("Path is empty");
        if (path.getEncoding() == StringEncoding::Utf16)
            return Result::Error("Path is not native (UTF8)");
        return Result(true);
    }
    static Result copyFile(StringSpan source, StringSpan destination, FileSystemCopyFlags options,
                           bool isDirectory = false);
};

#define SC_TRY_POSIX(func, msg)                                                                                        \
    {                                                                                                                  \
                                                                                                                       \
        if (func != 0)                                                                                                 \
        {                                                                                                              \
            return Result::Error(msg);                                                                                 \
        }                                                                                                              \
    }

SC::Result SC::FileSystemOperations::createSymbolicLink(StringSpan sourceFileOrDirectory, StringSpan linkFile)
{
    SC_TRY_MSG(Internal::validatePath(sourceFileOrDirectory),
               "createSymbolicLink: Invalid source file or directory path");
    SC_TRY_MSG(Internal::validatePath(linkFile), "createSymbolicLink: Invalid link file path");
    SC_TRY_POSIX(::symlink(sourceFileOrDirectory.getNullTerminatedNative(), linkFile.getNullTerminatedNative()),
                 "createSymbolicLink: Failed to create symbolic link");
    return Result(true);
}

SC::Result SC::FileSystemOperations::makeDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectory: Invalid path");
    SC_TRY_POSIX(::mkdir(path.getNullTerminatedNative(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH),
                 "makeDirectory: Failed to create directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::makeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectoryRecursive: Invalid path");
    const size_t pathLength = path.sizeInBytes();
    char         temp[PATH_MAX];
    // Copy path to temp, ensure null-terminated
    if (pathLength >= PATH_MAX)
        return Result::Error("makeDirectoryRecursive: Path too long");
    ::memcpy(temp, path.bytesWithoutTerminator(), pathLength);
    // Iterate and create directories
    for (size_t idx = 0; idx < pathLength; ++idx)
    {
        if (temp[idx] == '/' or temp[idx] == '\\')
        {
            if (idx == 0)
                continue; // Skip root
            char old  = temp[idx];
            temp[idx] = 0;
            if (temp[0] != 0) // skip empty
            {
                if (::mkdir(temp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
                {
                    if (errno != EEXIST)
                        return Result::Error("makeDirectoryRecursive: Failed to create parent directory");
                }
            }
            temp[idx] = old;
        }
    }
    // Create the final directory
    if (::mkdir(path.getNullTerminatedNative(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
    {
        if (errno != EEXIST)
            return Result::Error("makeDirectoryRecursive: Failed to create directory");
    }
    return Result(true);
}

SC::Result SC::FileSystemOperations::exists(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "exists: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "exists: Failed to get file stats");
    return Result(true);
}

SC::Result SC::FileSystemOperations::existsAndIsDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsDirectory: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "existsAndIsDirectory: Failed to get file stats");
    return Result(S_ISDIR(path_stat.st_mode));
}

SC::Result SC::FileSystemOperations::existsAndIsFile(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsFile: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "existsAndIsFile: Failed to get file stats");
    return Result(S_ISREG(path_stat.st_mode));
}

SC::Result SC::FileSystemOperations::existsAndIsLink(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsLink: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "existsAndIsLink: Failed to get file stats");
    return Result(S_ISLNK(path_stat.st_mode));
}

SC::Result SC::FileSystemOperations::removeEmptyDirectory(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeEmptyDirectory: Invalid path");
    SC_TRY_POSIX(::rmdir(path.getNullTerminatedNative()), "removeEmptyDirectory: Failed to remove directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::moveDirectory(StringSpan source, StringSpan destination)
{
    SC_TRY_MSG(Internal::validatePath(source), "moveDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "moveDirectory: Invalid destination path");
    SC_TRY_POSIX(::rename(source.getNullTerminatedNative(), destination.getNullTerminatedNative()),
                 "moveDirectory: Failed to move directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::removeFile(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeFile: Invalid path");
    SC_TRY_POSIX(::remove(path.getNullTerminatedNative()), "removeFile: Failed to remove file");
    return Result(true);
}

SC::Result SC::FileSystemOperations::getFileStat(StringSpan path, FileSystemStat& fileStat)
{
    SC_TRY_MSG(Internal::validatePath(path), "getFileStat: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "getFileStat: Failed to get file stats");
    fileStat.fileSize = static_cast<size_t>(path_stat.st_size);
#if __APPLE__
    auto ts = path_stat.st_mtimespec;
#else
    auto ts = path_stat.st_mtim;
#endif
    fileStat.modifiedTime = Time::Realtime(static_cast<int64_t>(::round(ts.tv_nsec / 1.0e6) + ts.tv_sec * 1000));
    return Result(true);
}

SC::Result SC::FileSystemOperations::setLastModifiedTime(StringSpan path, Time::Realtime time)
{
    SC_TRY_MSG(Internal::validatePath(path), "setLastModifiedTime: Invalid path");
    struct timespec times[2];
    times[0].tv_sec  = time.getMillisecondsSinceEpoch() / 1000;
    times[0].tv_nsec = (time.getMillisecondsSinceEpoch() % 1000) * 1000 * 1000;
    times[1]         = times[0];

    SC_TRY_POSIX(::utimensat(AT_FDCWD, path.getNullTerminatedNative(), times, 0),
                 "setLastModifiedTime: Failed to set last modified time");
    return Result(true);
}

SC::Result SC::FileSystemOperations::rename(StringSpan path, StringSpan newPath)
{
    SC_TRY_MSG(Internal::validatePath(path), "rename: Invalid path");
    SC_TRY_MSG(Internal::validatePath(newPath), "rename: Invalid new path");
    SC_TRY_POSIX(::rename(path.getNullTerminatedNative(), newPath.getNullTerminatedNative()),
                 "rename: Failed to rename");
    return Result(true);
}

SC::Result SC::FileSystemOperations::copyFile(StringSpan srcPath, StringSpan destPath, FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(srcPath), "copyFile: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destPath), "copyFile: Invalid destination path");

    return Result(Internal::copyFile(srcPath, destPath, flags, false));
}

SC::Result SC::FileSystemOperations::copyDirectory(StringSpan srcPath, StringSpan destPath, FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(srcPath), "copyDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destPath), "copyDirectory: Invalid destination path");
    return Result(Internal::copyFile(srcPath, destPath, flags, true));
}

#if __APPLE__
SC::Result SC::FileSystemOperations::Internal::copyFile(StringSpan source, StringSpan destination,
                                                        FileSystemCopyFlags options, bool isDirectory)
{
    const char* sourceFile      = source.getNullTerminatedNative();
    const char* destinationFile = destination.getNullTerminatedNative();

    // Try clonefile and fallback to copyfile in case it fails with ENOTSUP or EXDEV
    // https://www.manpagez.com/man/2/clonefile/
    // https://www.manpagez.com/man/3/copyfile/
    if (options.useCloneIfSupported)
    {
        int cloneRes = ::clonefile(sourceFile, destinationFile, CLONE_NOFOLLOW | CLONE_NOOWNERCOPY);
        if (cloneRes != 0)
        {
            if ((errno == EEXIST) and options.overwrite)
            {
                // TODO: We should probably renaming instead of deleting...and eventually rollback on failure
                if (isDirectory)
                {
                    auto removeState = ::removefile_state_alloc();
                    auto removeFree  = MakeDeferred([&] { ::removefile_state_free(removeState); });
                    SC_TRY_POSIX(::removefile(destinationFile, removeState, REMOVEFILE_RECURSIVE),
                                 "copyFile: Failed to remove file (removeRes == 0)");
                }
                else
                {
                    SC_TRY_POSIX(::remove(destinationFile), "copyFile: Failed to remove file");
                }
                cloneRes = ::clonefile(sourceFile, destinationFile, CLONE_NOFOLLOW | CLONE_NOOWNERCOPY);
            }
        }
        if (cloneRes == 0)
        {
            return Result(true);
        }
        else if (errno != ENOTSUP and errno != EXDEV)
        {
            // We only fallback in case of ENOTSUP and EXDEV (cross-device link)
            return Result::Error("copyFile: Failed to clone file (errno != ENOTSUP and errno != EXDEV)");
        }
    }

    uint32_t flags = COPYFILE_ALL; // We should not use COPYFILE_CLONE_FORCE as clonefile just failed
    if (options.overwrite)
    {
        flags |= COPYFILE_UNLINK;
    }
    if (isDirectory)
    {
        flags |= COPYFILE_RECURSIVE;
    }
    // TODO: Should define flags to decide if to follow symlinks on source and destination
    auto copyState = ::copyfile_state_alloc();
    auto copyFree  = MakeDeferred([&] { ::copyfile_state_free(copyState); });
    SC_TRY_POSIX(::copyfile(sourceFile, destinationFile, copyState, flags), "copyFile: Failed to copy file");
    return Result(true);
}

SC::Result SC::FileSystemOperations::removeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeDirectoryRecursive: Invalid path");
    auto state     = ::removefile_state_alloc();
    auto stateFree = MakeDeferred([&] { ::removefile_state_free(state); });
    SC_TRY_POSIX(::removefile(path.getNullTerminatedNative(), state, REMOVEFILE_RECURSIVE),
                 "removeDirectoryRecursive: Failed to remove directory");
    return Result(true);
}

SC::StringSpan SC::FileSystemOperations::getExecutablePath(StringPath& executablePath)
{
    uint32_t executableLength = static_cast<uint32_t>(StringPath::MaxPath);
    if (::_NSGetExecutablePath(executablePath.path, &executableLength) == 0)
    {
        executablePath.length = ::strlen(executablePath.path);
        return executablePath;
    }
    return {};
}

SC::StringSpan SC::FileSystemOperations::getApplicationRootDirectory(StringPath& applicationRootDirectory)
{
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle != nullptr)
    {
        CFURLRef bundleURL = CFBundleCopyBundleURL(mainBundle);
        if (bundleURL != nullptr)
        {
            if (CFURLGetFileSystemRepresentation(
                    bundleURL, true, reinterpret_cast<uint8_t*>(applicationRootDirectory.path), StringPath::MaxPath))
            {
                applicationRootDirectory.length = ::strlen(applicationRootDirectory.path);
                CFRelease(bundleURL);
                return applicationRootDirectory;
            }
            CFRelease(bundleURL);
        }
    }
    return {};
}

#else
SC::Result SC::FileSystemOperations::Internal::copyFile(StringSpan source, StringSpan destination,
                                                        FileSystemCopyFlags options, bool isDirectory)
{
    if (isDirectory)
    {
        // For directories, first check if source exists and is a directory
        SC_TRY_MSG(existsAndIsDirectory(source), "copyFile: Source path is not a directory");

        // Create destination directory if it doesn't exist
        if (not existsAndIsDirectory(destination))
        {
            SC_TRY(makeDirectory(destination));
        }
        else if (not options.overwrite)
        {
            return Result::Error("copyFile: Destination directory already exists and overwrite is not enabled");
        }

        // Open source directory
        DIR* dir = ::opendir(source.getNullTerminatedNative());
        if (dir == nullptr)
        {
            return Result::Error("copyFile: Failed to open source directory");
        }
        auto closeDir = MakeDeferred([&] { ::closedir(dir); });

        // Buffer for full path construction
        char fullSourcePath[PATH_MAX];
        char fullDestPath[PATH_MAX];

        struct dirent* entry;

        // Iterate through directory entries
        while ((entry = ::readdir(dir)) != nullptr)
        {
            // Skip . and .. entries
            if (::strcmp(entry->d_name, ".") == 0 or ::strcmp(entry->d_name, "..") == 0)
                continue;

            // Construct full paths
            if (::snprintf(fullSourcePath, sizeof(fullSourcePath), "%s/%s", source.getNullTerminatedNative(),
                           entry->d_name) >= static_cast<int>(sizeof(fullSourcePath)) or
                ::snprintf(fullDestPath, sizeof(fullDestPath), "%s/%s", destination.getNullTerminatedNative(),
                           entry->d_name) >= static_cast<int>(sizeof(fullDestPath)))
            {
                return Result::Error("copyFile: Path too long");
            }

            struct stat statbuf;
            if (::lstat(fullSourcePath, &statbuf) != 0)
            {
                return Result::Error("copyFile: Failed to get file stats");
            }

            // Recursively copy subdirectories and files
            SC_TRY(copyFile(StringSpan(fullSourcePath), StringSpan(fullDestPath), options, S_ISDIR(statbuf.st_mode)));
        }

        return Result(true);
    }

    // Original file copying logic for non-directory files
    if (not options.overwrite and existsAndIsFile(destination))
    {
        return Result::Error("copyFile: Failed to copy file (destination file already exists)");
    }
    int inputDescriptor = ::open(source.getNullTerminatedNative(), O_RDONLY);
    if (inputDescriptor < 0)
    {
        return Result::Error("copyFile: Failed to open source file");
    }
    auto closeInput = MakeDeferred([&] { ::close(inputDescriptor); });

    struct stat inputStat;

    SC_TRY_POSIX(::fstat(inputDescriptor, &inputStat), "copyFile: Failed to get file stats");

    int outputDescriptor = ::open(destination.getNullTerminatedNative(), O_WRONLY | O_CREAT | O_TRUNC,
                                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (outputDescriptor < 0)
    {
        return Result::Error("copyFile: Failed to open destination file");
    }
    auto closeOutput = MakeDeferred([&] { ::close(outputDescriptor); });

    const int sendRes = ::sendfile(outputDescriptor, inputDescriptor, nullptr, inputStat.st_size);
    if (sendRes < 0)
    {
        // Sendfile failed, fallback to traditional read/write
        constexpr size_t bufferSize = 4096;

        char buffer[bufferSize];

        ssize_t bytesRead;
        while ((bytesRead = ::read(inputDescriptor, buffer, bufferSize)) > 0)
        {
            if (::write(outputDescriptor, buffer, static_cast<size_t>(bytesRead)) < 0)
            {
                return Result::Error("copyFile: Failed to write to destination file");
            }
        }

        if (bytesRead < 0)
        {
            return Result::Error("copyFile: Failed to read from source file");
        }
    }
    return Result(true);
}

SC::Result SC::FileSystemOperations::removeDirectoryRecursive(StringSpan path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeDirectoryRecursive: Invalid path");

    // Open directory
    DIR* dir = ::opendir(path.getNullTerminatedNative());
    if (dir == nullptr)
    {
        return Result::Error("removeDirectoryRecursive: Failed to open directory");
    }
    auto closeDir = MakeDeferred([&] { ::closedir(dir); });

    // Buffer for full path construction
    char fullPath[PATH_MAX];

    struct dirent* entry;

    // Iterate through directory entries
    while ((entry = ::readdir(dir)) != nullptr)
    {
        // Skip . and .. entries
        if (::strcmp(entry->d_name, ".") == 0 or ::strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct full path
        if (::snprintf(fullPath, sizeof(fullPath), "%s/%s", path.getNullTerminatedNative(), entry->d_name) >=
            static_cast<int>(sizeof(fullPath)))
        {
            return Result::Error("removeDirectoryRecursive: Path too long");
        }

        struct stat statbuf;
        if (::lstat(fullPath, &statbuf) != 0)
        {
            return Result::Error("removeDirectoryRecursive: Failed to get file stats");
        }

        if (S_ISDIR(statbuf.st_mode))
        {
            // Recursively remove subdirectory
            SC_TRY(removeDirectoryRecursive(fullPath));
        }
        else
        {
            // Remove file
            if (::unlink(fullPath) != 0)
            {
                return Result::Error("removeDirectoryRecursive: Failed to remove file");
            }
        }
    }

    // Remove the now-empty directory
    if (::rmdir(path.getNullTerminatedNative()) != 0)
    {
        return Result::Error("removeDirectoryRecursive: Failed to remove directory");
    }

    return Result(true);
}

SC::StringSpan SC::FileSystemOperations::getExecutablePath(StringPath& executablePath)
{
    const int pathLength = ::readlink("/proc/self/exe", executablePath.path, StringPath::MaxPath);
    if (pathLength > 0)
    {
        executablePath.length = static_cast<size_t>(pathLength);
        return executablePath;
    }
    return {};
}

SC::StringSpan SC::FileSystemOperations::getApplicationRootDirectory(StringPath& applicationRootDirectory)
{
    StringSpan executablePath = getExecutablePath(applicationRootDirectory);
    if (!executablePath.isEmpty())
    {
        // Get the directory part of the executable path
        const char* lastSlash = ::strrchr(applicationRootDirectory.path, '/');
        if (lastSlash != nullptr)
        {
            applicationRootDirectory.length = static_cast<size_t>(lastSlash - applicationRootDirectory.path);
            // Null-terminate the path
            ::memset(applicationRootDirectory.path + applicationRootDirectory.length, 0,
                     StringPath::MaxPath - applicationRootDirectory.length);
            return applicationRootDirectory;
        }
    }
    return {};
}

#endif

#endif
