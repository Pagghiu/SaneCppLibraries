// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct FileTest;
}

struct SC::FileTest : public SC::TestCase
{
    FileTest(SC::TestReport& report) : TestCase(report, "FileTest")
    {
        using namespace SC;
        if (test_section("open"))
        {
            testOpen();
        }
    }
    inline void testOpen();
};

void SC::FileTest::testOpen()
{
    //! [FileSnippet]
    SmallStringNative<255> filePath = StringEncoding::Native;
    SmallStringNative<255> dirPath  = StringEncoding::Native;
    // Setup the test
    FileSystem fs;

    const StringView name     = "FileTest";
    const StringView fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.makeDirectory(name));
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));

    // Open a file, write and close it
    FileDescriptor fd;
    SC_TEST_EXPECT(fd.open(filePath.view(), FileOpen::Write));
    SC_TEST_EXPECT(fd.write(StringView("test").toCharSpan()));
    SC_TEST_EXPECT(fd.close());

    // Re-open the file for read
    SC_TEST_EXPECT(fd.open(filePath.view(), FileOpen::Read));

    // Read some data from the file
    char       buffer[4] = {0};
    Span<char> spanOut;
    SC_TEST_EXPECT(fd.read({buffer, sizeof(buffer)}, spanOut));
    SC_TEST_EXPECT(fd.close());

    // Check if read content matches
    StringView sv(spanOut, false, StringEncoding::Ascii);
    SC_TEST_EXPECT(sv.compare("test") == StringView::Comparison::Equals);

    // Shutdown test
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
    //! [FileSnippet]
}

namespace SC
{
void runFileTest(SC::TestReport& report) { FileTest test(report); }
} // namespace SC
