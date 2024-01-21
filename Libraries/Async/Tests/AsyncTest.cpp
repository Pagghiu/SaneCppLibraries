// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Async.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../Process/Process.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"
#include "../../Threading/Threading.h" // EventObject

namespace SC
{
struct AsyncTest;
}

struct SC::AsyncTest : public SC::TestCase
{
    AsyncTest(SC::TestReport& report) : TestCase(report, "AsyncTest")
    {
        loopTimeout();
        loopWakeUpFromExternalThread();
        loopWakeUp();
        loopWakeUpEventObject();
        processExit();
        socketAccept();
        socketConnect();
        socketSendReceive();
        socketSendReceiveError();
        socketClose();
        fileReadWrite();
        fileClose();
    }

    void loopTimeout()
    {
        // TODO: Add AsyncEventLoop::resetTimeout
        if (test_section("loop timeout"))
        {
            AsyncLoopTimeout timeout1, timeout2;
            AsyncEventLoop   eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            int timeout1Called = 0;
            int timeout2Called = 0;
            timeout1.callback  = [&](AsyncLoopTimeout::Result& res)
            {
                SC_TEST_EXPECT(res.async.getTimeout().ms == 1);
                timeout1Called++;
            };
            SC_TEST_EXPECT(timeout1.start(eventLoop, Time::Milliseconds(1)));
            timeout2.callback = [&](AsyncLoopTimeout::Result&)
            {
                // TODO: investigate allowing dropping AsyncResultBase
                timeout2Called++;
            };
            SC_TEST_EXPECT(timeout2.start(eventLoop, Time::Milliseconds(100)));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 0);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 1);
        }
    }

    int  threadWasCalled = 0;
    int  wakeUpSucceeded = 0;
    void loopWakeUpFromExternalThread()
    {
        // TODO: This test is not actually testing anything (on Linux)
        if (test_section("loop wakeUpFromExternalThread"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Thread newThread;
            threadWasCalled = 0;
            wakeUpSucceeded = 0;

            auto externalThreadLambda = [this, &eventLoop](Thread& thread)
            {
                thread.setThreadName(SC_NATIVE_STR("test"));
                threadWasCalled++;
                if (eventLoop.wakeUpFromExternalThread())
                {
                    wakeUpSucceeded++;
                }
            };
            SC_TEST_EXPECT(newThread.start(externalThreadLambda));
            Time::HighResolutionCounter start, end;
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(newThread.join());
            SC_TEST_EXPECT(threadWasCalled == 1);
            SC_TEST_EXPECT(wakeUpSucceeded == 1);
        }
    }

    int      wakeUp1Called   = 0;
    int      wakeUp2Called   = 0;
    uint64_t wakeUp1ThreadID = 0;

    void loopWakeUp()
    {
        if (test_section("loop wakeUp"))
        {
            wakeUp1Called   = 0;
            wakeUp2Called   = 0;
            wakeUp1ThreadID = 0;
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            AsyncLoopWakeUp wakeUp1;
            AsyncLoopWakeUp wakeUp2;

            wakeUp1.callback = [this](AsyncLoopWakeUp::Result& res)
            {
                wakeUp1ThreadID = Thread::CurrentThreadID();
                wakeUp1Called++;
                SC_TEST_EXPECT(res.async.stop());
            };
            SC_TEST_EXPECT(wakeUp1.start(eventLoop));
            wakeUp2.callback = [&](AsyncLoopWakeUp::Result& res)
            {
                wakeUp2Called++;
                SC_TEST_EXPECT(res.async.stop());
            };
            SC_TEST_EXPECT(wakeUp2.start(eventLoop));
            Thread newThread1;
            Thread newThread2;
            Result loopRes1 = Result(false);
            Result loopRes2 = Result(false);
            auto   action1  = [&](Thread& thread)
            {
                thread.setThreadName(SC_NATIVE_STR("test1"));
                loopRes1 = wakeUp1.wakeUp();
            };
            auto action2 = [&](Thread& thread)
            {
                thread.setThreadName(SC_NATIVE_STR("test2"));
                loopRes2 = wakeUp1.wakeUp();
            };
            SC_TEST_EXPECT(newThread1.start(action1));
            SC_TEST_EXPECT(newThread2.start(action2));
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(newThread2.join());
            SC_TEST_EXPECT(loopRes1);
            SC_TEST_EXPECT(loopRes2);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(wakeUp1Called == 1);
            SC_TEST_EXPECT(wakeUp2Called == 0);
            SC_TEST_EXPECT(wakeUp1ThreadID == Thread::CurrentThreadID());
        }
    }

    void loopWakeUpEventObject()
    {
        if (test_section("loop wakeUp eventObject"))
        {
            struct TestParams
            {
                int         notifier1Called         = 0;
                int         observedNotifier1Called = -1;
                EventObject eventObject;
                Result      loopRes1 = Result(false);
            } params;

            uint64_t callbackThreadID = 0;

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            AsyncLoopWakeUp wakeUp;

            wakeUp.callback = [&](AsyncLoopWakeUp::Result&)
            {
                callbackThreadID = Thread::CurrentThreadID();
                params.notifier1Called++;
            };
            SC_TEST_EXPECT(wakeUp.start(eventLoop, &params.eventObject));
            Thread newThread1;
            auto   threadLambda = [&params, &wakeUp](Thread& thread)
            {
                thread.setThreadName(SC_NATIVE_STR("test1"));
                params.loopRes1 = wakeUp.wakeUp();
                params.eventObject.wait();
                params.observedNotifier1Called = params.notifier1Called;
            };
            SC_TEST_EXPECT(newThread1.start(threadLambda));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.notifier1Called == 1);
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(params.loopRes1);
            SC_TEST_EXPECT(params.observedNotifier1Called == 1);
            SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
        }
    }

    void processExit()
    {
        if (test_section("process exit"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Process processSuccess;
            Process processFailure;
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(processSuccess.launch("where", "where.exe"));        // Returns 0 error code
            SC_TEST_EXPECT(processFailure.launch("cmd", "/C", "dir /DOCTORS")); // Returns 1 error code
#else
            // Must wait for the process to be still active when adding it to kqueue
            SC_TEST_EXPECT(processSuccess.launch("sleep", "0.1")); // Returns 0 error code
            SC_TEST_EXPECT(processFailure.launch("ls", "/~"));     // Returns 1 error code
#endif
            ProcessDescriptor::Handle processHandleSuccess = 0;
            SC_TEST_EXPECT(processSuccess.handle.get(processHandleSuccess, Result::Error("Invalid Handle 1")));
            ProcessDescriptor::Handle processHandleFailure = 0;
            SC_TEST_EXPECT(processFailure.handle.get(processHandleFailure, Result::Error("Invalid Handle 2")));
            AsyncProcessExit asyncSuccess;
            AsyncProcessExit asyncFailure;

            struct OutParams
            {
                int numCallbackCalled = 0;

                ProcessDescriptor::ExitStatus exitStatus = {-1};
            };
            OutParams outParams1;
            OutParams outParams2;
            asyncSuccess.callback = [&](AsyncProcessExit::Result& res)
            {
                SC_TEST_EXPECT(res.moveTo(outParams1.exitStatus));
                outParams1.numCallbackCalled++;
            };
            asyncFailure.callback = [&](AsyncProcessExit::Result& res)
            {
                SC_TEST_EXPECT(res.moveTo(outParams2.exitStatus));
                outParams2.numCallbackCalled++;
            };
            SC_TEST_EXPECT(asyncSuccess.start(eventLoop, processHandleSuccess));
            SC_TEST_EXPECT(asyncFailure.start(eventLoop, processHandleFailure));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(outParams1.numCallbackCalled == 1);
            SC_TEST_EXPECT(outParams1.exitStatus.status == 0); // Status == Ok
            SC_TEST_EXPECT(outParams2.numCallbackCalled == 1);
            SC_TEST_EXPECT(outParams2.exitStatus.status != 0); // Status == Not OK
        }
    }

    SocketDescriptor acceptedClient[3];
    int              acceptedCount = 0;

    void socketAccept()
    {
        if (test_section("socket accept"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            constexpr uint32_t numWaitingConnections = 2;
            SocketDescriptor   serverSocket;
            uint16_t           tcpPort = 5050;
            SocketIPAddress    nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            SC_TEST_EXPECT(SocketServer(serverSocket).listen(nativeAddress, numWaitingConnections));

            acceptedCount = 0;

            AsyncSocketAccept accept;
            accept.callback = [this](AsyncSocketAccept::Result& res)
            {
                SC_TEST_EXPECT(res.moveTo(acceptedClient[acceptedCount]));
                acceptedCount++;
                res.reactivateRequest(true);
            };
            SC_TEST_EXPECT(accept.start(eventLoop, serverSocket));

            SocketDescriptor client1, client2;
            SC_TEST_EXPECT(SocketClient(client1).connect("127.0.0.1", tcpPort));
            SC_TEST_EXPECT(SocketClient(client2).connect("127.0.0.1", tcpPort));
            SC_TEST_EXPECT(not acceptedClient[0].isValid());
            SC_TEST_EXPECT(not acceptedClient[1].isValid());
            SC_TEST_EXPECT(eventLoop.runOnce()); // first connect
            SC_TEST_EXPECT(eventLoop.runOnce()); // second connect
            SC_TEST_EXPECT(acceptedClient[0].isValid());
            SC_TEST_EXPECT(acceptedClient[1].isValid());
            SC_TEST_EXPECT(client1.close());
            SC_TEST_EXPECT(client2.close());
            SC_TEST_EXPECT(acceptedClient[0].close());
            SC_TEST_EXPECT(acceptedClient[1].close());

            SC_TEST_EXPECT(accept.stop());

            // on Windows stopAsync generates one more eventloop run because
            // of the closing of the clientsocket used for acceptex, so to unify
            // the behaviours in the test we do a runNoWait
            SC_TEST_EXPECT(eventLoop.runNoWait());

            SocketDescriptor client3;
            SC_TEST_EXPECT(SocketClient(client3).connect("127.0.0.1", tcpPort));

            // Now we need a runNoWait for both because there are for sure no other events to be dequeued
            SC_TEST_EXPECT(eventLoop.runNoWait());

            SC_TEST_EXPECT(not acceptedClient[2].isValid());
            SC_TEST_EXPECT(serverSocket.close());
            SC_TEST_EXPECT(eventLoop.close());
        }
    }

    void socketConnect()
    {
        if (test_section("socket connect"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor serverSocket;
            uint16_t         tcpPort        = 5050;
            StringView       connectAddress = "::1";
            SocketIPAddress  nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            SC_TEST_EXPECT(SocketServer(serverSocket).listen(nativeAddress, 2)); // 2 waiting connection

            acceptedCount = 0;

            AsyncSocketAccept accept;
            accept.callback = [&](AsyncSocketAccept::Result& res)
            {
                SC_TEST_EXPECT(res.moveTo(acceptedClient[acceptedCount]));
                acceptedCount++;
                res.reactivateRequest(acceptedCount < 2);
            };
            SC_TEST_EXPECT(accept.start(eventLoop, serverSocket));

            SocketIPAddress localHost;

            SC_TEST_EXPECT(localHost.fromAddressPort(connectAddress, tcpPort));

            AsyncSocketConnect connect[2];
            SocketDescriptor   clients[2];

            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[0]));
            int connectedCount  = 0;
            connect[0].callback = [&](AsyncSocketConnect::Result& res)
            {
                connectedCount++;
                SC_TEST_EXPECT(res.isValid());
            };
            SC_TEST_EXPECT(connect[0].start(eventLoop, clients[0], localHost));

            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[1]));
            connect[1].callback = connect[0].callback;
            SC_TEST_EXPECT(connect[1].start(eventLoop, clients[1], localHost));

            SC_TEST_EXPECT(connectedCount == 0);
            SC_TEST_EXPECT(acceptedCount == 0);
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(acceptedCount == 2);
            SC_TEST_EXPECT(connectedCount == 2);

            char       receiveBuffer[1] = {0};
            Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

            AsyncSocketReceive receiveAsync;
            int                receiveCalls = 0;
            receiveAsync.callback           = [&](AsyncSocketReceive::Result& res)
            {
                Span<char> readData;
                SC_TEST_EXPECT(res.moveTo(readData));
                SC_TEST_EXPECT(readData.data()[0] == 1);
                receiveCalls++;
            };
            SC_TEST_EXPECT(receiveAsync.start(eventLoop, acceptedClient[0], receiveData));
            char v = 1;
            SC_TEST_EXPECT(SocketClient(clients[0]).write({&v, 1}));
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(receiveCalls == 1);
        }
    }

    void createAndAssociateAsyncClientServerConnections(AsyncEventLoop& eventLoop, SocketDescriptor& client,
                                                        SocketDescriptor& serverSideClient)
    {
        SocketDescriptor serverSocket;
        uint16_t         tcpPort        = 5050;
        StringView       connectAddress = "::1";
        SocketIPAddress  nativeAddress;
        SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
        SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily()));
        SC_TEST_EXPECT(SocketServer(serverSocket).listen(nativeAddress, 0));

        SC_TEST_EXPECT(SocketClient(client).connect(connectAddress, tcpPort));
        SC_TEST_EXPECT(SocketServer(serverSocket).accept(nativeAddress.getAddressFamily(), serverSideClient));
        SC_TEST_EXPECT(client.setBlocking(false));
        SC_TEST_EXPECT(serverSideClient.setBlocking(false));

        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedTCPSocket(client));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedTCPSocket(serverSideClient));
    }

    void socketSendReceive()
    {
        if (test_section("socket send/receive"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SocketDescriptor client, serverSideClient;
            createAndAssociateAsyncClientServerConnections(eventLoop, client, serverSideClient);

            const char sendBuffer[] = {123, 111};

            Span<const char> sendData = {sendBuffer, sizeof(sendBuffer)};

            int             sendCount = 0;
            AsyncSocketSend sendAsync;
            sendAsync.callback = [&](AsyncSocketSend::Result& res)
            {
                SC_TEST_EXPECT(res.isValid());
                sendCount++;
            };

            SC_TEST_EXPECT(sendAsync.start(eventLoop, client, sendData));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(sendCount == 1);
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(sendCount == 1);

            char       receiveBuffer[1] = {0};
            Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

            AsyncSocketReceive receiveAsync;

            struct Params
            {
                int  receiveCount                     = 0;
                char receivedData[sizeof(sendBuffer)] = {0};
            };
            Params params;
            receiveAsync.callback = [this, &params](AsyncSocketReceive::Result& res)
            {
                Span<char> readData;
                SC_TEST_EXPECT(res.moveTo(readData));
                SC_TEST_EXPECT(readData.sizeInBytes() == 1);
                params.receivedData[params.receiveCount] = readData.data()[0];
                params.receiveCount++;
                res.reactivateRequest(size_t(params.receiveCount) < sizeof(sendBuffer));
            };
            SC_TEST_EXPECT(receiveAsync.start(eventLoop, serverSideClient, receiveData));
            SC_TEST_EXPECT(params.receiveCount == 0); // make sure we receive after run, in case of sync results
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(params.receiveCount == 2);
            SC_TEST_EXPECT(memcmp(params.receivedData, sendBuffer, sizeof(sendBuffer)) == 0);
        }
    }

    void socketClose()
    {
        if (test_section("socket close"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SocketDescriptor client, serverSideClient;
            createAndAssociateAsyncClientServerConnections(eventLoop, client, serverSideClient);

            AsyncSocketClose asyncClose1;

            int numCalledClose1  = 0;
            asyncClose1.callback = [&](AsyncSocketClose::Result& result)
            {
                numCalledClose1++;
                SC_TEST_EXPECT(result.isValid());
            };

            SC_TEST_EXPECT(asyncClose1.start(eventLoop, client));

            AsyncSocketClose asyncClose2;

            int numCalledClose2  = 0;
            asyncClose2.callback = [&](AsyncSocketClose::Result& result)
            {
                numCalledClose2++;
                SC_TEST_EXPECT(result.isValid());
            };
            SC_TEST_EXPECT(asyncClose2.start(eventLoop, serverSideClient));
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(numCalledClose1 == 1);
            SC_TEST_EXPECT(numCalledClose2 == 1);
        }
    }

    void fileReadWrite()
    {
        if (test_section("file read/write"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            StringNative<255> filePath = StringEncoding::Native;
            StringNative<255> dirPath  = StringEncoding::Native;
            const StringView  name     = "AsyncTest";
            const StringView  fileName = "test.txt";
            SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, name}));
            SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));

            FileDescriptor::OpenOptions options;
            options.async    = true;
            options.blocking = true;

            FileDescriptor fd;
            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::WriteCreateTruncate, options));
            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));

            auto writeSpan = StringView("test").toCharSpan();

            FileDescriptor::Handle handle = FileDescriptor::Invalid;
            SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

            AsyncFileWrite asyncWriteFile;
            asyncWriteFile.callback = [&](AsyncFileWrite::Result& res)
            {
                size_t writtenBytes = 0;
                SC_TEST_EXPECT(res.moveTo(writtenBytes));
                SC_TEST_EXPECT(writtenBytes == 4);
            };
            SC_TEST_EXPECT(asyncWriteFile.start(eventLoop, handle, writeSpan));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(fd.close());

            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::ReadOnly, options));
            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
            SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

            struct Params
            {
                int  readCount = 0;
                char readBuffer[4];
            };
            Params        params;
            AsyncFileRead asyncReadFile;
            asyncReadFile.callback = [this, &params](AsyncFileRead::Result& res)
            {
                Span<char> readData;
                SC_TEST_EXPECT(res.moveTo(readData));
                SC_TEST_EXPECT(readData.sizeInBytes() == 1);
                params.readBuffer[params.readCount++] = readData.data()[0];
                res.async.offset += readData.sizeInBytes();
                res.reactivateRequest(params.readCount < 4);
            };
            char buffer[1] = {0};
            SC_TEST_EXPECT(asyncReadFile.start(eventLoop, handle, {buffer, sizeof(buffer)}));
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(fd.close());

            StringView sv({params.readBuffer, sizeof(params.readBuffer)}, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(sv.compare("test") == StringView::Comparison::Equals);

            SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
            SC_TEST_EXPECT(fs.removeFile(fileName));
            SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
        }
    }

    void fileClose()
    {
        if (test_section("file close"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            StringNative<255> filePath = StringEncoding::Native;
            StringNative<255> dirPath  = StringEncoding::Native;
            const StringView  name     = "AsyncTest";
            const StringView  fileName = "test.txt";
            SC_TEST_EXPECT(Path::join(dirPath, {report.applicationRootDirectory, name}));
            SC_TEST_EXPECT(Path::join(filePath, {dirPath.view(), fileName}));

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(name));
            SC_TEST_EXPECT(fs.write(filePath.view(), "test"));

            FileDescriptor::OpenOptions options;
            options.async    = true;
            options.blocking = true;

            FileDescriptor fd;
            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::WriteCreateTruncate, options));
            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));

            FileDescriptor::Handle handle = FileDescriptor::Invalid;
            SC_TEST_EXPECT(fd.get(handle, Result::Error("handle")));
            AsyncFileClose asyncClose;
            asyncClose.callback = [this](auto& result) { SC_TEST_EXPECT(result.isValid()); };
            auto res            = asyncClose.start(eventLoop, handle);
            SC_TEST_EXPECT(res);
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
            SC_TEST_EXPECT(fs.removeFile(fileName));
            SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
            // fd.close() will fail as the file was already closed but it also throws a Win32 exception that will
            // stop the debugger by default. Opting for a .detach()
            // SC_TEST_EXPECT(not fd.close());
            fd.detach();
        }
    }

    void socketSendReceiveError()
    {
        if (test_section("error send/receive"))
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SocketDescriptor client, serverSideClient;
            createAndAssociateAsyncClientServerConnections(eventLoop, client, serverSideClient);

            // Setup send side on serverSideClient
            AsyncSocketSend asyncSend;
            asyncSend.setDebugName("server");
            char sendBuffer[1] = {1};

            {
                // Extract the raw handle from socket and close it
                // This will provoke the following failures:
                // - Apple: after poll on macOS (where we're pushing the async handles to OS)
                // - Windows: during Staging (precisely in Activate)
                SocketDescriptor::Handle handle = SocketDescriptor::Invalid;
                SC_TEST_EXPECT(serverSideClient.get(handle, Result::Error("ASD")));
                SocketDescriptor socketToClose;
                SC_TEST_EXPECT(socketToClose.assign(handle));
                SC_TEST_EXPECT(socketToClose.close());
            }
            int numOnSend      = 0;
            asyncSend.callback = [&](AsyncSocketSend::Result& result)
            {
                numOnSend++;
                SC_TEST_EXPECT(not result.isValid());
            };
            SC_TEST_EXPECT(asyncSend.start(eventLoop, serverSideClient, {sendBuffer, sizeof(sendBuffer)}));

            // Setup receive side on client
            char recvBuffer[1] = {1};

            int                numOnReceive = 0;
            AsyncSocketReceive asyncRecv;
            asyncRecv.setDebugName("client");
            asyncRecv.callback = [&](AsyncSocketReceive::Result& result)
            {
                numOnReceive++;
                SC_TEST_EXPECT(not result.isValid());
            };
            SC_TEST_EXPECT(asyncRecv.start(eventLoop, client, {recvBuffer, sizeof(recvBuffer)}));

            // This will fail because the receive async is not in Free state
            SC_TEST_EXPECT(not asyncRecv.start(eventLoop, client, {recvBuffer, sizeof(recvBuffer)}));

            // Just close the client to cause an error in the callback
            SC_TEST_EXPECT(client.close());

            AsyncSocketReceive asyncErr;
            asyncErr.setDebugName("asyncErr");
            // This will fail immediately as the socket is already closed before this call
            SC_TEST_EXPECT(not asyncErr.start(eventLoop, client, {recvBuffer, sizeof(recvBuffer)}));

            SC_TEST_EXPECT(eventLoop.run());

            SC_TEST_EXPECT(not asyncSend.stop());
            SC_TEST_EXPECT(eventLoop.run());

            SC_TEST_EXPECT(numOnSend == 1);
            SC_TEST_EXPECT(numOnReceive == 1);
        }
    }
};

