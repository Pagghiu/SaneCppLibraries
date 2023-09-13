// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../FileSystem/FileSystem.h"
#include "../FileSystem/Path.h"
#include "../Foundation/String.h"
#include "../Testing/Test.h"
#include "../Threading/Threading.h" // EventObject
#include "EventLoop.h"

namespace SC
{
struct EventLoopTest;
}

struct SC::EventLoopTest : public SC::TestCase
{
    EventLoopTest(SC::TestReport& report) : TestCase(report, "EventLoopTest")
    {
        loopTimeout();
        loopWakeUpFromExternalThread();
        loopWakeUp();
        loopWakeUpEventObject();
        socketAccept();
        socketConnect();
        socketSendReceive();
        socketSendReceiveError();
        fileReadWrite();
    }

    void loopTimeout()
    {
        // TODO: Add EventLoop::resetTimeout
        if (test_section("loop timeout"))
        {
            AsyncLoopTimeout timeout1, timeout2;
            EventLoop        eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            int  timeout1Called   = 0;
            int  timeout2Called   = 0;
            auto timeout1Callback = [&](AsyncResult::LoopTimeout& res)
            {
                SC_TEST_EXPECT(res.async.timeout.ms == 1);
                timeout1Called++;
            };
            SC_TEST_EXPECT(eventLoop.startLoopTimeout(timeout1, 1_ms, move(timeout1Callback)));
            auto timeout2Callback = [&](AsyncResult::LoopTimeout&)
            {
                // TODO: investigate allowing dropping AsyncResult
                timeout2Called++;
            };
            SC_TEST_EXPECT(eventLoop.startLoopTimeout(timeout2, 100_ms, move(timeout2Callback)));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 0);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 1);
        }
    }

    void loopWakeUpFromExternalThread()
    {
        if (test_section("loop wakeUpFromExternalThread"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            Thread newThread;
            int    threadWasCalled = 0;
            int    wakeUpSucceded  = 0;

            Action externalThreadLambda = [&]
            {
                threadWasCalled++;
                if (eventLoop.wakeUpFromExternalThread())
                {
                    wakeUpSucceded++;
                }
            };
            SC_TEST_EXPECT(newThread.start("test", &externalThreadLambda));
            TimeCounter start, end;
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(newThread.join());
            SC_TEST_EXPECT(threadWasCalled == 1);
            SC_TEST_EXPECT(wakeUpSucceded == 1);
        }
    }

    void loopWakeUp()
    {
        if (test_section("loop wakeUp"))
        {
            int       wakeUp1Called   = 0;
            int       wakeUp2Called   = 0;
            uint64_t  wakeUp1ThreadID = 0;
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            AsyncLoopWakeUp wakeUp1;
            AsyncLoopWakeUp wakeUp2;

            auto lambda1 = [&](AsyncResult::LoopWakeUp& res)
            {
                wakeUp1ThreadID = Thread::CurrentThreadID();
                wakeUp1Called++;
                SC_TEST_EXPECT(res.async.eventLoop->stopAsync(res.async));
            };
            SC_TEST_EXPECT(eventLoop.startLoopWakeUp(wakeUp1, lambda1));
            auto lambda2 = [&](AsyncResult::LoopWakeUp& res)
            {
                wakeUp2Called++;
                SC_TEST_EXPECT(res.async.eventLoop->stopAsync(res.async));
            };
            SC_TEST_EXPECT(eventLoop.startLoopWakeUp(wakeUp2, lambda2));
            Thread     newThread1;
            Thread     newThread2;
            ReturnCode loopRes1 = false;
            ReturnCode loopRes2 = false;
            Action     action1  = [&] { loopRes1 = wakeUp1.wakeUp(); };
            Action     action2  = [&] { loopRes2 = wakeUp1.wakeUp(); };
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
            } params;

            uint64_t callbackThreadID = 0;

            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            AsyncLoopWakeUp wakeUp;

            auto eventLoopLambda = [&](AsyncResult::LoopWakeUp&)
            {
                callbackThreadID = Thread::CurrentThreadID();
                params.notifier1Called++;
            };
            SC_TEST_EXPECT(eventLoop.startLoopWakeUp(wakeUp, eventLoopLambda, &params.eventObject));
            Thread     newThread1;
            ReturnCode loopRes1     = false;
            Action     threadLambda = [&]
            {
                loopRes1 = wakeUp.wakeUp();
                params.eventObject.wait();
                params.observedNotifier1Called = params.notifier1Called;
            };
            SC_TEST_EXPECT(newThread1.start("test1", &threadLambda));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(params.notifier1Called == 1);
            SC_TEST_EXPECT(newThread1.join());
            SC_TEST_EXPECT(loopRes1);
            SC_TEST_EXPECT(params.observedNotifier1Called == 1);
            SC_TEST_EXPECT(callbackThreadID == Thread::CurrentThreadID());
        }
    }

    void socketAccept()
    {
        if (test_section("socket accept"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            constexpr uint32_t numWaitingConnections = 2;
            SocketDescriptor   serverSocket;
            uint16_t           tcpPort = 5050;
            SocketIPAddress    nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            SC_TEST_EXPECT(SocketServer(serverSocket).listen(nativeAddress, numWaitingConnections));

            int              acceptedCount = 0;
            SocketDescriptor acceptedClient[3];

            auto onAccepted = [&](AsyncResult::SocketAccept& res)
            {
                SC_TEST_EXPECT(res.moveTo(acceptedClient[acceptedCount]));
                acceptedCount++;
                res.reactivateRequest(true);
            };
            AsyncSocketAccept accept;
            SC_TEST_EXPECT(eventLoop.startSocketAccept(accept, serverSocket, onAccepted));

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

            SC_TEST_EXPECT(eventLoop.stopAsync(accept));

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
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor serverSocket;
            uint16_t         tcpPort        = 5050;
            StringView       connectAddress = "::1";
            SocketIPAddress  nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            SC_TEST_EXPECT(SocketServer(serverSocket).listen(nativeAddress, 0));

            int              acceptedCount = 0;
            SocketDescriptor acceptedClient[3];

            auto onAccepted = [&](AsyncResult::SocketAccept& res)
            {
                SC_TEST_EXPECT(res.moveTo(acceptedClient[acceptedCount]));
                acceptedCount++;
                res.reactivateRequest(acceptedCount < 2);
            };
            AsyncSocketAccept accept;
            SC_TEST_EXPECT(eventLoop.startSocketAccept(accept, serverSocket, onAccepted));

            int  connectedCount = 0;
            auto onConnected    = [&](AsyncResult::SocketConnect& res)
            {
                connectedCount++;
                SC_TEST_EXPECT(res.isValid());
            };
            SocketIPAddress localHost;

            SC_TEST_EXPECT(localHost.fromAddressPort(connectAddress, tcpPort));

            AsyncSocketConnect connect[2];
            SocketDescriptor   clients[2];

            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[0]));
            SC_TEST_EXPECT(eventLoop.startSocketConnect(connect[0], clients[0], localHost, onConnected));

            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[1]));
            SC_TEST_EXPECT(eventLoop.startSocketConnect(connect[1], clients[1], localHost, onConnected));

            SC_TEST_EXPECT(connectedCount == 0);
            SC_TEST_EXPECT(acceptedCount == 0);
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(acceptedCount == 2);
            SC_TEST_EXPECT(connectedCount == 2);

            int  receiveCalls = 0;
            auto onReceive    = [&](AsyncResult::SocketReceive& res)
            {
                Span<char> readData;
                SC_TEST_EXPECT(res.moveTo(readData));
                SC_TEST_EXPECT(readData.data()[0] == 1);
                receiveCalls++;
            };

            char       receiveBuffer[1] = {0};
            Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

            AsyncSocketReceive receiveAsync;
            SC_TEST_EXPECT(eventLoop.startSocketReceive(receiveAsync, acceptedClient[0], receiveData, onReceive));
            char v = 1;
            SC_TEST_EXPECT(SocketClient(clients[0]).write({&v, 1}));
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(receiveCalls == 1);
        }
    }

    void createAndAssociateAsyncClientServerConnections(EventLoop& eventLoop, SocketDescriptor& client,
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
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SocketDescriptor client, serverSideClient;
            createAndAssociateAsyncClientServerConnections(eventLoop, client, serverSideClient);

            const char sendBuffer[] = {123, 111};

            Span<const char> sendData = {sendBuffer, sizeof(sendBuffer)};

            int  sendCount = 0;
            auto onSend    = [&](AsyncResult::SocketSend& res)
            {
                SC_TEST_EXPECT(res.isValid());
                sendCount++;
            };

            AsyncSocketSend sendAsync;
            SC_TEST_EXPECT(eventLoop.startSocketSend(sendAsync, client, sendData, onSend));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(sendCount == 1);
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(sendCount == 1);

            int  receiveCount                     = 0;
            char receivedData[sizeof(sendBuffer)] = {0};
            auto onReceive                        = [&](AsyncResult::SocketReceive& res)
            {
                Span<char> readData;
                SC_TEST_EXPECT(res.moveTo(readData));
                SC_TEST_EXPECT(readData.sizeInBytes() == 1);
                receivedData[receiveCount] = readData.data()[0];
                receiveCount++;
                res.reactivateRequest(size_t(receiveCount) < sizeof(sendBuffer));
            };

            char       receiveBuffer[1] = {0};
            Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

            AsyncSocketReceive receiveAsync;
            SC_TEST_EXPECT(eventLoop.startSocketReceive(receiveAsync, serverSideClient, receiveData, onReceive));
            SC_TEST_EXPECT(receiveCount == 0); // make sure we receive after run, in case of sync results
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(receiveCount == 2);
            SC_TEST_EXPECT(memcmp(receivedData, sendBuffer, sizeof(sendBuffer)) == 0);
        }
    }

    void fileReadWrite()
    {
        if (test_section("file read/write"))
        {
            EventLoop eventLoop;
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

            auto writeLambda = [&](AsyncResult::FileWrite& res)
            {
                size_t writtenBytes = 0;
                SC_TEST_EXPECT(res.moveTo(writtenBytes));
                SC_TEST_EXPECT(writtenBytes == 4);
            };
            auto writeSpan = StringView("test").toCharSpan();

            FileDescriptor::Handle handle;
            SC_TEST_EXPECT(fd.get(handle, "asd"_a8));

            AsyncFileWrite write;
            SC_TEST_EXPECT(eventLoop.startFileWrite(write, handle, writeSpan, writeLambda));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(fd.close());

            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::ReadOnly, options));
            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
            SC_TEST_EXPECT(fd.get(handle, "asd"_a8));

            int  readCount = 0;
            char readBuffer[4];
            auto readLambda = [&](AsyncResult::FileRead& res)
            {
                Span<char> readData;
                SC_TEST_EXPECT(res.moveTo(readData));
                SC_TEST_EXPECT(readData.sizeInBytes() == 1);
                readBuffer[readCount++] = readData.data()[0];
                res.async.offset += readData.sizeInBytes();
                res.reactivateRequest(readCount < 4);
            };
            AsyncFileRead read;
            char          buffer[1] = {0};
            SC_TEST_EXPECT(eventLoop.startFileRead(read, handle, {buffer, sizeof(buffer)}, readLambda));
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(fd.close());

            StringView sv(readBuffer, sizeof(readBuffer), false, StringEncoding::Ascii);
            SC_TEST_EXPECT(sv.compare("test") == StringView::Comparison::Equals);
            SC_TEST_EXPECT(fs.changeDirectory(dirPath.view()));
            SC_TEST_EXPECT(fs.removeFile(fileName));
            SC_TEST_EXPECT(fs.changeDirectory(report.applicationRootDirectory));
            SC_TEST_EXPECT(fs.removeEmptyDirectory(name));
        }
    }

    void socketSendReceiveError()
    {
        if (test_section("error send/receive"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            SocketDescriptor client, serverSideClient;
            createAndAssociateAsyncClientServerConnections(eventLoop, client, serverSideClient);

            // Setup send side on serverSideClient
            AsyncSocketSend asyncSend;
            asyncSend.debugName = "server";
            char sendBuffer[1]  = {1};
            int  numOnSend      = 0;
            auto onSend         = [&](AsyncSocketSendResult& result)
            {
                numOnSend++;
                SC_TEST_EXPECT(not result.isValid());
            };
            {
                // Extract the raw handle from socket and close it
                // This will provoke the following failures:
                // - Apple: after poll on macOS (where we're pushing the async handles to OS)
                // - Windows: during Staging (precisely in Activate)
                SocketDescriptor::Handle handle;
                SC_TEST_EXPECT(serverSideClient.get(handle, "ASD"_a8));
                SocketDescriptor socketToClose;
                SC_TEST_EXPECT(socketToClose.assign(handle));
                SC_TEST_EXPECT(socketToClose.close());
            }
            SC_TEST_EXPECT(
                eventLoop.startSocketSend(asyncSend, serverSideClient, {sendBuffer, sizeof(sendBuffer)}, onSend));

            // Setup receive side on client
            char recvBuffer[1] = {1};
            int  numOnReceive  = 0;
            auto onReceive     = [&](AsyncSocketReceiveResult& result)
            {
                numOnReceive++;
                SC_TEST_EXPECT(not result.isValid());
            };

            AsyncSocketReceive asyncRecv;
            asyncRecv.debugName = "client";
            SC_TEST_EXPECT(
                eventLoop.startSocketReceive(asyncRecv, client, {recvBuffer, sizeof(recvBuffer)}, onReceive));

            // This will fail because the receive async is not in Free state
            SC_TEST_EXPECT(
                not eventLoop.startSocketReceive(asyncRecv, client, {recvBuffer, sizeof(recvBuffer)}, onReceive));

            // Just close the client to cause an error in the callback
            SC_TEST_EXPECT(client.close());

            AsyncSocketReceive asyncErr;
            asyncErr.debugName = "asyncErr";
            // This will fail immediately as the socket is already closed before this call
            SC_TEST_EXPECT(
                not eventLoop.startSocketReceive(asyncErr, client, {recvBuffer, sizeof(recvBuffer)}, onReceive));

            SC_TEST_EXPECT(eventLoop.run());

            SC_TEST_EXPECT(not eventLoop.stopAsync(asyncSend));
            SC_TEST_EXPECT(eventLoop.run());

            SC_TEST_EXPECT(numOnSend == 1);
            SC_TEST_EXPECT(numOnReceive == 1);
        }
    }
};
