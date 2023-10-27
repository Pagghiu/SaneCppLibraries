// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../FileSystemIterator.h"
#include "../../Testing/Test.h"
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
            FileSystemIterator fsIterator;
            fsIterator.options.recursive = false;
            SC_TEST_EXPECT(fsIterator.init(report.applicationRootDirectory));
            while (fsIterator.enumerateNext())
            {
                auto& item = fsIterator.get();
                report.console.printLine(item.path); // This will take the UTF16 fast path on Windows
                if (item.isDirectory())
                {
                    SC_TEST_EXPECT(fsIterator.recurseSubdirectory());
                }
            }
            SC_TEST_EXPECT(fsIterator.checkErrors());
        }
    }
};

namespace SC
{
void runFileSystemIteratorTest(SC::TestReport& report) { FileSystemIteratorTest test(report); }
} // namespace SC
