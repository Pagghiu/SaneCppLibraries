// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Path.h"
#include <memory.h> // memcmp
#include <stdlib.h> // abort (independent regression failsafe)

struct SC::AsyncTest::FileSendContext
{
    bool   acceptDone           = false;
    bool   connectDone          = false;
    bool   sendDone             = false;
    bool   receiveDone          = false;
    size_t bytesSent            = 0;
    size_t bytesReceived        = 0;
    size_t expectedBytes        = 0;
    char   receiveBuffer[12000] = {0};
    char   receiveChunk[1024]   = {0};

    SocketDescriptor acceptedSocket;
};

void SC::AsyncTest::fileSend(bool useThreadPool, bool pastEOF, bool cancelFirst, bool large)
{
    if (cancelFirst)
    {
#if SC_PLATFORM_LINUX
        if (options.apiType != AsyncEventLoop::Options::ApiType::ForceUseIoUring)
            return;
#else
        return;
#endif
    }
    // 1. Create ThreadPool and tasks
    ThreadPool threadPool;
    if (useThreadPool)
    {
        SC_TEST_EXPECT(threadPool.create(4));
    }

    // 2. Create EventLoop
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    // 3. Create test directory and file
    SmallStringNative<255> filePath = StringEncoding::Native;
    SmallStringNative<255> dirPath  = StringEncoding::Native;
    const StringView       name     = "AsyncTest";
    const StringView       fileName = "sendfile_test.txt";
    SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory.view(), name}));
    SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));
    SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));

    // Write test content to file
    const char smallContent[] = "Hello, this is a test for AsyncFileSend!";
    char       largeContent[10000];
    if (large)
    {
        for (size_t idx = 0; idx < sizeof(largeContent); ++idx)
            largeContent[idx] = static_cast<char>('a' + idx % 26);
    }
    const char*  testContent = large ? largeContent : smallContent;
    const size_t contentSize = large ? sizeof(largeContent) : sizeof(smallContent) - 1;
    SC_TEST_EXPECT(fs.write(fileName, {testContent, contentSize}));

    // 4. Create a TCP socket pair for testing
    SocketDescriptor serverSocket, clientSocket;
    uint16_t         tcpPort = report.mapPort(5050);
    SocketIPAddress  nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));

    // Use createAsyncTCPSocket for proper async socket setup on Windows
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    {
        SocketServer server(serverSocket);
        SC_TEST_EXPECT(server.bind(nativeAddress));
        SC_TEST_EXPECT(server.listen(1));
    }

    // 5. Set up async accept BEFORE client connect (required on Windows)
    // On Windows, AcceptEx must be called before the client connects
    FileSendContext ctx;

    AsyncSocketAccept asyncAccept;
    asyncAccept.callback = [this, &ctx](AsyncSocketAccept::Result& res)
    {
        SC_TEST_EXPECT(res.moveTo(ctx.acceptedSocket));
        ctx.acceptDone = true;
    };
    SC_TEST_EXPECT(asyncAccept.start(eventLoop, serverSocket));

    // Now connect the client (after async accept is started)
    SC_TEST_EXPECT(clientSocket.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(SocketClient(clientSocket).connect("127.0.0.1", tcpPort));

    // Safety timeout against hangs
    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.runOnce()); // Accept the connection
    SC_TEST_EXPECT(ctx.acceptDone);

    // Associate accepted socket with event loop
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(ctx.acceptedSocket));

    // Also set the client socket to non-blocking for async receive
    SC_TEST_EXPECT(clientSocket.setBlocking(false));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(clientSocket));

    // 6. Open the file for reading
    // For true async I/O (without thread pool), open with blocking = false.
    // When using thread pool, blocking = true works because the synchronous
    // read/write fallback is used on a background thread.
    FileDescriptor fd;
    FileOpen       openModeRead;
    openModeRead.mode     = FileOpen::Read;
    openModeRead.blocking = useThreadPool; // Non-blocking for true async, blocking for thread pool
    SC_TEST_EXPECT(fd.open(filePath.view(), openModeRead));

    // For true async mode, associate the file descriptor with the event loop
    if (not useThreadPool)
    {
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
    }

    struct EOFGuard
    {
        StringSpan   path;
        Atomic<bool> completed    = false;
        Atomic<bool> forcedGrowth = false;
        Result       status{true};
        bool         complete = false;
        bool         zeroCopy = false;
    } eofGuard{filePath.view()};
    Thread guard;
    if (pastEOF)
    {
        SC_TEST_EXPECT(guard.start(
            [&eofGuard](Thread&)
            {
                for (int attempt = 0; attempt < 100 and not eofGuard.completed.load(); ++attempt)
                    Thread::Sleep(10);
                if (not eofGuard.completed.load())
                {
                    // Release the old EOF busy loop without relying on its owner-loop watchdog.
                    eofGuard.forcedGrowth.store(true);
                    FileDescriptor append;
                    if (not append.open(eofGuard.path, FileOpen::Append) or
                        not append.write(StringView("++").toCharSpan()))
                        ::abort();
                }
            }));
    }

    // 7. Create and run AsyncFileSend
    AsyncFileSend     asyncFileSend;
    AsyncTaskSequence asyncTask;