namespace SC
{
void runAsyncTest(SC::TestReport& report) { AsyncTest test(report); }
} // namespace SC

namespace SC
{
// clang-format off
Result snippetForEventLoop()
{
//! [AsyncEventLoopSnippet]
AsyncEventLoop eventLoop;
SC_TRY(eventLoop.create()); // Create OS specific queue handles
// ...
// Add all needed AsyncRequest
// ...
SC_TRY(eventLoop.run());
// ...
// Here all AsyncRequest have either finished or have been stopped
// ...
SC_TRY(eventLoop.close()); // Free OS specific queue handles
//! [AsyncEventLoopSnippet]
return Result(true);
}

SC::Result snippetForTimeout(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncLoopTimeoutSnippet]
// Create a timeout that will be called after 200 milliseconds
// AsyncLoopTimeout must be valid until callback is called
AsyncLoopTimeout timeout;
timeout.callback = [&](AsyncLoopTimeout::Result&)
{
    console.print("My timeout has been called!");
};
// Start the timeout, that will be called 200 ms from now
SC_TRY(timeout.start(eventLoop, Time::Milliseconds(200)));
//! [AsyncLoopTimeoutSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForWakeUp1(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncLoopWakeUpSnippet1]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
// This code runs on some different thread from the one calling SC::AsyncEventLoop::run.
// The callback is invoked from the thread calling SC::AsyncEventLoop::run
AsyncLoopWakeUp wakeUp; // Memory lifetime must be valid until callback is called
wakeUp.callback = [&](AsyncLoopWakeUp::Result&)
{
    console.print("My wakeUp has been called!");
};
SC_TRY(wakeUp.start(eventLoop));
//! [AsyncLoopWakeUpSnippet1]
return Result(true);
}

