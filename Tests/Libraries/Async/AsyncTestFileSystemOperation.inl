// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/File/File.h"
#include "Libraries/File/FileDescriptor.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystem/Path.h"
#include "Libraries/Testing/Testing.h"

void SC::AsyncTest::fileSystemOperations()
{
    if (test_section("file system operation - open"))
    {
        fileSystemOperationOpen();
    }
}

void SC::AsyncTest::fileSystemOperationOpen()
{
    //! [AsyncFileSystemOperationOpenSnippet]
    // Create Thread Pool where to run fs operations
    static constexpr int NUM_THREADS = 1;

    ThreadPool threadPool;
    SC_TEST_EXPECT(threadPool.create(NUM_THREADS));

    // Create Event Loop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // Create a test file using FileSystem
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.writeString("FileSystemOperationOpen.txt", "FileSystemOperationOpen"));

    AsyncFileSystemOperation asyncFileSystemOperation;
    asyncFileSystemOperation.callback = [&](AsyncFileSystemOperation::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.code == 0);
        SC_TEST_EXPECT(res.completionData.handle != FileDescriptor::Invalid);
        // Read the file content from the file descriptor handle (that is already opened)
        // and check that the content is correct. Descriptor is closed automatically by FileDescriptor.
        FileDescriptor fd(res.completionData.handle);

        File   file(fd);
        String text;
        SC_TEST_EXPECT(file.readUntilEOF(text));
        SC_TEST_EXPECT(text.view() == "FileSystemOperationOpen");
    };
    // Set the thread pool where the open operation will be run
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    // Start the open operation on the given file
    // IMPORTANT! The path string passed in must be in Native Encoding (that means UTF16 on Windows)
    String path = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory, "FileSystemOperationOpen.txt"}));
    SC_TEST_EXPECT(asyncFileSystemOperation.open(eventLoop, path.view(), FileOpen::Read));
    SC_TEST_EXPECT(eventLoop.run());
    //! [AsyncFileSystemOperationOpenSnippet]
}
