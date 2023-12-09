// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../FileDescriptor.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct FileDescriptorTest;
}

struct SC::FileDescriptorTest : public SC::TestCase
{
    FileDescriptorTest(SC::TestReport& report) : TestCase(report, "FileDescriptorTest")
    {
        using namespace SC;
        if (test_section("open"))
        {
            testOpen();
        }
    }
    inline void testOpen();
};

void SC::FileDescriptorTest::testOpen()
{
    //! [FileSnippet]
    StringNative<255> filePath = StringEncoding::Native;
    StringNative<255> dirPath  = StringEncoding::Native;
    // Setup the test
    FileSystem fs;

    const StringView name     = "FileDescriptorTest";
    const StringView fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.makeDirectory(name));
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));

    // Open a file, write and close it
    FileDescriptor fd;
    SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::WriteCreateTruncate));
    SC_TEST_EXPECT(fd.write(StringView("test").toCharSpan()));
    SC_TEST_EXPECT(fd.close());

    // Re-open the file for read
    SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::ReadOnly));

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
void runFileDescriptorTest(SC::TestReport& report) { FileDescriptorTest test(report); }
} // namespace SC
