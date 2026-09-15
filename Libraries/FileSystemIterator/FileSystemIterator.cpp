// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystemIterator/FileSystemIterator.h"

#define SC_ASSERT_PROVIDER FileSystemIteratorAssert
#include "../Common/Assert.inl"

#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemIteratorWindows.inl"
#else
#include "Internal/FileSystemIteratorPosix.inl"
#endif

SC::FileSystemIterator::~FileSystemIterator() { Internal::destroy(recurseStack); }

SC::ResultFileSystemIterator SC::FileSystemIterator::init(StringSpan directory, Span<FolderState> recursiveEntries)
{
    Internal::destroy(recurseStack);
    errorResult    = ResultFileSystemIterator(true);
    initialized    = false;
    finished       = false;
    entryAvailable = false;

    const ResultFileSystemIterator result = initInternal(directory, recursiveEntries);
    if (not result)
    {
        errorResult = result;
        finished    = true;
        Internal::destroy(recurseStack);
        return result;
    }
    initialized = true;
    return result;
}

bool SC::FileSystemIterator::enumerateNext()
{
    if (not errorResult or finished)
        return false;
    if (not initialized)
    {
        errorResult = ResultFileSystemIterator(FileSystemIteratorError::NotInitialized);
        finished    = true;
        return false;
    }

    bool                           hasEntry = false;
    const ResultFileSystemIterator result   = enumerateNextInternal(currentEntry, hasEntry);
    if (not result)
    {
        errorResult    = result;
        finished       = true;
        entryAvailable = false;
        return false;
    }
    if (not hasEntry)
    {
        finished       = true;
        entryAvailable = false;
        return false;
    }
    entryAvailable = true;
    return true;
}

SC::ResultFileSystemIterator SC::FileSystemIterator::recurseSubdirectory()
{
    if (not errorResult)
        return errorResult;
    if (not initialized)
    {
        errorResult = ResultFileSystemIterator(FileSystemIteratorError::NotInitialized);
        finished    = true;
        return errorResult;
    }
    if (options.recursive or finished or not entryAvailable or not currentEntry.isDirectory())
    {
        const uint32_t depth = recurseStack.isEmpty() ? 0 : static_cast<uint32_t>(recurseStack.size() - 1);
        errorResult          = ResultFileSystemIterator(FileSystemIteratorError::InvalidRecursionState,
                                                        FileSystemIteratorErrorDetail::PushRecursionState, 0, depth);
        finished             = true;
        return errorResult;
    }

    const ResultFileSystemIterator result = recurseSubdirectoryInternal(currentEntry);
    entryAvailable                        = false;
    if (not result)
    {
        errorResult = result;
        finished    = true;
    }
    return result;
}

SC::FileSystemIterator::FolderState& SC::FileSystemIterator::RecurseStack::back()
{
    SC_FILE_SYSTEM_ITERATOR_ASSERT_RELEASE(currentEntry >= 0);
    return recursiveEntries[size_t(currentEntry)];
}

void SC::FileSystemIterator::RecurseStack::pop_back()
{
    SC_FILE_SYSTEM_ITERATOR_ASSERT_RELEASE(currentEntry >= 0);
    currentEntry--;
}

SC::ResultFileSystemIterator SC::FileSystemIterator::RecurseStack::push_back(const FolderState& other)
{
    if (size_t(currentEntry + 1) >= recursiveEntries.sizeInElements())
        return ResultFileSystemIterator(FileSystemIteratorError::RecursionLimitExceeded,
                                        FileSystemIteratorErrorDetail::PushRecursionState, 0,
                                        static_cast<uint32_t>(size()));
    currentEntry += 1;
    recursiveEntries.data()[currentEntry] = other;
    return ResultFileSystemIterator(true);
}
