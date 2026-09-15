// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../FileSystemIterator/FileSystemIterator.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__has_include)
#if __has_include(<linux/limits.h>)
#include <linux/limits.h> // For PATH_MAX on Linux
#elif __has_include(<limits.h>)
#include <limits.h> // For PATH_MAX when Linux UAPI headers are unavailable
#else
#include <sys/syslimits.h> // For PATH_MAX on Apple and other POSIX systems
#endif
#else
#include <limits.h>
#endif

struct SC::FileSystemIterator::Internal
{

    static ResultFileSystemIterator nativeError(FileSystemIteratorError error, FileSystemIteratorErrorDetail detail,
                                                int errorCode, uint32_t depth)
    {
        return ResultFileSystemIterator(error, detail, static_cast<uint32_t>(errorCode), depth);
    }
    static ResultFileSystemIterator initFolderState(FolderState& entry, int fd, uint32_t depth)
    {
        entry.fileDescriptor = fd;
        if (entry.fileDescriptor == -1)
        {
            return nativeError(FileSystemIteratorError::OpenDirectoryFailed, FileSystemIteratorErrorDetail::PosixOpen,
                               errno, depth);
        }
        entry.dirEnumerator = ::fdopendir(entry.fileDescriptor);
        if (entry.dirEnumerator == nullptr)
        {
            const int nativeError = errno;
            ::close(entry.fileDescriptor);
            entry.fileDescriptor = -1; // Reset file descriptor on error
            return Internal::nativeError(FileSystemIteratorError::OpenDirectoryFailed,
                                         FileSystemIteratorErrorDetail::PosixFdOpenDir, nativeError, depth);
        }
        return ResultFileSystemIterator(true);
    }

    static void closeFolderState(FolderState& entry)
    {
        if (entry.dirEnumerator != nullptr)
        {
            ::closedir(static_cast<DIR*>(entry.dirEnumerator));
        }
        if (entry.fileDescriptor != -1)
        {
            ::close(entry.fileDescriptor);
        }
    }

    static void destroy(RecurseStack& recurseStack)
    {
        while (not recurseStack.isEmpty())
        {
            closeFolderState(recurseStack.back());
            recurseStack.pop_back();
        }
    }
};

SC::ResultFileSystemIterator SC::FileSystemIterator::initInternal(StringSpan        directory,
                                                                  Span<FolderState> recursiveEntries)
{
    recurseStack.recursiveEntries = recursiveEntries;
    recurseStack.currentEntry     = -1;

    FolderState entry;
    if (directory.getEncoding() == StringEncoding::Utf16)
    {
        return ResultFileSystemIterator(FileSystemIteratorError::UnsupportedPathEncoding);
    }

    if (not currentPath.assign(directory))
        return ResultFileSystemIterator(FileSystemIteratorError::PathTooLong, FileSystemIteratorErrorDetail::BuildPath,
                                        0, 0);

    entry.textLengthInBytes = directory.sizeInBytes();

    SC_TRY(recurseStack.push_back(entry));
    const int fd = ::open(currentPath.view().bytesIncludingTerminator(), O_DIRECTORY);
    SC_TRY(Internal::initFolderState(recurseStack.back(), fd, 0));
    return ResultFileSystemIterator(true);
}

SC::ResultFileSystemIterator SC::FileSystemIterator::enumerateNextInternal(Entry& entry, bool& hasEntry)
{
    hasEntry = false;
    if (recurseStack.isEmpty())
        return ResultFileSystemIterator(FileSystemIteratorError::NotInitialized);

    FolderState&   parent = recurseStack.back();
    struct dirent* item;
    for (;;)
    {
        item = ::readdir(static_cast<DIR*>(parent.dirEnumerator));
        if (item == nullptr)
        {
            Internal::closeFolderState(recurseStack.back());
            recurseStack.pop_back();
            if (recurseStack.isEmpty())
            {
                return ResultFileSystemIterator(true);
            }
            parent = recurseStack.back();

            (void)currentPath.resize(parent.textLengthInBytes);
            continue;
        }
        if (not(parent.gotDot1 and parent.gotDot2))
        {
            if (::strcmp(item->d_name, "..") == 0)
            {
                parent.gotDot2 = true;
                continue;
            }
            else if (::strcmp(item->d_name, ".") == 0)
            {
                parent.gotDot1 = true;
                continue;
            }
        }
        break;
    }
#if SC_PLATFORM_APPLE
    entry.name = StringSpan({item->d_name, item->d_namlen}, true, StringEncoding::Utf8);
#else
    entry.name = StringSpan({item->d_name, strlen(item->d_name)}, true, StringEncoding::Utf8);
#endif
    (void)currentPath.resize(recurseStack.back().textLengthInBytes);

    if (not currentPath.append("/") or not currentPath.append(entry.name))
        return ResultFileSystemIterator(FileSystemIteratorError::PathTooLong, FileSystemIteratorErrorDetail::BuildPath,
                                        0, static_cast<uint32_t>(recurseStack.size() - 1));

    entry.path  = currentPath.view();
    entry.level = static_cast<decltype(entry.level)>(recurseStack.size() - 1);

    entry.parentFileDescriptor = parent.fileDescriptor;
    if (item->d_type == DT_DIR)
    {
        entry.type = Type::Directory;
        if (options.recursive)
        {
            SC_TRY(recurseSubdirectoryInternal(entry));
        }
    }
    else
    {
        entry.type = Type::File;
    }
    hasEntry = true;
    return ResultFileSystemIterator(true);
}

SC::ResultFileSystemIterator SC::FileSystemIterator::recurseSubdirectoryInternal(Entry& entry)
{
    FolderState newParent;
    (void)currentPath.resize(recurseStack.back().textLengthInBytes);
    if (not currentPath.append("/") or not currentPath.append(entry.name))
        return ResultFileSystemIterator(FileSystemIteratorError::PathTooLong, FileSystemIteratorErrorDetail::BuildPath,
                                        0, static_cast<uint32_t>(recurseStack.size()));
    newParent.textLengthInBytes = currentPath.view().sizeInBytes();
    if (not entry.name.isNullTerminated())
        return ResultFileSystemIterator(FileSystemIteratorError::InvalidRecursionState,
                                        FileSystemIteratorErrorDetail::PushRecursionState, 0,
                                        static_cast<uint32_t>(recurseStack.size()));
    SC_TRY(recurseStack.push_back(newParent));
    const int fd = ::openat(entry.parentFileDescriptor, entry.name.getNullTerminatedNative(), O_DIRECTORY);
    SC_TRY(Internal::initFolderState(recurseStack.back(), fd, static_cast<uint32_t>(recurseStack.size() - 1)));
    return ResultFileSystemIterator(true);
}
