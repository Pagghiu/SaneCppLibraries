// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../FileSystemIterator.h"
#include "../../Testing/Testing.h"
namespace SC
{
struct FileSystemIteratorTest;
}

struct SC::FileSystemIteratorTest : public SC::TestCase
{
    FileSystemIteratorTest(SC::TestReport& report) : TestCase(report, "FileSystemIteratorTest")
    {
        using namespace SC;
        if (test_section("walk_recursive"))
        {
            walkRecursiveManual();
            walkRecursive();
        }
    };
    inline void walkRecursiveManual();
    inline void walkRecursive();
};
void SC::FileSystemIteratorTest::walkRecursive()
{
    //! [walkRecursiveSnippet]
    FileSystemIterator fsIterator;
    fsIterator.options.recursive = true;
    SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory));
    while (fsIterator.enumerateNext())
    {
        report.console.printLine(fsIterator.get().path);
    }
    SC_TEST_EXPECT(fsIterator.checkErrors());
    //! [walkRecursiveSnippet]
}
void SC::FileSystemIteratorTest::walkRecursiveManual()
{
    //! [walkRecursiveManualSnippet]
    FileSystemIterator fsIterator;
    fsIterator.options.recursive = false; // As we manually call recurseSubdirectory
    SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory));
    while (fsIterator.enumerateNext())
    {
        const FileSystemIterator::Entry& entry = fsIterator.get();
        report.console.printLine(entry.path);
        // Only recurse directories not ending with "someExcludePattern"
        if (entry.isDirectory() and not entry.name.endsWith("someExcludePattern"))
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
