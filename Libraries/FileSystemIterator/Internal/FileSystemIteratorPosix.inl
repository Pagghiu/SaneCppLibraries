// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystemIterator.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if SC_PLATFORM_LINUX
#include <linux/limits.h> // For PATH_MAX on Linux
#else
#include <sys/syslimits.h> // For PATH_MAX on Apple and other POSIX systems
#endif

namespace SC
{
SC::Result getErrorCode(int errorCode); // defined in FileSystemInternalPosix.inl
}

struct SC::FileSystemIterator::Internal
{
    static Result initFolderState(FolderState& entry, int fd)
    {
        entry.fileDescriptor = fd;
        if (entry.fileDescriptor == -1)
        {
            return getErrorCode(errno);
        }
        entry.dirEnumerator = ::fdopendir(entry.fileDescriptor);
        if (entry.dirEnumerator == nullptr)
        {
            ::close(entry.fileDescriptor);
            entry.fileDescriptor = -1; // Reset file descriptor on error
            return getErrorCode(errno);
        }
        return Result(true);
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

SC::Result SC::FileSystemIterator::init(StringSpan directory, Span<FolderState> recursiveEntries)
{
    Internal::destroy(recurseStack);
    recurseStack.recursiveEntries = recursiveEntries;
    recurseStack.currentEntry     = -1;

    FolderState entry;
    if (directory.getEncoding() == StringEncoding::Utf16)
    {
        return Result::Error("FileSystemIterator on Posix does not support UTF16 encoded paths");
    }

    SC_TRY_MSG(currentPath.path.assign(directory), "Directory path is too long");

    entry.textLengthInBytes = directory.sizeInBytes();

    SC_TRY_MSG(recurseStack.push_back(entry), "Exceeding maximum number of recursive entries");
    const int fd = ::open(currentPath.path.buffer, O_DIRECTORY);
    SC_TRY(Internal::initFolderState(recurseStack.back(), fd));
    return Result(true);
}

SC::Result SC::FileSystemIterator::enumerateNextInternal(Entry& entry)
{
    if (recurseStack.isEmpty())
        return Result::Error("Forgot to call init");

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
                return Result::Error("Iteration Finished");
            }
            parent = recurseStack.back();

            currentPath.path.length = parent.textLengthInBytes;
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
    currentPath.path.length = recurseStack.back().textLengthInBytes;

    SC_TRY_MSG(currentPath.path.append("/"), "Insufficient space on current path string");
    SC_TRY_MSG(currentPath.path.append(entry.name), "Insufficient space on current path string");

    entry.path  = currentPath.path.view();
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
    return Result(true);
}

SC::Result SC::FileSystemIterator::recurseSubdirectoryInternal(Entry& entry)
{
    FolderState newParent;
    currentPath.path.length = recurseStack.back().textLengthInBytes;
    SC_TRY_MSG(currentPath.path.append("/"), "Directory path is too long");
    SC_TRY_MSG(currentPath.path.append(entry.name), "Directory path is too long");
    newParent.textLengthInBytes = currentPath.path.length;
    SC_TRY(entry.name.isNullTerminated());
    SC_TRY_MSG(recurseStack.push_back(newParent), "Exceeding maximum number of recursive entries");
    const int fd = ::openat(entry.parentFileDescriptor, entry.name.getNullTerminatedNative(), O_DIRECTORY);
    SC_TRY(Internal::initFolderState(recurseStack.back(), fd));
    return Result(true);
}
