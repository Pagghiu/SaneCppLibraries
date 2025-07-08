// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Strings/Path.h"
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
    if (test_section("file system operation - rename"))
    {
        fileSystemOperationRename();
    }
    if (test_section("file system operation - remove empty directory"))
    {
        fileSystemOperationRemoveEmptyDirectory();
    }
    if (test_section("file system operation - remove file"))
    {
        fileSystemOperationRemoveFile();
    }
    if (test_section("file system operation - copy directory"))
    {
        fileSystemOperationCopyDirectory();
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
        // and check that the content is correct. Descriptor is closed automatically by File.
        FileDescriptor fd(res.completionData.handle);

        String text;
        SC_TEST_EXPECT(fd.readUntilEOF(text));
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
    SC_TEST_EXPECT(fd.open(path.view(), FileOpen::Read));
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
    SC_TEST_EXPECT(fd.open(path.view(), FileOpen::Read));
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
    SC_TEST_EXPECT(fd.open(path.view(), FileOpen::Write));
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
    SC_TEST_EXPECT(verifyFd.open(path.view(), FileOpen::Read));
    String text;
    SC_TEST_EXPECT(verifyFd.readUntilEOF(text));
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
    SC_TEST_EXPECT(verifyFd.open(destPath.view(), FileOpen::Read));
    String text;
    SC_TEST_EXPECT(verifyFd.readUntilEOF(text));
    SC_TEST_EXPECT(text.view() == "FileSystemOperationCopy");
    SC_TEST_EXPECT(verifyFd.close()); // Close before removing it

    // Remove test files
    SC_TEST_EXPECT(fs.removeFile(sourcePath.view()));
    SC_TEST_EXPECT(fs.removeFile(destPath.view()));
    //! [AsyncFileSystemOperationCopySnippet]
}

void SC::AsyncTest::fileSystemOperationRename()
{
    //! [AsyncFileSystemOperationRenameSnippet]
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
    SC_TEST_EXPECT(fs.writeString("FileSystemOperationRename.txt", "FileSystemOperationRename"));

    AsyncFileSystemOperation asyncFileSystemOperation;
    asyncFileSystemOperation.callback = [&](AsyncFileSystemOperation::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.code == 0);
    };
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    // Rename the file
    String sourcePath = StringEncoding::Native;
    String destPath   = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(sourcePath, {report.applicationRootDirectory, "FileSystemOperationRename.txt"}));
    SC_TEST_EXPECT(Path::join(destPath, {report.applicationRootDirectory, "FileSystemOperationRename2.txt"}));
    SC_TEST_EXPECT(asyncFileSystemOperation.rename(eventLoop, sourcePath.view(), destPath.view()));
    SC_TEST_EXPECT(eventLoop.run());

    // Verify the content was renamed correctly
    FileDescriptor verifyFd;
    SC_TEST_EXPECT(verifyFd.open(destPath.view(), FileOpen::Read));
    String text;
    SC_TEST_EXPECT(verifyFd.readUntilEOF(text));
    SC_TEST_EXPECT(text.view() == "FileSystemOperationRename");
    SC_TEST_EXPECT(verifyFd.close()); // Close before removing it

    // Remove test files
    SC_TEST_EXPECT(fs.removeFile(destPath.view()));
    //! [AsyncFileSystemOperationRenameSnippet]
}

void SC::AsyncTest::fileSystemOperationRemoveEmptyDirectory()
{
    //! [AsyncFileSystemOperationRemoveEmptyFolderSnippet]
    // Create Thread Pool where to run fs operations
    static constexpr int NUM_THREADS = 1;

    ThreadPool threadPool;
    SC_TEST_EXPECT(threadPool.create(NUM_THREADS));

    // Create Event Loop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // Create a test directory using FileSystem
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    String dirPath = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, "FileSystemOperationRemoveEmptyDirectory"}));
    SC_TEST_EXPECT(fs.makeDirectory(dirPath.view()));

    AsyncFileSystemOperation asyncFileSystemOperation;
    int                      numCallbacks = 0;
    asyncFileSystemOperation.callback     = [&](AsyncFileSystemOperation::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.code == 0);
        numCallbacks++;
    };
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    // Remove the empty directory
    SC_TEST_EXPECT(asyncFileSystemOperation.removeEmptyDirectory(eventLoop, dirPath.view()));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(numCallbacks == 1); // Ensure the callback was called

    // Verify the directory was removed
    SC_TEST_EXPECT(!fs.existsAndIsDirectory(dirPath.view()));

    //! [AsyncFileSystemOperationRemoveEmptyFolderSnippet]
}

