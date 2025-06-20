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
    if (test_section("file system operation - close"))
    {
        fileSystemOperationClose();
    }
    if (test_section("file system operation - read"))
    {
        fileSystemOperationRead();
    }
    if (test_section("file system operation - write"))
    {
        fileSystemOperationWrite();
    }
    if (test_section("file system operation - copy"))
    {
        fileSystemOperationCopy();
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

    // Remove test files
    SC_TEST_EXPECT(fs.removeFile("FileSystemOperationOpen.txt"));
    //! [AsyncFileSystemOperationOpenSnippet]
}

void SC::AsyncTest::fileSystemOperationClose()
{
    //! [AsyncFileSystemOperationCloseSnippet]
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
    SC_TEST_EXPECT(fs.writeString("FileSystemOperationClose.txt", "FileSystemOperationClose"));

    AsyncFileSystemOperation asyncFileSystemOperation;

    int callbackCalled = 0;

    asyncFileSystemOperation.callback = [&](AsyncFileSystemOperation::Result& res)
    {
        callbackCalled++;
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.code == 0);
    };
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    FileDescriptor fd;
    String         path = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory, "FileSystemOperationClose.txt"}));
    SC_TEST_EXPECT(fd.openNativeEncoding(path.view(), FileOpen::Read));
    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fd.get(handle, Result::Error("Invalid FD")));
    fd.detach();
    SC_TEST_EXPECT(asyncFileSystemOperation.close(eventLoop, handle));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(callbackCalled == 1);

    // Remove test files
    SC_TEST_EXPECT(fs.removeFile("FileSystemOperationClose.txt"));
    //! [AsyncFileSystemOperationCloseSnippet]
}

void SC::AsyncTest::fileSystemOperationRead()
{
    //! [AsyncFileSystemOperationReadSnippet]
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
    SC_TEST_EXPECT(fs.writeString("FileSystemOperationRead.txt", "FileSystemOperationRead"));

    // Open the file first
    FileDescriptor fd;
    String         path = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory, "FileSystemOperationRead.txt"}));
    SC_TEST_EXPECT(fd.openNativeEncoding(path.view(), FileOpen::Read));
    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fd.get(handle, Result::Error("Invalid FD")));
    fd.detach();

    AsyncFileSystemOperation asyncFileSystemOperation;
    asyncFileSystemOperation.callback = [&](AsyncFileSystemOperation::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.numBytes == 23); // Length of "FileSystemOperationRead"
    };
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    // Read from the file
    char buffer[32] = {0};
    SC_TEST_EXPECT(asyncFileSystemOperation.read(eventLoop, handle, Span<char>(buffer, sizeof(buffer)), 0));
    SC_TEST_EXPECT(eventLoop.run());
    StringView readContent({buffer, 23}, true, StringEncoding::Ascii);
    SC_TEST_EXPECT(readContent == StringView("FileSystemOperationRead"));

    SC_TEST_EXPECT(eventLoop.run());

    // Remove test files
    SC_TEST_EXPECT(fs.removeFile("FileSystemOperationRead.txt"));
    //! [AsyncFileSystemOperationReadSnippet]
}

void SC::AsyncTest::fileSystemOperationWrite()
{
    //! [AsyncFileSystemOperationWriteSnippet]
    // Create Thread Pool where to run fs operations
    static constexpr int NUM_THREADS = 1;

    ThreadPool threadPool;
    SC_TEST_EXPECT(threadPool.create(NUM_THREADS));

    // Create Event Loop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // Open the file first
    FileDescriptor fd;
    String         path = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(path, {report.applicationRootDirectory, "FileSystemOperationWrite.txt"}));
    SC_TEST_EXPECT(fd.openNativeEncoding(path.view(), FileOpen::Write));
    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fd.get(handle, Result::Error("Invalid FD")));
    fd.detach();

    AsyncFileSystemOperation asyncFileSystemOperation;
    asyncFileSystemOperation.callback = [&](AsyncFileSystemOperation::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.numBytes == 24); // Length of "FileSystemOperationWrite"
    };
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    // Write to the file
    const char* writeData = "FileSystemOperationWrite";
    SC_TEST_EXPECT(asyncFileSystemOperation.write(eventLoop, handle, Span<const char>(writeData, 24), 0));
    SC_TEST_EXPECT(eventLoop.run());

    // Verify the content was written correctly
    FileDescriptor verifyFd;
    SC_TEST_EXPECT(verifyFd.openNativeEncoding(path.view(), FileOpen::Read));
    File   verifyFile(verifyFd);
    String text;
    SC_TEST_EXPECT(verifyFile.readUntilEOF(text));
    SC_TEST_EXPECT(text.view() == "FileSystemOperationWrite");
    SC_TEST_EXPECT(verifyFd.close()); // Close before removing it

    // Remove test files
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.removeFile("FileSystemOperationWrite.txt"));
    //! [AsyncFileSystemOperationWriteSnippet]
}

void SC::AsyncTest::fileSystemOperationCopy()
{
    //! [AsyncFileSystemOperationCopySnippet]
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
    SC_TEST_EXPECT(fs.writeString("FileSystemOperationCopy.txt", "FileSystemOperationCopy"));

    AsyncFileSystemOperation asyncFileSystemOperation;
    asyncFileSystemOperation.callback = [&](AsyncFileSystemOperation::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.code == 0);
    };
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    // Copy the file
    String sourcePath = StringEncoding::Native;
    String destPath   = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(sourcePath, {report.applicationRootDirectory, "FileSystemOperationCopy.txt"}));
    SC_TEST_EXPECT(Path::join(destPath, {report.applicationRootDirectory, "FileSystemOperationCopy2.txt"}));
    SC_TEST_EXPECT(asyncFileSystemOperation.copyFile(eventLoop, sourcePath.view(), destPath.view()));
    SC_TEST_EXPECT(eventLoop.run());

    // Verify the content was copied correctly
    FileDescriptor verifyFd;
    SC_TEST_EXPECT(verifyFd.openNativeEncoding(destPath.view(), FileOpen::Read));
    File   verifyFile(verifyFd);
    String text;
    SC_TEST_EXPECT(verifyFile.readUntilEOF(text));
    SC_TEST_EXPECT(text.view() == "FileSystemOperationCopy");
    SC_TEST_EXPECT(verifyFd.close()); // Close before removing it

    // Remove test files
    SC_TEST_EXPECT(fs.removeFile(sourcePath.view()));
    SC_TEST_EXPECT(fs.removeFile(destPath.view()));
    //! [AsyncFileSystemOperationCopySnippet]
}
