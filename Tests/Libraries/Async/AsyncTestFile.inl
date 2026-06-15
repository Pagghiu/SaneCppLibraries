// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Path.h"

void SC::AsyncTest::fileReadWrite(bool useThreadPool)
{
    // 1. Create ThreadPool and tasks
    ThreadPool threadPool;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(threadPool.create(4));
    }

    // 2. Create EventLoop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // 3. Create some files on disk
    SmallStringNative<255> filePath = StringEncoding::Native;
    SmallStringNative<255> dirPath  = StringEncoding::Native;
    const StringView       name     = "AsyncTest";
    const StringView       fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));

    // 4. Open the destination file and associate it with the event loop
    FileDescriptor fd;
    FileOpen       openModeWrite;
    openModeWrite.mode     = FileOpen::Write;
    openModeWrite.blocking = useThreadPool;
    SC_TEST_EXPECT(fd.open(filePath.view(), openModeWrite));
    if (not useThreadPool)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
    }

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

    // 5. Create and start the write operation
    AsyncFileWrite    asyncWriteFile;
    AsyncTaskSequence asyncWriteTask;

    asyncWriteFile.setDebugName("FileWrite");
    asyncWriteFile.callback = [&](AsyncFileWrite::Result& res)
    {
        size_t writtenBytes = 0;
        SC_TEST_EXPECT(res.get(writtenBytes));
        SC_TEST_EXPECT(writtenBytes == 4);
    };
    asyncWriteFile.handle = handle;
    asyncWriteFile.buffer = StringView("test").toCharSpan();
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncWriteFile.executeOn(asyncWriteTask, threadPool));
    }
    SC_TEST_EXPECT(asyncWriteFile.start(eventLoop));

    // 6. Run the write operation and close the file
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(fd.close());

    // 7. Open the file for read now
    FileOpen openModeRead;
    openModeRead.mode     = FileOpen::Read;
    openModeRead.blocking = useThreadPool;
    SC_TEST_EXPECT(fd.open(filePath.view(), openModeRead));
    if (not useThreadPool)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
    }
    SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

    // 8. Create and run the read task, reading a single byte at every reactivation
    struct Params
    {
        int  readCount     = 0;
        char readBuffer[4] = {0};
    };
    Params            params;
    AsyncFileRead     asyncReadFile;
    AsyncTaskSequence asyncReadTask;
    asyncReadFile.setDebugName("FileRead");
    asyncReadFile.callback = [this, &params](AsyncFileRead::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        if (params.readCount < 4)
        {
            SC_TEST_EXPECT(readData.sizeInBytes() == 1);
            params.readBuffer[params.readCount++] = readData.data()[0];
            res.getAsync().setOffset(res.getAsync().getOffset() + readData.sizeInBytes());
            res.reactivateRequest(true);
        }
        else
        {
            SC_TEST_EXPECT(res.completionData.endOfFile);
            SC_TEST_EXPECT(readData.empty()); // EOF
        }
    };
    char buffer[1]       = {0};
    asyncReadFile.handle = handle;
    asyncReadFile.buffer = {buffer, sizeof(buffer)};
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncReadFile.executeOn(asyncReadTask, threadPool));
    }
    SC_TEST_EXPECT(asyncReadFile.start(eventLoop));

    // 9. Run the read operation and close the file
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(fd.close());

    // 10. Check Results
    StringView sv({params.readBuffer, sizeof(params.readBuffer)}, false, StringEncoding::Ascii);
    SC_TEST_EXPECT(sv.compare("test") == StringView::Comparison::Equals);

    // 11. Remove test files
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}

