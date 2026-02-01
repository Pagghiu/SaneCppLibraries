// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/File/File.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Path.h"

struct SC::AsyncTest::FileSendContext
{
    bool   acceptDone         = false;
    bool   connectDone        = false;
    bool   sendDone           = false;
    bool   receiveDone        = false;
    size_t bytesSent          = 0;
    size_t bytesReceived      = 0;
    char   receiveBuffer[256] = {0};

    SocketDescriptor acceptedSocket;
};

void SC::AsyncTest::fileSend(bool useThreadPool)
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
    const char testContent[] = "Hello, this is a test for AsyncFileSend!";
    SC_TEST_EXPECT(fs.write(fileName, {testContent, sizeof(testContent) - 1}));

    // 4. Create a TCP socket pair for testing
    SocketDescriptor serverSocket, clientSocket;
    uint16_t         tcpPort = 5050;
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

    // 7. Create and run AsyncFileSend
    AsyncFileSend     asyncFileSend;
    AsyncTaskSequence asyncTask;

    asyncFileSend.callback = [this, &ctx](AsyncFileSend::Result& result)
    {
        ctx.bytesSent = result.getBytesTransferred();
        ctx.sendDone  = true;
    };

    if (useThreadPool)
    {
        SC_TEST_EXPECT(asyncFileSend.executeOn(asyncTask, threadPool));
    }
    SC_TEST_EXPECT(asyncFileSend.start(eventLoop, fd, ctx.acceptedSocket, 0, sizeof(testContent) - 1));

    // 8. Receive the data on client socket
    AsyncSocketReceive asyncReceive;
    asyncReceive.callback = [this, &ctx](AsyncSocketReceive::Result& result)
    {
        Span<char> receivedData;
        SC_TEST_EXPECT(result.get(receivedData));
        ctx.bytesReceived = receivedData.sizeInBytes();
        // Copy received data for verification
        if (receivedData.sizeInBytes() <= sizeof(ctx.receiveBuffer))
        {
            memcpy(ctx.receiveBuffer, receivedData.data(), receivedData.sizeInBytes());
        }
        ctx.receiveDone = true;
    };
    SC_TEST_EXPECT(asyncReceive.start(eventLoop, clientSocket, {ctx.receiveBuffer, sizeof(ctx.receiveBuffer)}));

    // Run until both send and receive complete
    SC_TEST_EXPECT(eventLoop.run());

    // Verify results
    SC_TEST_EXPECT(ctx.bytesSent == sizeof(testContent) - 1);
    SC_TEST_EXPECT(ctx.bytesReceived == sizeof(testContent) - 1);
    SC_TEST_EXPECT(memcmp(ctx.receiveBuffer, testContent, sizeof(testContent) - 1) == 0);

    // 9. Cleanup
    SC_TEST_EXPECT(fd.close());
    SC_TEST_EXPECT(ctx.acceptedSocket.close());
    SC_TEST_EXPECT(clientSocket.close());
    SC_TEST_EXPECT(serverSocket.close());

    SC_TEST_EXPECT(fs.removeFile(fileName));
    SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
}
