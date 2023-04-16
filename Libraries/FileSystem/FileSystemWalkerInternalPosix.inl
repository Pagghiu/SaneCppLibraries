// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
// clang-format off
#include "FileSystemWalker.h"
#include "../Foundation/StringConverter.h"
#include "../Foundation/SmallVector.h"

// clang-format on
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "FileSystemInternalPosix.inl"

struct SC::FileSystemWalker::Internal
{
    struct StackEntry
    {
        DIR*   dirEnumerator     = nullptr;
        size_t textLengthInBytes = 0;
        int    fileDescriptor    = -1; // TODO: use actual filedescriptor class here
        bool   gotDot1           = false;
        bool   gotDot2           = false;

        ReturnCode init(int fd)
        {
            fileDescriptor = fd;
            if (fileDescriptor == -1)
            {
                return getErrorCode(errno);
            }
            dirEnumerator = fdopendir(fileDescriptor);
            if (dirEnumerator == nullptr)
            {
                return getErrorCode(errno);
            }
            return true;
        }
        void close()
        {
            if (fileDescriptor != -1)
            {
                ::close(fileDescriptor);
            }
        }
    };
    SmallVector<StackEntry, 64> recurseStack;
    StringNative<512>           currentPathString = StringEncoding::Native;
    Internal() {}

    ~Internal()
    {
        while (not recurseStack.isEmpty())
        {
            recurseStack.back().close();
            SC_TRUST_RESULT(recurseStack.pop_back());
        }
    }

    [[nodiscard]] ReturnCode init(StringView directory)
    {
        StringConverter currentPath(currentPathString);
        currentPath.clear();
        SC_TRY_IF(currentPath.appendNullTerminated(directory));
        StackEntry entry;
        entry.textLengthInBytes = currentPathString.view().sizeInBytesIncludingTerminator();
        SC_TRY_IF(entry.init(open(currentPathString.view().getNullTerminatedNative(), O_DIRECTORY)));
        SC_TRY_IF(recurseStack.push_back(move(entry)));
        return true;
    }

    ReturnCode enumerateNext(Entry& entry, const Options& options)
    {
        StringConverter currentPath(currentPathString);
        if (recurseStack.isEmpty())
            return "Forgot to call init"_a8;
        StackEntry&    parent = recurseStack.back();
        struct dirent* item;
        for (;;)
        {
            item = readdir(parent.dirEnumerator);
            if (item == nullptr)
            {
                recurseStack.back().close();
                SC_TRY_IF(recurseStack.pop_back());
                if (recurseStack.isEmpty())
                {
                    entry.parentFileDescriptor.handle.detach();
                    return "Iteration Finished"_a8;
                }
                parent = recurseStack.back();
                SC_TRY_IF(currentPath.setTextLengthInBytesIncludingTerminator(parent.textLengthInBytes));
                continue;
            }
            if (not(parent.gotDot1 and parent.gotDot2))
            {
                const bool isDot1 = strncmp(item->d_name, ".", 1) == 0;
                const bool isDot2 = strncmp(item->d_name, "..", 2) == 0;
                if (isDot1)
                    parent.gotDot1 = true;
                if (isDot2)
                    parent.gotDot2 = true;
            }
            else
            {
                break;
            }
        }
        entry.name = StringView(item->d_name, item->d_namlen, true, StringEncoding::Utf8);
        SC_TRY_IF(currentPath.setTextLengthInBytesIncludingTerminator(recurseStack.back().textLengthInBytes));
        SC_TRY_IF(currentPath.appendNullTerminated("/"_u8));
        SC_TRY_IF(currentPath.appendNullTerminated(entry.name));
        entry.path  = currentPathString.view();
        entry.level = static_cast<decltype(entry.level)>(recurseStack.size() - 1);
        entry.parentFileDescriptor.handle.detach();
        SC_TRY_IF(entry.parentFileDescriptor.handle.assign(parent.fileDescriptor));
        if (item->d_type == DT_DIR)
        {
            entry.type = Type::Directory;
            if (options.recursive)
            {
                SC_TRY_IF(recurseSubdirectory(entry));
            }
        }
        else
        {
            entry.type = Type::File;
        }
        return true;
    }

    [[nodiscard]] ReturnCode recurseSubdirectory(Entry& entry)
    {
        StringConverter currentPath(currentPathString);
        StackEntry      newParent;
        SC_TRY_IF(currentPath.setTextLengthInBytesIncludingTerminator(recurseStack.back().textLengthInBytes));
        SC_TRY_IF(currentPath.appendNullTerminated("/"_u8));
        SC_TRY_IF(currentPath.appendNullTerminated(entry.name));
        newParent.textLengthInBytes = currentPathString.view().sizeInBytesIncludingTerminator();
        FileDescriptorNative handle;
        SC_TRY_IF(entry.parentFileDescriptor.handle.get(handle, ReturnCode("recurseSubdirectory - InvalidHandle"_a8)));
        SC_TRY_IF(newParent.init(openat(handle, entry.name.bytesIncludingTerminator(), O_DIRECTORY)));
        SC_TRY_IF(recurseStack.push_back(newParent))
        return true;
    }
};
