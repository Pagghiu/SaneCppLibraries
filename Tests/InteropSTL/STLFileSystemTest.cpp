// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Remember to compile this with SC_COMPILER_ENABLE_STD_CPP=1, and possibly exceptions and RTTI enabled

#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"
#include "SaneCppSTLAdapters.h"

namespace SC
{
struct CppSTLFileSystemTest;
} // namespace SC

struct SC::CppSTLFileSystemTest : public SC::TestCase
{
    CppSTLFileSystemTest(SC::TestReport& report) : TestCase(report, "CppSTLFileSystemTest")
    {
        if (test_section("fileReadWrite"))
        {
            fileReadWriteTest();
        }
    }

    void fileReadWriteTest();
};

void SC::CppSTLFileSystemTest::fileReadWriteTest()
{
    // FileSystem
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    std::string sampleText = "STL is not so much Sane";
    SC_TEST_EXPECT(fs.writeString("test.txt", asSane(sampleText)));
    std::string      readFromFS;
    std::string_view targetFile = "test.txt";
    SC_TEST_EXPECT(fs.read(asSane(targetFile), readFromFS));
    SC_TEST_EXPECT(readFromFS == sampleText);

    // Path
    std::string finalPath;
    SC_TEST_EXPECT(Path::join(finalPath, {report.applicationRootDirectory.view(), asSane(targetFile)}));

    // File
    FileDescriptor fd;
    SC_TEST_EXPECT(fd.open(asSane(finalPath), FileOpen::Mode::Read));
    std::string readFromFD;
    SC_TEST_EXPECT(fd.readUntilEOF(readFromFD));
    SC_TEST_EXPECT(readFromFS == readFromFD);
}

namespace SC
{
void runCppSTLFileSystemTest(SC::TestReport& report) { CppSTLFileSystemTest test(report); }
} // namespace SC