void SC::AsyncTest::fileEndOfFile(bool useThreadPool)
{
    // This tests a weird edge case where doing a single file read of the entire size of file
    // will not produce end of file flag

    // 1. Create ThreadPool and tasks
    ThreadPool threadPool;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(threadPool.create(4));
    }

    // 2. Create EventLoop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // 3. Create some files on disk
    SmallStringNative<255> filePath = StringEncoding::Native;
    SmallStringNative<255> dirPath  = StringEncoding::Native;
    const StringView       name     = "AsyncTest";
    const StringView       fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
    {
        char data[1024] = {0};
        SC_TEST_EXPECT(fs.write(fileName, {data, sizeof(data)}));
    }

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    FileDescriptor         fd;
    FileOpen               openModeRead;
    openModeRead.mode     = FileOpen::Read;
    openModeRead.blocking = useThreadPool;
    SC_TEST_EXPECT(fd.open(filePath.view(), openModeRead));
    if (not useThreadPool)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
    }
    SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

    struct Context
    {
        int    readCount = 0;
        size_t readSize  = 0;
    } context;
    AsyncFileRead     asyncReadFile;
    AsyncTaskSequence asyncReadTask;
    asyncReadFile.setDebugName("FileRead");
    asyncReadFile.callback = [this, &context](AsyncFileRead::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        if (context.readCount == 0)
        {
            context.readSize += readData.sizeInBytes();
            res.reactivateRequest(true);
        }
        else if (context.readCount == 1)
        {
            context.readSize += readData.sizeInBytes();
        }
        else if (context.readCount == 2)
        {
            SC_TEST_EXPECT(res.completionData.endOfFile);
            SC_TEST_EXPECT(readData.empty()); // EOF
        }
        else
        {
            SC_TEST_EXPECT(context.readCount <= 3);
        }
        context.readCount++;
    };
    char buffer[512]     = {0};
    asyncReadFile.handle = handle;
    asyncReadFile.buffer = {buffer, sizeof(buffer)};
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncReadFile.executeOn(asyncReadTask, threadPool));
    }
    SC_TEST_EXPECT(asyncReadFile.start(eventLoop));

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.readCount == 2);
    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncReadFile.executeOn(asyncReadTask, threadPool));
    }
    SC_TEST_EXPECT(asyncReadFile.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.readCount == 3);
    SC_TEST_EXPECT(fd.close());

    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}

void SC::AsyncTest::fileWriteMultiple(bool useThreadPool)
{
    // 1. Create ThreadPool and tasks
    ThreadPool threadPool;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(threadPool.create(4));
    }

    // 2. Create EventLoop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // 2. Create Some file paths
    SmallStringNative<255> filePath = StringEncoding::Native;
    SmallStringNative<255> dirPath  = StringEncoding::Native;
    const StringView       name     = "AsyncTest";
    const StringView       fileName = "test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));

    // 4. Open the destination file and associate it with the event loop
    FileOpen openModeWrite;
    openModeWrite.mode     = FileOpen::Write;
    openModeWrite.blocking = useThreadPool;

    FileDescriptor fd;
    SC_TEST_EXPECT(fd.open(filePath.view(), openModeWrite));
    if (not useThreadPool)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
    }

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(fd.get(handle, Result::Error("handle")));

    // 5. Write the file using two requests on the same sequence (that ensures ordering)
    AsyncFileWrite    fileWrite[2];
    AsyncTaskSequence fileWriteTask;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(fileWrite[0].executeOn(fileWriteTask, threadPool)); // executed first
        SC_TEST_EXPECT(fileWrite[1].executeOn(fileWriteTask, threadPool)); // executed second
    }
    else
    {
        fileWrite[0].executeOn(fileWriteTask); // executed first
        fileWrite[1].executeOn(fileWriteTask); // executed second
    }

    fileWrite[0].callback = [&](AsyncFileWrite::Result& res)
    {
        size_t writtenBytes = 0;
        SC_TEST_EXPECT(res.get(writtenBytes));
        SC_TEST_EXPECT(writtenBytes == 8);
    };
    fileWrite[1].callback = [&](AsyncFileWrite::Result& res)
    {
        size_t writtenBytes = 0;
        SC_TEST_EXPECT(res.get(writtenBytes));
        SC_TEST_EXPECT(writtenBytes == 8);
    };
    fileWrite[0].handle = handle;
    fileWrite[1].handle = handle;

    // TODO: Find a way to reliably test partial gather writes
    Span<const char> buffers1[] = {{"PING", 4}, {"PONG", 4}};
    SC_TEST_EXPECT(fileWrite[0].start(eventLoop, buffers1));
    Span<const char> buffers2[] = {{"PENG", 4}, {"PANG", 4}};
    SC_TEST_EXPECT(fileWrite[1].start(eventLoop, buffers2));

    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(fd.close());

    // 6. Verify file contents
    String contents = StringEncoding::Ascii;
    SC_TEST_EXPECT(fs.read(filePath.view(), contents));
    StringView sv = contents.view();
    SC_TEST_EXPECT(sv == "PINGPONGPENGPANG");

    // 7. Cleanup
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}

