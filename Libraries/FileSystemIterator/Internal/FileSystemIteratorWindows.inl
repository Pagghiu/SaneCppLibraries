// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../FileSystemIterator/FileSystemIterator.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

namespace SC
{
namespace FileSystemIteratorWindowsDetail
{
#include "../../Common/WindowsPath.inl"
}
} // namespace SC

namespace
{
static bool fileSystemIteratorNeedsWindowsLongPathTransport(SC::StringSpan path)
{
    return path.sizeInBytes() / sizeof(wchar_t) >= MAX_PATH;
}
} // namespace

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

    SC_TRY(FileSystemIteratorWindowsDetail::WindowsPath::makeAbsoluteLogicalPath(directory, {}, currentPath));
    const size_t dirLen = currentPath.view().sizeInBytes() / sizeof(wchar_t);

    StringPath searchPath = currentPath;
    SC_TRY_MSG(searchPath.append(L"\\*.*"), "Directory path is too long");
    FileSystemIteratorWindowsDetail::WindowsPath::TransportString transportPath;
    const wchar_t* searchPattern = searchPath.view().getNullTerminatedNative();
    if (fileSystemIteratorNeedsWindowsLongPathTransport(searchPath.view()))
    {
        SC_TRY(FileSystemIteratorWindowsDetail::WindowsPath::appendTransportPrefix(searchPath.view(), transportPath));
        searchPattern = transportPath.view().getNullTerminatedNative();
    }
    {
        FolderState entry;
        entry.textLengthInBytes = dirLen * sizeof(wchar_t);
        SC_TRY(recurseStack.push_back(entry));
    }

    FolderState&      currentFolder = recurseStack.back();
    WIN32_FIND_DATAW& dirEnumerator = reinterpret_cast<WIN32_FIND_DATAW&>(dirEnumeratorBuffer);
    currentFolder.fileDescriptor    = ::FindFirstFileW(searchPattern, &dirEnumerator);

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

    entry.name = StringSpan::fromNullTerminated(dirEnumerator.cFileName, StringEncoding::Utf16);

    (void)currentPath.resize(dirLen);
    SC_TRY_MSG(currentPath.append(L"\\"), "Path too long");
    SC_TRY_MSG(currentPath.append(entry.name), "Path too long");

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

    if (options.forwardSlashes)
    {
        outputPath              = currentPath;
        wchar_t*     pathData   = outputPath.writableSpan().data();
        const size_t pathLength = outputPath.view().sizeInBytes() / sizeof(wchar_t);
        for (size_t i = 0; i < pathLength; ++i)
        {
            if (pathData[i] == L'\\')
                pathData[i] = L'/';
        }
        entry.path = outputPath.view();
    }
    else
    {
        entry.path = currentPath.view();
    }
    entry.level = static_cast<decltype(entry.level)>(recurseStack.size() - 1);
    return Result(true);
}

SC::Result SC::FileSystemIterator::recurseSubdirectoryInternal(Entry& entry)
{
    StringPath recursePath;

    // Build subdirectory path
    recursePath = currentPath;
    (void)recursePath.resize(recurseStack.back().textLengthInBytes / sizeof(wchar_t));
    SC_TRY_MSG(recursePath.append(L"\\"), "Directory path is too long");
    SC_TRY_MSG(recursePath.append(entry.name), "Directory path is too long");

    {
        // Store the length of the sub directory without the trailing \*.* added later
        FolderState newParent;
        newParent.textLengthInBytes = recursePath.view().sizeInBytes();
        SC_TRY(recurseStack.push_back(newParent));
    }
    StringPath searchPath = recursePath;
    SC_TRY_MSG(searchPath.append(L"\\*.*"), "Directory path is too long");
    FileSystemIteratorWindowsDetail::WindowsPath::TransportString transportPath;
    const wchar_t* searchPattern = searchPath.view().getNullTerminatedNative();
    if (fileSystemIteratorNeedsWindowsLongPathTransport(searchPath.view()))
    {
        SC_TRY(FileSystemIteratorWindowsDetail::WindowsPath::appendTransportPrefix(searchPath.view(), transportPath));
        searchPattern = transportPath.view().getNullTerminatedNative();
    }

    FolderState&      currentFolder = recurseStack.back();
    WIN32_FIND_DATAW& dirEnumerator = reinterpret_cast<WIN32_FIND_DATAW&>(dirEnumeratorBuffer);
    currentFolder.fileDescriptor    = ::FindFirstFileW(searchPattern, &dirEnumerator);
    if (INVALID_HANDLE_VALUE == currentFolder.fileDescriptor)
    {
        return Result::Error("FindFirstFileW failed");
    }

    expectDotDirectories = true;
    return Result(true);
}