void SC::AsyncTest::fileSystemOperationRemoveFile()
{
    //! [AsyncFileSystemOperationRemoveFileSnippet]
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
    String filePath = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(filePath, {report.applicationRootDirectory, "FileSystemOperationRemoveFile.txt"}));
    SC_TEST_EXPECT(fs.writeString(filePath.view(), "FileSystemOperationRemoveFile"));

    AsyncFileSystemOperation asyncFileSystemOperation;
    asyncFileSystemOperation.callback = [&](AsyncFileSystemOperation::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.code == 0);
    };
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    // Remove the file
    SC_TEST_EXPECT(asyncFileSystemOperation.removeFile(eventLoop, filePath.view()));
    SC_TEST_EXPECT(eventLoop.run());

    // Verify the file was removed
    SC_TEST_EXPECT(not fs.existsAndIsFile(filePath.view()));

    //! [AsyncFileSystemOperationRemoveFileSnippet]
}

void SC::AsyncTest::fileSystemOperationCopyDirectory()
{
    //! [AsyncFileSystemOperationCopyDirectorySnippet]
    // Create Thread Pool where to run fs operations
    static constexpr int NUM_THREADS = 1;

    ThreadPool threadPool;
    SC_TEST_EXPECT(threadPool.create(NUM_THREADS));

    // Create Event Loop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // Create a test directory structure synchronously
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
    SC_TEST_EXPECT(fs.makeDirectory("AsyncCopyDir"));
    SC_TEST_EXPECT(fs.writeString("AsyncCopyDir/file1.txt", "data1"));
    SC_TEST_EXPECT(fs.makeDirectory("AsyncCopyDir/subdir"));
    SC_TEST_EXPECT(fs.writeString("AsyncCopyDir/subdir/file2.txt", "data2"));

    // Prepare async copy operation
    AsyncFileSystemOperation asyncFileSystemOperation;
    asyncFileSystemOperation.callback = [&](AsyncFileSystemOperation::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        SC_TEST_EXPECT(res.completionData.code == 0);
    };
    SC_TEST_EXPECT(asyncFileSystemOperation.setThreadPool(threadPool));

    // Copy the directory
    String sourcePath = StringEncoding::Native;
    String destPath   = StringEncoding::Native;
    SC_TEST_EXPECT(Path::join(sourcePath, {report.applicationRootDirectory, "AsyncCopyDir"}));
    SC_TEST_EXPECT(Path::join(destPath, {report.applicationRootDirectory, "AsyncCopyDirCopy"}));
    SC_TEST_EXPECT(asyncFileSystemOperation.copyDirectory(eventLoop, sourcePath.view(), destPath.view()));
    SC_TEST_EXPECT(eventLoop.run());

    // Verify the content was copied correctly
    SC_TEST_EXPECT(fs.existsAndIsFile("AsyncCopyDirCopy/file1.txt"));
    SC_TEST_EXPECT(fs.existsAndIsFile("AsyncCopyDirCopy/subdir/file2.txt"));
    String text = StringEncoding::Ascii;
    SC_TEST_EXPECT(fs.read("AsyncCopyDirCopy/file1.txt", text));
    SC_TEST_EXPECT(text.view() == "data1");
    SC_TEST_EXPECT(fs.read("AsyncCopyDirCopy/subdir/file2.txt", text));
    SC_TEST_EXPECT(text.view() == "data2");

    // Cleanup
    SC_TEST_EXPECT(fs.removeFile("AsyncCopyDir/file1.txt"));
    SC_TEST_EXPECT(fs.removeFile("AsyncCopyDir/subdir/file2.txt"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("AsyncCopyDir/subdir"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("AsyncCopyDir"));
    SC_TEST_EXPECT(fs.removeFile("AsyncCopyDirCopy/file1.txt"));
    SC_TEST_EXPECT(fs.removeFile("AsyncCopyDirCopy/subdir/file2.txt"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("AsyncCopyDirCopy/subdir"));
    SC_TEST_EXPECT(fs.removeEmptyDirectory("AsyncCopyDirCopy"));
    //! [AsyncFileSystemOperationCopyDirectorySnippet]
}
