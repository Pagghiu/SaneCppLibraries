// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../FileSystemDirectories.h"
#include "../../Foundation/Limits.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct FileSystemDirectoriesTest;
}

struct SC::FileSystemDirectoriesTest : public SC::TestCase
{
    FileSystemDirectoriesTest(SC::TestReport& report) : TestCase(report, "FileSystemDirectoriesTest")
    {
        using namespace SC;
        if (test_section("FileSystemDirectories"))
        {
            FileSystemDirectories directories;
            SC_TEST_EXPECT(directories.init());
            report.console.print("executableFile=\"{}\"\n", directories.getExecutablePath());
            report.console.print("applicationRootDirectory=\"{}\"\n", directories.getApplicationPath());
        }
    }
};

namespace SC
{
void runFileSystemDirectoriesTest(SC::TestReport& report) { FileSystemDirectoriesTest test(report); }
} // namespace SC
