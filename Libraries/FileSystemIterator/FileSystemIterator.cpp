// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "FileSystemIterator.h"
#include "../Foundation/Assert.h"

#if SC_PLATFORM_WINDOWS
#include "Internal/FileSystemIteratorWindows.inl"
#else
#include "Internal/FileSystemIteratorPosix.inl"
#endif

SC::FileSystemIterator::~FileSystemIterator() { Internal::destroy(recurseStack); }

SC::Result SC::FileSystemIterator::enumerateNext()
{
    Result res = enumerateNextInternal(currentEntry);
    if (not res)
    {
        if (::strcmp(res.message, "Iteration Finished") != 0)
        {
            errorResult   = res;
            errorsChecked = false;
        }
    }
    return res;
}

SC::Result SC::FileSystemIterator::recurseSubdirectory()
{
    if (options.recursive)
    {
        errorResult   = Result::Error("Cannot recurseSubdirectory() with recursive==true");
        errorsChecked = false;
        return errorResult;
    }
    return recurseSubdirectoryInternal(currentEntry);
}

SC::FileSystemIterator::FolderState& SC::FileSystemIterator::RecurseStack::back()
{
    SC_ASSERT_RELEASE(currentEntry >= 0);
    return recursiveEntries[size_t(currentEntry)];
}

void SC::FileSystemIterator::RecurseStack::pop_back()
{
    SC_ASSERT_RELEASE(currentEntry >= 0);
    currentEntry--;
}

SC::Result SC::FileSystemIterator::RecurseStack::push_back(const FolderState& other)
{
    if (size_t(currentEntry + 1) >= recursiveEntries.sizeInElements())
        return Result::Error("FileSystemIterator - Not enough space in recurse stack");
    currentEntry += 1;
    recursiveEntries.data()[currentEntry] = other;
    return Result(true);
}