SC::Result snippetForWakeUp2(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncLoopWakeUpSnippet2]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
// This code runs on some different thread from the one calling SC::AsyncEventLoop::run.
// The callback is invoked from the thread calling SC::AsyncEventLoop::run
AsyncLoopWakeUp wakeUpWaiting; // Memory lifetime must be valid until callback is called
wakeUpWaiting.callback = [&](AsyncLoopWakeUp::Result&)
{
    console.print("My wakeUp has been called!");
};
EventObject eventObject;
SC_TRY(wakeUpWaiting.start(eventLoop, &eventObject));
eventObject.wait(); // Wait until callback has been fully run inside event loop thread
// From here on we know for sure that callback has been called
//! [AsyncLoopWakeUpSnippet2]
return Result(true);
}

SC::Result snippetForProcess(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncProcessSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
Process process;
SC_TRY(process.launch("executable", "--parameter"));
ProcessDescriptor::Handle processHandle;
SC_TRY(process.handle.get(processHandle, Result::Error("Invalid Handle")));
AsyncProcessExit processExit; //  Memory lifetime must be valid until callback is called
processExit.callback = [&](AsyncProcessExit::Result& res)
{
    ProcessDescriptor::ExitStatus exitStatus;
    if(res.moveTo(exitStatus))
    {
        console.print("Process Exit status = {}", exitStatus.status);
    }
};
SC_TRY(processExit.start(eventLoop, processHandle));
//! [AsyncProcessSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketAccept(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncSocketAcceptSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
// Create a listening socket
constexpr uint32_t numWaitingConnections = 2;
SocketDescriptor   serverSocket;
uint16_t           tcpPort = 5050;
SocketIPAddress    nativeAddress;
SC_TRY(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
SC_TRY(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
SC_TRY(SocketServer(serverSocket).listen(nativeAddress, numWaitingConnections));
// Accept connect for new clients
AsyncSocketAccept accept;
accept.callback = [&](AsyncSocketAccept::Result& res)
{
    SocketDescriptor client;
    if(res.moveTo(client))
    {
        // ...do something with new client
        console.printLine("New client connected!");
        res.reactivateRequest(true); // We want to receive more clients
    }
};
SC_TRY(accept.start(eventLoop, serverSocket));
// ... at some later point
// Stop accepting new clients
SC_TRY(accept.stop());
//! [AsyncSocketAcceptSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketConnect(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncSocketConnectSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...
SocketIPAddress localHost;
SC_TRY(localHost.fromAddressPort("127.0.0.1", 5050)); // Connect to some host and port
AsyncSocketConnect connect;
SocketDescriptor   client;
SC_TRY(eventLoop.createAsyncTCPSocket(localHost.getAddressFamily(), client));
connect.callback = [&](AsyncSocketConnect::Result& res)
{
    if (res.isValid())
    {
        // Do something with client that is now connected
        console.printLine("Client connected");
    }
};
SC_TRY(connect.start(eventLoop, client, localHost));
//! [AsyncSocketConnectSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketSend(AsyncEventLoop& eventLoop, Console& console)
{
SocketDescriptor client;
//! [AsyncSocketSendSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// and a connected or accepted socket named `client`
// ...
const char sendBuffer[] = {123, 111};

// The memory pointed by the span must be valid until callback is called
Span<const char> sendData = {sendBuffer, sizeof(sendBuffer)};

AsyncSocketSend sendAsync;
sendAsync.callback = [&](AsyncSocketSend::Result& res)
{
    if(res.isValid())
    {
        // Now we could free the data pointed by span and queue new data
        console.printLine("Ready to send more data");
    }
};

SC_TRY(sendAsync.start(eventLoop, client, sendData));
//! [AsyncSocketSendSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketReceive(AsyncEventLoop& eventLoop, Console& console)
{
SocketDescriptor client;
//! [AsyncSocketReceiveSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// and a connected or accepted socket named `client`
// ...
AsyncSocketReceive receiveAsync;
char receivedData[100] = {0};
receiveAsync.callback = [&](AsyncSocketReceive::Result& res)
{
    Span<char> readData;
    if(res.moveTo(readData))
    {
        // readData now contains a slice of receivedData with the received bytes
        console.print("{} bytes have been read", readData.sizeInBytes());
    }
    // Ask to reactivate the request if we want to receive more data
    res.reactivateRequest(true);
};
SC_TRY(receiveAsync.start(eventLoop, client, {receivedData, sizeof(receivedData)}));
//! [AsyncSocketReceiveSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForSocketClose(AsyncEventLoop& eventLoop, Console& console)
{
SocketDescriptor client;
//! [AsyncSocketCloseSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// and a connected or accepted socket named `client`
// ...
AsyncSocketClose asyncClose;

asyncClose.callback = [&](AsyncSocketClose::Result& result)
{
    if(result.isValid())
    {
        console.printLine("Socket was closed successfully");
    }
};
SC_TRY(asyncClose.start(eventLoop, client));

//! [AsyncSocketCloseSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForFileRead(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncFileReadSnippet]
// Assuming an already created (and running) AsyncEventLoop named `eventLoop`
// ...

// Open the file
FileDescriptor fd;
FileDescriptor::OpenOptions options;
options.async    = true;
options.blocking = false;
SC_TRY(fd.open("MyFile.txt", FileDescriptor::ReadOnly, options));
FileDescriptor::Handle handle;

// Obtain file descriptor handle and associate it with event loop
SC_TRY(fd.get(handle, Result::Error("Invalid handle")));
SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(fd));

// Create the async file read request
AsyncFileRead asyncReadFile;
asyncReadFile.callback = [&](AsyncFileRead::Result& res)
{
    Span<char> readData;
    if(res.moveTo(readData))
    {
        console.print("Read {} bytes from file", readData.sizeInBytes());
        res.reactivateRequest(true); // Ask to receive more data
    }
};
char buffer[100] = {0};
SC_TRY(asyncReadFile.start(eventLoop, handle, {buffer, sizeof(buffer)}));
//! [AsyncFileReadSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}


SC::Result snippetForFileWrite(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncFileWriteSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...

// Open the file (for write)
FileDescriptor::OpenOptions options;
options.async    = true;
options.blocking = false;
FileDescriptor fd;
SC_TRY(fd.open("MyFile.txt", FileDescriptor::WriteCreateTruncate, options));
SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(fd));
auto writeSpan = StringView("test").toCharSpan();
FileDescriptor::Handle handle;
// Obtain file descriptor handle
SC_TRY(fd.get(handle, Result::Error("Invalid Handle")));

// Create the async file write request
AsyncFileWrite asyncWriteFile;
asyncWriteFile.callback = [&](AsyncFileWrite::Result& res)
{
    size_t writtenBytes = 0;
    if(res.moveTo(writtenBytes))
    {
        console.print("{} bytes have been written", writtenBytes);
    }
};
SC_TRY(asyncWriteFile.start(eventLoop, handle, writeSpan));
//! [AsyncFileWriteSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}

SC::Result snippetForFileClose(AsyncEventLoop& eventLoop, Console& console)
{
//! [AsyncFileCloseSnippet]
// Assuming an already created (and running) AsyncEventLoop named eventLoop
// ...

// Open a file and associated it with event loop
FileDescriptor fd;
FileDescriptor::OpenOptions options;
options.async    = true;
options.blocking = false;
SC_TRY(fd.open("MyFile.txt", FileDescriptor::WriteCreateTruncate, options));
SC_TRY(eventLoop.associateExternallyCreatedFileDescriptor(fd));

// Create the file close request
FileDescriptor::Handle handle;
SC_TRY(fd.get(handle, Result::Error("handle")));
AsyncFileClose asyncFileClose;
asyncFileClose.callback = [&](AsyncFileClose::Result& result)
{
    if(result.isValid())
    {
        console.printLine("File was closed successfully");
    }
};
SC_TRY(asyncFileClose.start(eventLoop, handle));
//! [AsyncFileCloseSnippet]
SC_TRY(eventLoop.run());
return Result(true);
}
// clang-format on
} // namespace SC
