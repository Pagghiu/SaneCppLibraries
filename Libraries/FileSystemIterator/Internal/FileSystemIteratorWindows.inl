// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../FileSystemIterator.h"

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

SC::Result SC::FileSystemIterator::init(StringViewData directory, Span<FolderState> recursiveEntries)
{
    Internal::destroy(recurseStack);
    recurseStack.recursiveEntries = recursiveEntries;
    recurseStack.currentEntry     = -1;

    size_t dirLen;

    if (directory.getEncoding() == StringEncoding::Utf16)
    {
        dirLen = directory.sizeInBytes() / sizeof(wchar_t);
        if (directory.sizeInBytes() + sizeof(L"\\*.*") > sizeof(currentPathString))
            return Result::Error("Directory path is too long");

        // Copy directory path
        ::memcpy(currentPathString, directory.bytesWithoutTerminator(), directory.sizeInBytes());
    }
    else
    {
        // Convert to UTF16 using MultiByteToWideChar
        dirLen = ::MultiByteToWideChar(CP_UTF8, 0, directory.bytesWithoutTerminator(),
                                       static_cast<int>(directory.sizeInBytes()), currentPathString,
                                       MaxPath - sizeof(L"\\*.*") / sizeof(wchar_t));
    }

    // Append "\\*.*"
    static constexpr wchar_t pattern[] = L"\\*.*";
    // After MultiByteToWideChar
    const size_t patternLen = sizeof(pattern) / sizeof(wchar_t); // includes null terminator
    if (dirLen + patternLen > MaxPath)
        return Result::Error("Directory path is too long");
    ::memcpy(currentPathString + dirLen, pattern, sizeof(pattern));

    {
        FolderState entry;
        entry.textLengthInBytes = dirLen * sizeof(wchar_t);
        SC_TRY(recurseStack.push_back(entry));
    }

    FolderState&      currentFolder = recurseStack.back();
    WIN32_FIND_DATAW& dirEnumerator = reinterpret_cast<WIN32_FIND_DATAW&>(dirEnumeratorBuffer);
    currentFolder.fileDescriptor    = ::FindFirstFileW(currentPathString, &dirEnumerator);
    // Set currentPathString back to just the directory (no pattern)
    currentPathString[dirLen] = L'\0';

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

    // Set entry name
    const size_t nameLen = ::wcsnlen(dirEnumerator.cFileName, MAX_PATH);

    entry.name = StringViewData({dirEnumerator.cFileName, nameLen}, true);

    // Build full path in currentItemString
    if (dirLen + 1 + nameLen + 1 > MaxPath)
        return Result::Error("Path too long");

    ::memcpy(currentPathString + dirLen + 1, dirEnumerator.cFileName, nameLen * sizeof(wchar_t));

    currentPathString[dirLen]               = L'\\';
    currentPathString[dirLen + 1 + nameLen] = L'\0';

    const size_t subDirLen = dirLen + 1 + nameLen;
    if (options.forwardSlashes)
    {
        // Convert backslashes to forward slashes
        for (size_t i = dirLen; i < subDirLen; ++i)
        {
            if (currentPathString[i] == L'\\')
                currentPathString[i] = L'/';
        }
    }
    entry.path  = StringViewData({currentPathString, subDirLen}, true);
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
    wchar_t recurseString[sizeof(currentPathString) / sizeof(wchar_t)];

    const size_t dirLen  = recurseStack.back().textLengthInBytes / sizeof(wchar_t);
    const size_t nameLen = entry.name.sizeInBytes() / sizeof(wchar_t);

    if ((dirLen + 1 + nameLen + sizeof(L"\\*.*")) / sizeof(wchar_t) > MaxPath)
        return Result::Error("Directory path is too long");

    // Build subdirectory path in currentPathString
    ::memcpy(recurseString, currentPathString, dirLen * sizeof(wchar_t));
    ::memcpy(recurseString + dirLen, L"\\", sizeof(wchar_t));
    ::memcpy(recurseString + dirLen + 1, entry.name.getNullTerminatedNative(), nameLen * sizeof(wchar_t));

    const size_t subDirLen = dirLen + 1 + nameLen;

    // Append "\\*.*"
    static constexpr wchar_t pattern[] = L"\\*.*";
    ::memcpy(recurseString + subDirLen, pattern, sizeof(pattern));

    {
        FolderState newParent;
        newParent.textLengthInBytes = subDirLen * sizeof(wchar_t);
        SC_TRY(recurseStack.push_back(newParent));
    }

    FolderState&      currentFolder = recurseStack.back();
    WIN32_FIND_DATAW& dirEnumerator = reinterpret_cast<WIN32_FIND_DATAW&>(dirEnumeratorBuffer);
    currentFolder.fileDescriptor    = ::FindFirstFileW(recurseString, &dirEnumerator);
    if (INVALID_HANDLE_VALUE == currentFolder.fileDescriptor)
    {
        return Result::Error("FindFirstFileW failed");
    }

    expectDotDirectories = true;
    return Result(true);
}