#if SC_PLATFORM_LINUX
    if (cancelFirst)
    {
        bool earlyCompletion   = false;
        asyncFileSend.callback = [&earlyCompletion](AsyncFileSend::Result&) { earlyCompletion = true; };
        SC_TEST_EXPECT(asyncFileSend.start(eventLoop, fd, ctx.acceptedSocket, 0, contentSize));
        alignas(uint64_t) uint8_t storage[8192];
        AsyncKernelEvents         events;
        events.eventsMemory = storage;
        // Dispatch only the file-to-pipe stage. No pipe-to-socket submission has been driven yet.
        SC_TEST_EXPECT(eventLoop.submitRequests(events));
        SC_TEST_EXPECT(eventLoop.blockingPoll(events));
        SC_TEST_EXPECT(eventLoop.dispatchCompletions(events));
        SC_TEST_EXPECT(not earlyCompletion and asyncFileSend.bytesSent == 0);
        bool                         stopped      = false;
        Function<void(AsyncResult&)> afterStopped = [&stopped](AsyncResult&) { stopped = true; };
        SC_TEST_EXPECT(asyncFileSend.stop(eventLoop, &afterStopped));
        SC_TEST_EXPECT(eventLoop.run());
        SC_TEST_EXPECT(stopped and asyncFileSend.isFree());
    }
#endif
    const size_t rangeOffset = cancelFirst ? 2 : 0;
    const size_t rangeLength = contentSize - (cancelFirst ? 4 : 0);
    ctx.expectedBytes        = rangeLength;
    asyncFileSend.callback   = [&ctx, &eofGuard](AsyncFileSend::Result& result)
    {
        ctx.bytesSent     = result.getBytesTransferred();
        ctx.sendDone      = true;
        eofGuard.status   = result.isValid();
        eofGuard.complete = result.isComplete();
        eofGuard.zeroCopy = result.usedZeroCopy();
        eofGuard.completed.store(true);
    };

    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncFileSend.executeOn(asyncTask, threadPool, AsyncThreadPoolMode::ForceThreadPool));
    }
    SC_TEST_EXPECT(asyncFileSend.start(eventLoop, fd, ctx.acceptedSocket, static_cast<int64_t>(rangeOffset),
                                       rangeLength + (pastEOF ? 2 : 0)));

    // 8. Receive the data on client socket
    AsyncSocketReceive asyncReceive;
    asyncReceive.callback = [this, &ctx](AsyncSocketReceive::Result& result)
    {
        Span<char> receivedData;
        SC_TEST_EXPECT(result.get(receivedData));
        const size_t available = sizeof(ctx.receiveBuffer) - ctx.bytesReceived;
        SC_TEST_EXPECT(receivedData.sizeInBytes() <= available);
        if (receivedData.sizeInBytes() <= available)
        {
            memcpy(ctx.receiveBuffer + ctx.bytesReceived, receivedData.data(), receivedData.sizeInBytes());
            ctx.bytesReceived += receivedData.sizeInBytes();
        }
        if (ctx.bytesReceived < ctx.expectedBytes and not receivedData.empty())
            result.reactivateRequest(true);
        else
            ctx.receiveDone = true;
    };
    if (not pastEOF)
        SC_TEST_EXPECT(asyncReceive.start(eventLoop, clientSocket, {ctx.receiveChunk, sizeof(ctx.receiveChunk)}));

    // Run until both send and receive complete
    SC_TEST_EXPECT(eventLoop.run());

    // Join before any failing expectation can leave the fixture.
    if (pastEOF)
    {
        SC_TEST_EXPECT(guard.join());
        SC_TEST_EXPECT(not eofGuard.forcedGrowth.load());
        SC_TEST_EXPECT(not eofGuard.status and not eofGuard.complete and ctx.sendDone);
        SC_TEST_EXPECT(ctx.bytesSent <= contentSize);
#if SC_PLATFORM_WINDOWS
        if (useThreadPool)
#endif
        {
            SC_TEST_EXPECT(eofGuard.status.isError(AsyncResultCategory, AsyncError::FileSendIncomplete));
            SC_TEST_EXPECT(ctx.bytesSent == contentSize);
        }
    }
    else
    {
        SC_TEST_EXPECT(eofGuard.status and eofGuard.complete and ctx.sendDone);
        SC_TEST_EXPECT(ctx.bytesSent == rangeLength);
        SC_TEST_EXPECT(ctx.bytesReceived == rangeLength);
        SC_TEST_EXPECT(memcmp(ctx.receiveBuffer, testContent + rangeOffset, rangeLength) == 0);
    }

#if SC_PLATFORM_NETBSD || SC_PLATFORM_OPENBSD
    SC_TEST_EXPECT(not eofGuard.zeroCopy);
#elif SC_PLATFORM_WINDOWS
    if (useThreadPool)
        SC_TEST_EXPECT(not eofGuard.zeroCopy);
    else
        SC_TEST_EXPECT(ctx.bytesSent == 0 or eofGuard.zeroCopy);
#else
    SC_TEST_EXPECT(ctx.bytesSent == 0 or eofGuard.zeroCopy);
#endif

    // 9. Cleanup
    SC_TEST_EXPECT(fd.close());
    SC_TEST_EXPECT(ctx.acceptedSocket.close());
    SC_TEST_EXPECT(clientSocket.close());
    SC_TEST_EXPECT(serverSocket.close());

    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}
