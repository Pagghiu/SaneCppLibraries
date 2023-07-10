// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Foundation/SmallVector.h"
#include "../Foundation/String.h"
#include "../Foundation/StringBuilder.h"
#include "../Foundation/StringConverter.h"

#include "FileSystemWalker.h"

struct SC::FileSystemWalker::Internal
{
    struct StackEntry
    {
        HANDLE fileDescriptor    = INVALID_HANDLE_VALUE;
        size_t textLengthInBytes = 0;
        bool   gotDot1           = false;
        bool   gotDot2           = false;

        ReturnCode init(const wchar_t* path, WIN32_FIND_DATAW& dirEnumerator)
        {
            fileDescriptor = FindFirstFileW(path, &dirEnumerator);
            if (INVALID_HANDLE_VALUE == fileDescriptor)
            {
                return "FindFirstFileW failed"_a8;
            }
            return true;
        }

        void close()
        {
            if (fileDescriptor != INVALID_HANDLE_VALUE)
            {
                FindClose(fileDescriptor);
            }
        }
    };
    WIN32_FIND_DATAW            dirEnumerator;
    bool                        expectDotDirectories = true;
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
        StackEntry entry;
        SC_TRY_IF(currentPath.appendNullTerminated(directory));
        entry.textLengthInBytes = currentPathString.view().sizeInBytesIncludingTerminator();
        SC_TRY_IF(currentPath.appendNullTerminated(L"\\*.*"));
        SC_TRY_IF(entry.init(currentPathString.view().getNullTerminatedNative(), dirEnumerator));
        SC_TRY_IF(recurseStack.push_back(entry));
        SC_TRY_IF(currentPath.setTextLengthInBytesIncludingTerminator(recurseStack.back().textLengthInBytes));
        expectDotDirectories = true;
        return true;
    }

    [[nodiscard]] ReturnCode enumerateNext(Entry& entry, const Options& options)
    {
        StringConverter currentPath(currentPathString);
        StackEntry&     parent = recurseStack.back();
        for (;;)
        {
            if (not expectDotDirectories)
            {
                if (not FindNextFileW(parent.fileDescriptor, &dirEnumerator))
                {
                    recurseStack.back().close();
                    SC_TRY_IF(recurseStack.pop_back());
                    if (recurseStack.isEmpty())
                        return "Iteration Finished"_a8;
                    parent = recurseStack.back();
                    SC_TRY_IF(
                        currentPath.setTextLengthInBytesIncludingTerminator(recurseStack.back().textLengthInBytes));
                    continue;
                }
            }
            expectDotDirectories = false;
            if (not(parent.gotDot1 and parent.gotDot2))
            {
                const bool isDot1 = wcsncmp(dirEnumerator.cFileName, L".", 1) == 0;
                const bool isDot2 = wcsncmp(dirEnumerator.cFileName, L"..", 2) == 0;
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

        entry.name = StringView(dirEnumerator.cFileName, wcsnlen_s(dirEnumerator.cFileName, MAX_PATH) * sizeof(wchar_t),
                                true, StringEncoding::Utf16);
        SC_TRY_IF(currentPath.setTextLengthInBytesIncludingTerminator(recurseStack.back().textLengthInBytes));
        SC_TRY_IF(currentPath.appendNullTerminated(options.forwardSlashes ? L"/" : L"\\"));
        SC_TRY_IF(currentPath.appendNullTerminated(entry.name));
        if (options.forwardSlashes)
        {
            // TODO: Probably this allocation could be avoided using StringBuilder instead of StringConverter
            auto copy = currentPathString;
            currentPathString.data.clear();
            StringBuilder sb(currentPathString);
            SC_TRY_IF(sb.appendReplaceAll(copy.view(), L"\\", L"/"));
        }
        entry.path  = currentPathString.view();
        entry.level = static_cast<decltype(entry.level)>(recurseStack.size() - 1);
        if (dirEnumerator.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
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
        SC_TRY_IF(currentPath.appendNullTerminated(L"\\"));
        SC_TRY_IF(currentPath.appendNullTerminated(entry.name));
        newParent.textLengthInBytes = currentPathString.view().sizeInBytesIncludingTerminator();
        SC_TRY_IF(currentPath.appendNullTerminated(L"\\*.*"));
        SC_TRY_IF(newParent.init(currentPathString.view().getNullTerminatedNative(), dirEnumerator));
        SC_TRY_IF(recurseStack.push_back(newParent));
        expectDotDirectories = true;
        return true;
    }
};
