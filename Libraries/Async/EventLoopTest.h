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
        timeout();
        wakeUpFromExternalThread();
        wakeUp();
        wakeupEventObject();
        accept();
        connect();
        sendReceive();
        readWrite();
    }

    void timeout()
    {
        // TODO: Add EventLoop::resetTimeout
        if (test_section("timeout"))
        {
            AsyncTimeout timeout1, timeout2;
            EventLoop    eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            int  timeout1Called   = 0;
            int  timeout2Called   = 0;
            auto timeout1Callback = [&](AsyncResult::Timeout& res)
            {
                SC_TEST_EXPECT(res.async.timeout.ms == 1);
                timeout1Called++;
            };
            SC_TEST_EXPECT(eventLoop.startTimeout(timeout1, 1_ms, move(timeout1Callback)));
            auto timeout2Callback = [&](AsyncResult::Timeout&)
            {
                // TODO: investigate allowing dropping AsyncResult
                timeout2Called++;
            };
            SC_TEST_EXPECT(eventLoop.startTimeout(timeout2, 100_ms, move(timeout2Callback)));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 0);
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(timeout1Called == 1 and timeout2Called == 1);
        }
    }

    void wakeUpFromExternalThread()
    {
        if (test_section("wakeUpFromExternalThread"))
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

    void wakeUp()
    {
        if (test_section("wakeUp"))
        {
            int       wakeUp1Called   = 0;
            int       wakeUp2Called   = 0;
            uint64_t  wakeUp1ThreadID = 0;
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            AsyncWakeUp wakeUp1;
            AsyncWakeUp wakeUp2;

            auto lambda1 = [&](AsyncResult::WakeUp& res)
            {
                wakeUp1ThreadID = Thread::CurrentThreadID();
                wakeUp1Called++;
                SC_TEST_EXPECT(res.async.eventLoop->stopAsync(res.async));
            };
            SC_TEST_EXPECT(eventLoop.startWakeUp(wakeUp1, lambda1));
            auto lambda2 = [&](AsyncResult::WakeUp& res)
            {
                wakeUp2Called++;
                SC_TEST_EXPECT(res.async.eventLoop->stopAsync(res.async));
            };
            SC_TEST_EXPECT(eventLoop.startWakeUp(wakeUp2, lambda2));
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

    void wakeupEventObject()
    {
        if (test_section("wakeUp-eventObject"))
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
            AsyncWakeUp wakeUp;

            auto eventLoopLambda = [&](AsyncResult::WakeUp&)
            {
                callbackThreadID = Thread::CurrentThreadID();
                params.notifier1Called++;
            };
            SC_TEST_EXPECT(eventLoop.startWakeUp(wakeUp, eventLoopLambda, &params.eventObject));
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

    void accept()
    {
        if (test_section("accept"))
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

            auto onAccepted = [&](AsyncResult::Accept& res)
            {
                SC_TEST_EXPECT(acceptedClient[acceptedCount].assign(move(res.acceptedClient)));
                acceptedCount++;
                res.rearm(true);
            };
            AsyncAccept accept;
            SC_TEST_EXPECT(eventLoop.startAccept(accept, serverSocket, onAccepted));

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

    void connect()
    {
        if (test_section("connect"))
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

            auto onAccepted = [&](AsyncResult::Accept& res)
            {
                SC_TEST_EXPECT(acceptedClient[acceptedCount].assign(move(res.acceptedClient)));
                acceptedCount++;
                res.rearm(acceptedCount < 2);
            };
            AsyncAccept accept;
            SC_TEST_EXPECT(eventLoop.startAccept(accept, serverSocket, onAccepted));

            int  connectedCount = 0;
            auto onConnected    = [&](AsyncResult::Connect& res)
            {
                connectedCount++;
                SC_UNUSED(res);
            };
            SocketIPAddress localHost;

            SC_TEST_EXPECT(localHost.fromAddressPort(connectAddress, tcpPort));

            AsyncConnect     connect[2];
            SocketDescriptor clients[2];

            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[0]));
            SC_TEST_EXPECT(eventLoop.startConnect(connect[0], clients[0], localHost, onConnected));

            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), clients[1]));
            SC_TEST_EXPECT(eventLoop.startConnect(connect[1], clients[1], localHost, onConnected));

            SC_TEST_EXPECT(connectedCount == 0);
            SC_TEST_EXPECT(acceptedCount == 0);
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(acceptedCount == 2);
            SC_TEST_EXPECT(connectedCount == 2);

            int  receiveCalls = 0;
            auto onReceive    = [&](AsyncResult::Receive& res)
            {
                SC_TEST_EXPECT(res.readData.data()[0] == 1);
                receiveCalls++;
            };

            char       receiveBuffer[1] = {0};
            Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

            AsyncReceive receiveAsync;
            SC_TEST_EXPECT(eventLoop.startReceive(receiveAsync, acceptedClient[0], receiveData, onReceive));
            char v = 1;
            SC_TEST_EXPECT(SocketClient(clients[0]).write({&v, 1}));
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(receiveCalls == 1);
        }
    }

    void sendReceive()
    {
        if (test_section("send/receive"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor serverSocket;
            uint16_t         tcpPort        = 5050;
            StringView       connectAddress = "::1";
            SocketIPAddress  nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
            SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily()));
            SC_TEST_EXPECT(SocketServer(serverSocket).listen(nativeAddress, 0));

            SocketDescriptor client;
            SC_TEST_EXPECT(SocketClient(client).connect(connectAddress, tcpPort));
            SocketDescriptor serverSideClient;
            SC_TEST_EXPECT(SocketServer(serverSocket).accept(SocketFlags::AddressFamilyIPV6, serverSideClient));
            SC_TEST_EXPECT(client.setBlocking(false));
            SC_TEST_EXPECT(serverSideClient.setBlocking(false));

            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedTCPSocket(client));
            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedTCPSocket(serverSideClient));

            const char sendBuffer[] = {123, 111};

            Span<const char> sendData = {sendBuffer, sizeof(sendBuffer)};

            int  sendCount = 0;
            auto onSend    = [&](AsyncResult::Send& res)
            {
                SC_UNUSED(res);
                sendCount++;
            };

            AsyncSend sendAsync;
            SC_TEST_EXPECT(eventLoop.startSend(sendAsync, client, sendData, onSend));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(sendCount == 1);
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(sendCount == 1);

            int  receiveCount                     = 0;
            char receivedData[sizeof(sendBuffer)] = {0};
            auto onReceive                        = [&](AsyncResult::Receive& res)
            {
                SC_TEST_EXPECT(res.readData.sizeInBytes() == 1);
                receivedData[receiveCount] = res.readData.data()[0];
                receiveCount++;
                res.rearm(size_t(receiveCount) < sizeof(sendBuffer));
            };

            char       receiveBuffer[1] = {0};
            Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

            AsyncReceive receiveAsync;
            SC_TEST_EXPECT(eventLoop.startReceive(receiveAsync, serverSideClient, receiveData, onReceive));
            SC_TEST_EXPECT(receiveCount == 0); // make sure we receive after run, in case of sync results
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(receiveCount == 2);
            SC_TEST_EXPECT(memcmp(receivedData, sendBuffer, sizeof(sendBuffer)) == 0);
        }
    }

    void readWrite()
    {
        if (test_section("read/write"))
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

            auto writeLambda = [&](AsyncResult::Write& res) { SC_TEST_EXPECT(res.writtenBytes == 4); };
            auto writeSpan   = StringView("test").toCharSpan();

            FileDescriptor::Handle handle;
            SC_TEST_EXPECT(fd.get(handle, "asd"_a8));

            AsyncWrite write;
            SC_TEST_EXPECT(eventLoop.startWrite(write, handle, writeSpan, writeLambda));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(fd.close());

            SC_TEST_EXPECT(fd.open(filePath.view(), FileDescriptor::ReadOnly, options));
            SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(fd));
            SC_TEST_EXPECT(fd.get(handle, "asd"_a8));

            int  readCount = 0;
            char readBuffer[4];
            auto readLambda = [&](AsyncResult::Read& res)
            {
                SC_TEST_EXPECT(res.readData.sizeInBytes() == 1);
                readBuffer[readCount++] = res.readData.data()[0];
                res.async.offset += res.readData.sizeInBytes();
                res.rearm(readCount < 4);
            };
            AsyncRead read;
            char      buffer[1] = {0};
            SC_TEST_EXPECT(eventLoop.startRead(read, handle, {buffer, sizeof(buffer)}, readLambda));
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
};
