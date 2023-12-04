// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../Async.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
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
        // TODO: Add EventLoop::resetTimeout
        if (test_section("loop timeout"))
        {
            Async::LoopTimeout timeout1, timeout2;
            Async::EventLoop   eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            int timeout1Called = 0;
            int timeout2Called = 0;
            timeout1.callback  = [&](Async::LoopTimeout::Result& res)
            {
                SC_TEST_EXPECT(res.async.getTimeout().ms == 1);
                timeout1Called++;
            };
            SC_TEST_EXPECT(timeout1.start(eventLoop, 1_ms));
            timeout2.callback = [&](Async::LoopTimeout::Result&)
            {
                // TODO: investigate allowing dropping Async::ResultBase
                timeout2Called++;
            };
            SC_TEST_EXPECT(timeout2.start(eventLoop, 100_ms));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 0);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 1);
        }
    }

    int  threadWasCalled = 0;
    int  wakeUpSucceded  = 0;
    void loopWakeUpFromExternalThread()
    {
        if (test_section("loop wakeUpFromExternalThread"))
        {
            Async::EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Thread newThread;
            threadWasCalled = 0;
            wakeUpSucceded  = 0;

            Action externalThreadLambda = [this, &eventLoop]()
            {
                threadWasCalled++;
                if (eventLoop.wakeUpFromExternalThread())
                {
                    wakeUpSucceded++;
                }
            };
            SC_TEST_EXPECT(newThread.start("test", &externalThreadLambda));
            Time::HighResolutionCounter start, end;
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(newThread.join());
            SC_TEST_EXPECT(threadWasCalled == 1);
            SC_TEST_EXPECT(wakeUpSucceded == 1);
        }
    }

    int      wakeUp1Called   = 0;
    int      wakeUp2Called   = 0;
    uint64_t wakeUp1ThreadID = 0;
    void     loopWakeUp()
    {
        if (test_section("loop wakeUp"))
        {
            wakeUp1Called   = 0;
            wakeUp2Called   = 0;
            wakeUp1ThreadID = 0;
            Async::EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Async::LoopWakeUp wakeUp1;
            Async::LoopWakeUp wakeUp2;

            wakeUp1.callback = [this](Async::LoopWakeUp::Result& res)
            {
                wakeUp1ThreadID = Thread::CurrentThreadID();
                wakeUp1Called++;
                SC_TEST_EXPECT(res.async.stop());
            };
            SC_TEST_EXPECT(wakeUp1.start(eventLoop));
            wakeUp2.callback = [&](Async::LoopWakeUp::Result& res)
            {
                wakeUp2Called++;
                SC_TEST_EXPECT(res.async.stop());
            };
            SC_TEST_EXPECT(wakeUp2.start(eventLoop));
            Thread newThread1;
            Thread newThread2;
            Result loopRes1 = Result(false);
            Result loopRes2 = Result(false);
            Action action1  = [&] { loopRes1 = wakeUp1.wakeUp(); };
            Action action2  = [&] { loopRes2 = wakeUp1.wakeUp(); };
            SC_TEST_EXPECT(newThread1.start("test1", &action1));
            SC_TEST_EXPECT(newThread2.start("test2", &action2));
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

            Async::EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Async::LoopWakeUp wakeUp;

            wakeUp.callback = [&](Async::LoopWakeUp::Result&)
            {
                callbackThreadID = Thread::CurrentThreadID();
                params.notifier1Called++;
            };
            SC_TEST_EXPECT(wakeUp.start(eventLoop, &params.eventObject));
            Thread newThread1;
            Action threadLambda = [&params, &wakeUp]
            {
                params.loopRes1 = wakeUp.wakeUp();
                params.eventObject.wait();
                params.observedNotifier1Called = params.notifier1Called;
            };
            SC_TEST_EXPECT(newThread1.start("test1", &threadLambda));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.notifier1Called == 1);
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(params.loopRes1);
            SC_TEST_EXPECT(params.observedNotifier1Called == 1);
            SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
        }
    }

    int              acceptedCount = 0;
    SocketDescriptor acceptedClient[3];
    void             socketAccept()
    {
        if (test_section("socket accept"))
        {
            Async::EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            constexpr uint32_t numWaitingConnections = 2;
            SocketDescriptor   serverSocket;
            uint16_t           tcpPort = 5050;
            SocketIPAddress    nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            SC_TEST_EXPECT(SocketServer(serverSocket).listen(nativeAddress, numWaitingConnections));

            acceptedCount = 0;

            Async::SocketAccept accept;
            accept.callback = [this](Async::SocketAccept::Result& res)
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
            Async::EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor serverSocket;
            uint16_t         tcpPort        = 5050;
            StringView       connectAddress = "::1";
            SocketIPAddress  nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            SC_TEST_EXPECT(SocketServer(serverSocket).listen(nativeAddress, 0));

            acceptedCount = 0;

            Async::SocketAccept accept;
            accept.callback = [&](Async::SocketAccept::Result& res)
            {
                SC_TEST_EXPECT(res.moveTo(acceptedClient[acceptedCount]));
                acceptedCount++;
                res.reactivateRequest(acceptedCount < 2);
            };
            SC_TEST_EXPECT(accept.start(eventLoop, serverSocket));

            SocketIPAddress localHost;

            SC_TEST_EXPECT(localHost.fromAddressPort(connectAddress, tcpPort));

            Async::SocketConnect connect[2];
            SocketDescriptor     clients[2];

            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[0]));
            int connectedCount  = 0;
            connect[0].callback = [&](Async::SocketConnect::Result& res)
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

            Async::SocketReceive receiveAsync;
            int                  receiveCalls = 0;
            receiveAsync.callback             = [&](Async::SocketReceive::Result& res)
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

    void createAndAssociateAsyncClientServerConnections(Async::EventLoop& eventLoop, SocketDescriptor& client,
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
            Async::EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SocketDescriptor client, serverSideClient;
            createAndAssociateAsyncClientServerConnections(eventLoop, client, serverSideClient);

            const char sendBuffer[] = {123, 111};

            Span<const char> sendData = {sendBuffer, sizeof(sendBuffer)};

            int               sendCount = 0;
            Async::SocketSend sendAsync;
            sendAsync.callback = [&](Async::SocketSend::Result& res)
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

            Async::SocketReceive receiveAsync;

            struct Params
            {
                int  receiveCount                     = 0;
                char receivedData[sizeof(sendBuffer)] = {0};
            };
            Params params;
            receiveAsync.callback = [this, &params](Async::SocketReceive::Result& res)
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
            Async::EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SocketDescriptor client, serverSideClient;
            createAndAssociateAsyncClientServerConnections(eventLoop, client, serverSideClient);

            Async::SocketClose asyncClose1;

            int numCalledClose1  = 0;
            asyncClose1.callback = [&](Async::SocketClose::Result& result)
            {
                numCalledClose1++;
                SC_TEST_EXPECT(result.isValid());
            };

            SC_TEST_EXPECT(asyncClose1.start(eventLoop, client));

            Async::SocketClose asyncClose2;

            int numCalledClose2  = 0;
            asyncClose2.callback = [&](Async::SocketClose::Result& result)
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
            Async::EventLoop eventLoop;
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
            options.blocking = false;

            FileDescriptor fd;
            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::WriteCreateTruncate, options));
            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));

            auto writeSpan = StringView("test").toCharSpan();

            FileDescriptor::Handle handle;
            SC_TEST_EXPECT(fd.get(handle, Result::Error("asd")));

            Async::FileWrite asyncWriteFile;
            asyncWriteFile.callback = [&](Async::FileWrite::Result& res)
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
            Params          params;
            Async::FileRead asyncReadFile;
            asyncReadFile.callback = [this, &params](Async::FileRead::Result& res)
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

            StringView sv(params.readBuffer, sizeof(params.readBuffer), false, StringEncoding::Ascii);
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
            Async::EventLoop eventLoop;
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
            options.blocking = false;

            FileDescriptor fd;
            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::WriteCreateTruncate, options));
            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));

            FileDescriptor::Handle handle;
            SC_TEST_EXPECT(fd.get(handle, Result::Error("handle")));
            Async::FileClose asyncClose;
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
            Async::EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SocketDescriptor client, serverSideClient;
            createAndAssociateAsyncClientServerConnections(eventLoop, client, serverSideClient);

            // Setup send side on serverSideClient
            Async::SocketSend asyncSend;
            asyncSend.setDebugName("server");
            char sendBuffer[1] = {1};

            {
                // Extract the raw handle from socket and close it
                // This will provoke the following failures:
                // - Apple: after poll on macOS (where we're pushing the async handles to OS)
                // - Windows: during Staging (precisely in Activate)
                SocketDescriptor::Handle handle;
                SC_TEST_EXPECT(serverSideClient.get(handle, Result::Error("ASD")));
                SocketDescriptor socketToClose;
                SC_TEST_EXPECT(socketToClose.assign(handle));
                SC_TEST_EXPECT(socketToClose.close());
            }
            int numOnSend      = 0;
            asyncSend.callback = [&](Async::SocketSend::Result& result)
            {
                numOnSend++;
                SC_TEST_EXPECT(not result.isValid());
            };
            SC_TEST_EXPECT(asyncSend.start(eventLoop, serverSideClient, {sendBuffer, sizeof(sendBuffer)}));

            // Setup receive side on client
            char recvBuffer[1] = {1};

            int                  numOnReceive = 0;
            Async::SocketReceive asyncRecv;
            asyncRecv.setDebugName("client");
            asyncRecv.callback = [&](Async::SocketReceive::Result& result)
            {
                numOnReceive++;
                SC_TEST_EXPECT(not result.isValid());
            };
            SC_TEST_EXPECT(asyncRecv.start(eventLoop, client, {recvBuffer, sizeof(recvBuffer)}));

            // This will fail because the receive async is not in Free state
            SC_TEST_EXPECT(not asyncRecv.start(eventLoop, client, {recvBuffer, sizeof(recvBuffer)}));

            // Just close the client to cause an error in the callback
            SC_TEST_EXPECT(client.close());

            Async::SocketReceive asyncErr;
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