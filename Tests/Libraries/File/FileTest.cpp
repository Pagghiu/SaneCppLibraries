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
        if (test_section("open stdhandles"))
        {
            testOpenStdHandles();
        }
    }
    inline void testOpen();
    inline void testOpenStdHandles();

    Result snippetForUniqueHandle();
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

void SC::FileTest::testOpenStdHandles()
{
    FileDescriptor fd[3];
    SC_TEST_EXPECT(fd[0].openStdInDuplicate());
    SC_TEST_EXPECT(fd[1].openStdOutDuplicate());
    SC_TEST_EXPECT(fd[2].openStdErrDuplicate());
}

#if !SC_PLATFORM_WINDOWS
//! [UniqueHandleExampleSnippet]
#include <fcntl.h> // for open
SC::Result SC::FileTest::snippetForUniqueHandle()
{
    StringPath filePath;
    SC_TRY(filePath.assign("someFile.txt"));

    const int flags  = O_RDWR | O_CREAT | O_TRUNC; // Open for read/write, create if not exists, truncate if exists
    const int access = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH; // Read/write for owner, read for group and others

    FileDescriptor myDescriptor;

    const int nativeFd = ::open(filePath.view().bytesIncludingTerminator(), flags, access);

    // Assign the native handle to UniqueHandle (will release the existing one, if any)
    SC_TRY(myDescriptor.assign(nativeFd));

    // UniqueHandle can only be moved, but not copied
    FileDescriptor otherDescriptor = move(myDescriptor);
    // FileDescriptor otherDescriptor = myDescriptor; // <- Doesn't compile

    // Explicitly close (or it will be automatically released on scope close / destructor)
    SC_TRY(otherDescriptor.close());

    // If detach() is called, the handle will be made invalid without releasing it
    otherDescriptor.detach();

    // Check handle for validity
    if (otherDescriptor.isValid())
    {
        // ... do something
    }
    return Result(true);
}
//! [UniqueHandleExampleSnippet]
#endif
namespace SC
{
void runFileTest(SC::TestReport& report) { FileTest test(report); }
} // namespace SC