void SC::AsyncTest::fileReadiness()
{
    PipeDescriptor pipe;
    SC_TEST_EXPECT(pipe.createPipe());

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    FileDescriptor::Handle readHandle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(pipe.readPipe.get(readHandle, Result::Error("read handle")));

    int                pollCount = 0;
    AsyncFileReadiness poll;
    poll.setDebugName("FileReadiness");
    poll.callback = [this, &pollCount](AsyncFileReadiness::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        pollCount++;
    };

#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(not poll.start(eventLoop, readHandle));
    SC_TEST_EXPECT(pollCount == 0);
#else
    SC_TEST_EXPECT(poll.start(eventLoop, readHandle));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(pollCount == 0);

    SC_TEST_EXPECT(pipe.writePipe.writeString("x"));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(pollCount == 1);

    char       buffer[1] = {};
    Span<char> readData;
    SC_TEST_EXPECT(pipe.readPipe.read({buffer, sizeof(buffer)}, readData));
    SC_TEST_EXPECT(readData.sizeInBytes() == 1);
    SC_TEST_EXPECT(readData.data()[0] == 'x');
#endif

    SC_TEST_EXPECT(pipe.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::fileReadinessCancel()
{
    PipeDescriptor pipe;
    SC_TEST_EXPECT(pipe.createPipe());

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    FileDescriptor::Handle readHandle = FileDescriptor::Invalid;
    SC_TEST_EXPECT(pipe.readPipe.get(readHandle, Result::Error("read handle")));

    int                pollCount       = 0;
    int                afterStopCalled = 0;
    AsyncFileReadiness poll;
    poll.setDebugName("FileReadinessCancel");
    poll.callback = [&pollCount](AsyncFileReadiness::Result&) { pollCount++; };

#if SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(not poll.start(eventLoop, readHandle));
    SC_TEST_EXPECT(pollCount == 0);
    SC_TEST_EXPECT(afterStopCalled == 0);
#else
    SC_TEST_EXPECT(poll.start(eventLoop, readHandle));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(pollCount == 0);

    Function<void(AsyncResult&)> afterStopped = [&](AsyncResult&) { afterStopCalled++; };
    SC_TEST_EXPECT(poll.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(afterStopCalled == 0);
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(afterStopCalled == 1);
    SC_TEST_EXPECT(pollCount == 0);
#endif

    SC_TEST_EXPECT(pipe.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::externalCompletionManual()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    struct ExternalCompletionManualState
    {
        AsyncTest*               test            = nullptr;
        AsyncExternalCompletion* completion      = nullptr;
        int                      completionCount = 0;
        size_t                   bytes           = 0;
    };

    AsyncExternalCompletion       completion;
    ExternalCompletionManualState state;
    state.test       = this;
    state.completion = &completion;
    completion.setDebugName("ExternalCompletionManual");
    completion.callback = [&state](AsyncExternalCompletion::Result& result)
    {
        state.test->recordExpectation(StringSpan("result.isValid()"), result.isValid());
        state.completionCount++;
        state.bytes = result.completionData.bytesTransferred;
        if (state.completionCount <= 2)
        {
            state.test->recordExpectation(StringSpan("result.getAsync().markSubmissionPending()"),
                                          result.getAsync().markSubmissionPending());
            result.reactivateRequest(true);
        }
    };

    SC_TEST_EXPECT(completion.start(eventLoop));
    SC_TEST_EXPECT(eventLoop.runNoWait());

    SC_TEST_EXPECT(completion.markSubmissionPending());
    SC_TEST_EXPECT(eventLoop.postExternalCompletion(completion, 17));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(state.completionCount == 1);
    SC_TEST_EXPECT(state.bytes == 17);
    SC_TEST_EXPECT(eventLoop.runNoWait());

    SC_TEST_EXPECT(eventLoop.postExternalCompletion(completion, 23));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(state.completionCount == 2);
    SC_TEST_EXPECT(state.bytes == 23);
    SC_TEST_EXPECT(eventLoop.runNoWait());

    SC_TEST_EXPECT(eventLoop.postExternalCompletion(completion, 42));
    int                          afterStopCalled = 0;
    Function<void(AsyncResult&)> afterStopped    = [&](AsyncResult&) { afterStopCalled++; };
    SC_TEST_EXPECT(completion.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(afterStopCalled == 1);
    SC_TEST_EXPECT(state.completionCount == 2);

    SC_TEST_EXPECT(eventLoop.close());
}
