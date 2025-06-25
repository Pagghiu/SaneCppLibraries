// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/FileSystemIterator/FileSystemIterator.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct FileSystemIteratorTest;
}

struct SC::FileSystemIteratorTest : public SC::TestCase
{
    FileSystemIteratorTest(SC::TestReport& report) : TestCase(report, "FileSystemIteratorTest")
    {
        using namespace SC;
        if (test_section("recursive manual"))
        {
            walkRecursiveManual();
        }
        if (test_section("recursive"))
        {
            walkRecursive();
        }
        if (test_section("not enough"))
        {
            walkNotEnough();
        }
    };
    inline void walkRecursiveManual();
    inline void walkRecursive();
    inline void walkNotEnough();
};

void SC::FileSystemIteratorTest::walkRecursive()
{
    //! [walkRecursiveSnippet]
    FileSystemIterator::FolderState entries[16];

    FileSystemIterator fsIterator;
    fsIterator.options.recursive = true;
    SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory, entries));
    while (fsIterator.enumerateNext())
    {
        report.console.printLine(fsIterator.get().path);
    }
    SC_TEST_EXPECT(fsIterator.checkErrors());
    //! [walkRecursiveSnippet]
}

void SC::FileSystemIteratorTest::walkNotEnough()
{
    //! [walkNotEnoughSnippet]
    // Test that unsufficient number of folderStates leads to failure to iterate
    // One FolderState is needed for current directory, plus one for each level
    // of recursion done when iterating.

    FileSystemIterator::FolderState folderStates[1];

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("test"));
    FileSystemIterator fsIterator;
    fsIterator.options.recursive = true;
    SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory, folderStates));
    while (fsIterator.enumerateNext())
    {
        report.console.printLine(fsIterator.get().path);
    }
    SC_TEST_EXPECT(not fsIterator.checkErrors()); // one error must be reported
    SC_TEST_EXPECT(fs.removeEmptyDirectory("test"));
    //! [walkNotEnoughSnippet]
}
void SC::FileSystemIteratorTest::walkRecursiveManual()
{
    //! [walkRecursiveManualSnippet]
    FileSystemIterator::FolderState entries[16];

    FileSystemIterator fsIterator;
    fsIterator.options.recursive = false; // As we manually call recurseSubdirectory
    SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory, entries));
    while (fsIterator.enumerateNext())
    {
        const FileSystemIterator::Entry& entry = fsIterator.get();
        report.console.printLine(entry.path);
        // Only recurse directories not ending with "someExcludePattern"
        if (entry.isDirectory() and not StringView(entry.name).endsWith("someExcludePattern"))
        {
            SC_TEST_EXPECT(fsIterator.recurseSubdirectory());
        }
    }
    SC_TEST_EXPECT(fsIterator.checkErrors());
    //! [walkRecursiveManualSnippet]
}

namespace SC
{
void runFileSystemIteratorTest(SC::TestReport& report) { FileSystemIteratorTest test(report); }
} // namespace SC
