// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Deferred.h"
#include "../FileSystemOperations.h"
#include <dirent.h>   // DIR, opendir, readdir, closedir
#include <fcntl.h>    // AT_FDCWD
#include <limits.h>   // PATH_MAX
#include <math.h>     // round
#include <stdio.h>    // rename
#include <string.h>   // strcmp
#include <sys/stat.h> // mkdir
#include <unistd.h>   // rmdir
#if __APPLE__
#include <copyfile.h>
#include <errno.h> // errno
#include <removefile.h>
#include <sys/attr.h>
#include <sys/clonefile.h>
#elif SC_PLATFORM_LINUX
#include <sys/sendfile.h>
#endif
struct SC::FileSystemOperations::Internal
{
    static Result validatePath(StringViewData path)
    {
        if (path.sizeInBytes() == 0)
            return Result::Error("Path is empty");
        if (path.getEncoding() == StringEncoding::Utf16)
            return Result::Error("Path is not native (UTF8)");
        return Result(true);
    }
    static Result copyFile(StringViewData source, StringViewData destination, FileSystemCopyFlags options,
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

SC::Result SC::FileSystemOperations::createSymbolicLink(StringViewData sourceFileOrDirectory, StringViewData linkFile)
{
    SC_TRY_MSG(Internal::validatePath(sourceFileOrDirectory),
               "createSymbolicLink: Invalid source file or directory path");
    SC_TRY_MSG(Internal::validatePath(linkFile), "createSymbolicLink: Invalid link file path");
    SC_TRY_POSIX(::symlink(sourceFileOrDirectory.getNullTerminatedNative(), linkFile.getNullTerminatedNative()),
                 "createSymbolicLink: Failed to create symbolic link");
    return Result(true);
}

SC::Result SC::FileSystemOperations::makeDirectory(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "makeDirectory: Invalid path");
    SC_TRY_POSIX(::mkdir(path.getNullTerminatedNative(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH),
                 "makeDirectory: Failed to create directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::exists(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "exists: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "exists: Failed to get file stats");
    return Result(true);
}

SC::Result SC::FileSystemOperations::existsAndIsDirectory(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsDirectory: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "existsAndIsDirectory: Failed to get file stats");
    return Result(S_ISDIR(path_stat.st_mode));
}

SC::Result SC::FileSystemOperations::existsAndIsFile(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsFile: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "existsAndIsFile: Failed to get file stats");
    return Result(S_ISREG(path_stat.st_mode));
}

SC::Result SC::FileSystemOperations::existsAndIsLink(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "existsAndIsLink: Invalid path");
    struct stat path_stat;
    SC_TRY_POSIX(::stat(path.getNullTerminatedNative(), &path_stat), "existsAndIsLink: Failed to get file stats");
    return Result(S_ISLNK(path_stat.st_mode));
}

SC::Result SC::FileSystemOperations::removeEmptyDirectory(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeEmptyDirectory: Invalid path");
    SC_TRY_POSIX(::rmdir(path.getNullTerminatedNative()), "removeEmptyDirectory: Failed to remove directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::moveDirectory(StringViewData source, StringViewData destination)
{
    SC_TRY_MSG(Internal::validatePath(source), "moveDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destination), "moveDirectory: Invalid destination path");
    SC_TRY_POSIX(::rename(source.getNullTerminatedNative(), destination.getNullTerminatedNative()),
                 "moveDirectory: Failed to move directory");
    return Result(true);
}

SC::Result SC::FileSystemOperations::removeFile(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeFile: Invalid path");
    SC_TRY_POSIX(::remove(path.getNullTerminatedNative()), "removeFile: Failed to remove file");
    return Result(true);
}

SC::Result SC::FileSystemOperations::getFileStat(StringViewData path, FileSystemStat& fileStat)
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

SC::Result SC::FileSystemOperations::setLastModifiedTime(StringViewData path, Time::Realtime time)
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

SC::Result SC::FileSystemOperations::rename(StringViewData path, StringViewData newPath)
{
    SC_TRY_MSG(Internal::validatePath(path), "rename: Invalid path");
    SC_TRY_MSG(Internal::validatePath(newPath), "rename: Invalid new path");
    SC_TRY_POSIX(::rename(path.getNullTerminatedNative(), newPath.getNullTerminatedNative()),
                 "rename: Failed to rename");
    return Result(true);
}

SC::Result SC::FileSystemOperations::copyFile(StringViewData srcPath, StringViewData destPath,
                                              FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(srcPath), "copyFile: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destPath), "copyFile: Invalid destination path");

    return Result(Internal::copyFile(srcPath, destPath, flags, false));
}

SC::Result SC::FileSystemOperations::copyDirectory(StringViewData srcPath, StringViewData destPath,
                                                   FileSystemCopyFlags flags)
{
    SC_TRY_MSG(Internal::validatePath(srcPath), "copyDirectory: Invalid source path");
    SC_TRY_MSG(Internal::validatePath(destPath), "copyDirectory: Invalid destination path");
    return Result(Internal::copyFile(srcPath, destPath, flags, true));
}

#if __APPLE__
SC::Result SC::FileSystemOperations::Internal::copyFile(StringViewData source, StringViewData destination,
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

SC::Result SC::FileSystemOperations::removeDirectoryRecursive(StringViewData path)
{
    SC_TRY_MSG(Internal::validatePath(path), "removeDirectoryRecursive: Invalid path");
    auto state     = ::removefile_state_alloc();
    auto stateFree = MakeDeferred([&] { ::removefile_state_free(state); });
    SC_TRY_POSIX(::removefile(path.getNullTerminatedNative(), state, REMOVEFILE_RECURSIVE),
                 "removeDirectoryRecursive: Failed to remove directory");
    return Result(true);
}
#else
SC::Result SC::FileSystemOperations::Internal::copyFile(StringViewData source, StringViewData destination,
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
            SC_TRY(copyFile(StringViewData(fullSourcePath), StringViewData(fullDestPath), options,
                            S_ISDIR(statbuf.st_mode)));
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

SC::Result SC::FileSystemOperations::removeDirectoryRecursive(StringViewData path)
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
#endif
