// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../FileSystemIterator/FileSystemIterator.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct SC::FileSystemIterator::Internal
{
    static Result initFolderState(FolderState& entry, const wchar_t* path, WIN32_FIND_DATAW& dirEnumerator)
    {
        entry.fileDescriptor = ::FindFirstFileW(path, &dirEnumerator);
        if (INVALID_HANDLE_VALUE == entry.fileDescriptor)
        {
            return Result::Error("FindFirstFileW failed");
        }
        return Result(true);
    }

    static void closeFolderState(FolderState& entry)
    {
        if (entry.fileDescriptor != INVALID_HANDLE_VALUE)
        {
            ::FindClose(entry.fileDescriptor);
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

    SC_TRY_MSG(currentPath.path.assign(directory), "Directory path is too long");
    const size_t dirLen = currentPath.path.length;
    SC_TRY_MSG(currentPath.path.append(L"\\*.*"), "Directory path is too long");
    {
        FolderState entry;
        entry.textLengthInBytes = dirLen * sizeof(wchar_t);
        SC_TRY(recurseStack.push_back(entry));
    }

    FolderState&      currentFolder = recurseStack.back();
    WIN32_FIND_DATAW& dirEnumerator = reinterpret_cast<WIN32_FIND_DATAW&>(dirEnumeratorBuffer);
    currentFolder.fileDescriptor    = ::FindFirstFileW(currentPath.path.buffer, &dirEnumerator);
    // Set currentPathString back to just the directory (no pattern)
    currentPath.path.buffer[dirLen] = L'\0';

    if (INVALID_HANDLE_VALUE == currentFolder.fileDescriptor)
    {
        return Result::Error("FindFirstFileW failed");
    }

    expectDotDirectories = true;
    return Result(true);
}

SC::Result SC::FileSystemIterator::enumerateNextInternal(Entry& entry)
{
    FolderState& parent = recurseStack.back();
    static_assert(sizeof(dirEnumeratorBuffer) >= sizeof(WIN32_FIND_DATAW), "WIN32_FIND_DATAW");
    WIN32_FIND_DATAW& dirEnumerator = reinterpret_cast<WIN32_FIND_DATAW&>(dirEnumeratorBuffer);

    size_t dirLen = parent.textLengthInBytes / sizeof(wchar_t);

    for (;;)
    {
        if (!expectDotDirectories)
        {
            if (!::FindNextFileW(parent.fileDescriptor, &dirEnumerator))
            {
                Internal::closeFolderState(recurseStack.back());
                recurseStack.pop_back();
                if (recurseStack.isEmpty())
                    return Result::Error("Iteration Finished");
                parent = recurseStack.back();
                dirLen = parent.textLengthInBytes / sizeof(wchar_t);
                continue;
            }
        }
        expectDotDirectories = false;
        if (!(parent.gotDot1 && parent.gotDot2))
        {
            const bool isDot1 = ::wcsncmp(dirEnumerator.cFileName, L".", 2) == 0;
            const bool isDot2 = ::wcsncmp(dirEnumerator.cFileName, L"..", 3) == 0;
            if (isDot1)
                parent.gotDot1 = true;
            if (isDot2)
                parent.gotDot2 = true;
            if (isDot1 || isDot2)
                continue;
        }
        break;
    }

    entry.name = StringSpan({dirEnumerator.cFileName, ::wcsnlen(dirEnumerator.cFileName, MAX_PATH)}, true);

    currentPath.path.length = dirLen;
    SC_TRY_MSG(currentPath.path.append(L"\\"), "Path too long");
    SC_TRY_MSG(currentPath.path.append(entry.name), "Path too long");

    if (options.forwardSlashes)
    {
        // Convert backslashes to forward slashes
        for (size_t i = dirLen; i < currentPath.path.length; ++i)
        {
            if (currentPath.path.buffer[i] == L'\\')
                currentPath.path.buffer[i] = L'/';
        }
    }
    entry.path  = currentPath.path.view();
    entry.level = static_cast<decltype(entry.level)>(recurseStack.size() - 1);

    entry.parentFileDescriptor = parent.fileDescriptor;
    if (dirEnumerator.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
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
    StringPath recursePath;

    // Build subdirectory path
    recursePath.path        = currentPath.path;
    recursePath.path.length = recurseStack.back().textLengthInBytes / sizeof(wchar_t);
    SC_TRY_MSG(recursePath.path.append(L"\\"), "Directory path is too long");
    SC_TRY_MSG(recursePath.path.append(entry.name), "Directory path is too long");

    {
        // Store the length of the sub directory without the trailing \*.* added later
        FolderState newParent;
        newParent.textLengthInBytes = recursePath.path.length * sizeof(wchar_t);
        SC_TRY(recurseStack.push_back(newParent));
    }
    SC_TRY_MSG(recursePath.path.append(L"\\*.*"), "Directory path is too long");

    FolderState&      currentFolder = recurseStack.back();
    WIN32_FIND_DATAW& dirEnumerator = reinterpret_cast<WIN32_FIND_DATAW&>(dirEnumeratorBuffer);
    currentFolder.fileDescriptor    = ::FindFirstFileW(recursePath.path.buffer, &dirEnumerator);
    if (INVALID_HANDLE_VALUE == currentFolder.fileDescriptor)
    {
        return Result::Error("FindFirstFileW failed");
    }

    expectDotDirectories = true;
    return Result(true);
}
