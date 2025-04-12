// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/FileSystem/FileSystemDirectories.h"
#include "Libraries/Foundation/Limits.h"
#include "Libraries/Testing/Testing.h"

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
