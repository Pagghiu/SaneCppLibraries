// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Foundation/Buffer.h"

void SC::AsyncTest::createTCPSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& client,
                                        SocketDescriptor& serverSideClient)
{
    SocketDescriptor serverSocket;
    uint16_t         tcpPort        = 5050;
    StringView       connectAddress = "::1";
    SocketIPAddress  nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
    SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily()));

    {
        SocketServer server(serverSocket);
        SC_TEST_EXPECT(server.bind(nativeAddress));
        SC_TEST_EXPECT(server.listen(0));
    }

    SC_TEST_EXPECT(client.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(SocketClient(client).connect(connectAddress, tcpPort));
    SC_TEST_EXPECT(SocketServer(serverSocket).accept(nativeAddress.getAddressFamily(), serverSideClient));
    SC_TEST_EXPECT(client.setBlocking(false));
    SC_TEST_EXPECT(serverSideClient.setBlocking(false));

    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(client));
    SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(serverSideClient));
}

void SC::AsyncTest::socketTCPAccept()
{
    struct Context
    {
        int              acceptedCount = 0;
        SocketDescriptor acceptedClient[3];
    } context;
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    constexpr uint32_t numWaitingConnections = 2;
    SocketDescriptor   serverSocket;
    uint16_t           tcpPort = 5050;
    SocketIPAddress    nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    {
        SocketServer server(serverSocket);
        SC_TEST_EXPECT(server.bind(nativeAddress));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    AsyncSocketAccept accept;
    accept.setDebugName("Accept");
    accept.callback = [this, &context](AsyncSocketAccept::Result& res)
    {
        SC_TEST_EXPECT(res.moveTo(context.acceptedClient[context.acceptedCount]));
        context.acceptedCount++;
        SC_TEST_EXPECT(context.acceptedCount < 3);
        res.reactivateRequest(true);
    };
    SC_TEST_EXPECT(accept.start(eventLoop, serverSocket));

    SocketDescriptor client1, client2;
    SC_TEST_EXPECT(client1.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(client2.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(SocketClient(client1).connect("127.0.0.1", tcpPort));
    SC_TEST_EXPECT(SocketClient(client2).connect("127.0.0.1", tcpPort));
    SC_TEST_EXPECT(not context.acceptedClient[0].isValid());
    SC_TEST_EXPECT(not context.acceptedClient[1].isValid());
    SC_TEST_EXPECT(eventLoop.runOnce()); // first connect
    SC_TEST_EXPECT(eventLoop.runOnce()); // second connect
    SC_TEST_EXPECT(context.acceptedClient[0].isValid());
    SC_TEST_EXPECT(context.acceptedClient[1].isValid());
    SC_TEST_EXPECT(client1.close());
    SC_TEST_EXPECT(client2.close());
    SC_TEST_EXPECT(context.acceptedClient[0].close());
    SC_TEST_EXPECT(context.acceptedClient[1].close());

    int afterStopCalled = 0;

    Function<void(AsyncResult&)> afterStopped = [&](AsyncResult&) { afterStopCalled++; };

    SC_TEST_EXPECT(accept.stop(eventLoop, &afterStopped));
    SC_TEST_EXPECT(afterStopCalled == 0);

    SC_TEST_EXPECT(eventLoop.runOnce());
    SC_TEST_EXPECT(afterStopCalled == 1);

    SocketDescriptor client3;
    SC_TEST_EXPECT(client3.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(SocketClient(client3).connect("127.0.0.1", tcpPort));

    // Now we need a runNoWait for both because there are for sure no other events to be dequeued
    SC_TEST_EXPECT(eventLoop.runNoWait());

    SC_TEST_EXPECT(not context.acceptedClient[2].isValid());
    SC_TEST_EXPECT(serverSocket.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::socketTCPConnect()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));

    SocketDescriptor serverSocket;
    uint16_t         tcpPort        = 5050;
    StringView       connectAddress = "::1";
    SocketIPAddress  nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort(connectAddress, tcpPort));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));

    {
        SocketServer server(serverSocket);
        SC_TEST_EXPECT(server.bind(nativeAddress));
        SC_TEST_EXPECT(server.listen(2)); // 2 waiting connections
    }

    struct Context
    {
        int              acceptedCount = 0;
        SocketDescriptor acceptedClient[3];
    } context;

    AsyncSocketAccept accept;
    accept.callback = [this, &context](AsyncSocketAccept::Result& res)
    {
        SC_TEST_EXPECT(res.moveTo(context.acceptedClient[context.acceptedCount]));
        context.acceptedCount++;
        res.reactivateRequest(context.acceptedCount < 2);
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
    SC_TEST_EXPECT(context.acceptedCount == 0);
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.acceptedCount == 2);
    SC_TEST_EXPECT(connectedCount == 2);

    char       receiveBuffer[1] = {0};
    Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

    AsyncSocketReceive receiveAsync;
    int                receiveCalls = 0;
    receiveAsync.callback           = [&](AsyncSocketReceive::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        SC_TEST_EXPECT(readData.data()[0] == 1);
        receiveCalls++;
    };
    SC_TEST_EXPECT(receiveAsync.start(eventLoop, context.acceptedClient[0], receiveData));
    char v = 1;
    SC_TEST_EXPECT(SocketClient(clients[0]).write({&v, 1}));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(receiveCalls == 1);
    SC_TEST_EXPECT(context.acceptedClient[0].close());
    SC_TEST_EXPECT(context.acceptedClient[1].close());
}

void SC::AsyncTest::socketTCPSendReceive()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketDescriptor client, serverSideClient;
    createTCPSocketPair(eventLoop, client, serverSideClient);

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
        int  sizeOfSendBuffer                 = sizeof(sendBuffer); // Need this for vs2019
    };
    Params params;
    receiveAsync.callback = [this, &params](AsyncSocketReceive::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        SC_TEST_EXPECT(readData.sizeInBytes() == 1);
        params.receivedData[params.receiveCount] = readData.data()[0];
        params.receiveCount++;
        res.reactivateRequest(params.receiveCount < params.sizeOfSendBuffer);
    };
    SC_TEST_EXPECT(receiveAsync.start(eventLoop, serverSideClient, receiveData));
    SC_TEST_EXPECT(params.receiveCount == 0); // make sure we receive after run, in case of sync results
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(params.receiveCount == 2);
    SC_TEST_EXPECT(memcmp(params.receivedData, sendBuffer, sizeof(sendBuffer)) == 0);

    // Test sending large data
    constexpr size_t largeBufferSize = 1024 * 1024; // 1Mb
    Buffer           sendBufferLarge, receiveBufferLarge;
    SC_TEST_EXPECT(sendBufferLarge.resize(largeBufferSize));
    SC_TEST_EXPECT(receiveBufferLarge.resizeWithoutInitializing(sendBufferLarge.size()));
    sendAsync.callback = {};
    SC_TEST_EXPECT(sendAsync.start(eventLoop, client, sendBufferLarge.toSpanConst()));

    struct Context
    {
        SocketDescriptor& client;

        size_t bufferSize          = largeBufferSize; // Need this for vs2019
        int    largeCallbackCalled = 0;
        size_t totalNumBytesRead   = 0;
    } context = {client};

    receiveAsync.callback = [this, &context](AsyncSocketReceive::Result& res)
    {
        context.largeCallbackCalled++;
        if (context.totalNumBytesRead < context.bufferSize)
        {
            context.totalNumBytesRead += res.completionData.numBytes;
            if (context.totalNumBytesRead == context.bufferSize)
            {
                SC_TEST_EXPECT(context.client.close()); // Causes EOF
            }
            res.reactivateRequest(true);
        }
        else
        {
            SC_TEST_EXPECT(res.completionData.disconnected);
            SC_TEST_EXPECT(res.completionData.numBytes == 0); // EOF
        }
    };
    SC_TEST_EXPECT(receiveAsync.start(eventLoop, serverSideClient, receiveBufferLarge.toSpan()));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.largeCallbackCalled >= 1);
    SC_TEST_EXPECT(context.totalNumBytesRead == largeBufferSize);
}

void SC::AsyncTest::socketTCPSendMultiple()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketDescriptor client, serverSideClient;
    createTCPSocketPair(eventLoop, client, serverSideClient);

    int             sendCount = 0;
    AsyncSocketSend sendAsync[2];
    AsyncSequence   sendSequence;
    sendAsync[0].callback = [&](AsyncSocketSend::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        sendCount++;
    };
    sendAsync[1].callback = sendAsync[0].callback;

    // Use an AsyncSequence to enforce order of execution
    sendAsync[0].executeOn(sendSequence); // executed first
    sendAsync[1].executeOn(sendSequence); // executed second

    Span<const char> sendData1[] = {{"PING", 4}, {"PONG", 4}};
    SC_TEST_EXPECT(sendAsync[0].start(eventLoop, client, sendData1));
    Span<const char> sendData2[] = {{"PENG", 4}, {"PANG", 4}};
    SC_TEST_EXPECT(sendAsync[1].start(eventLoop, client, sendData2));
    SC_TEST_EXPECT(eventLoop.runOnce());

    AsyncSocketReceive receiveAsync;

    struct Context
    {
        SocketDescriptor& client;
        Buffer            finalString  = {};
        int               receiveCount = 0;
    } context = {client};

    receiveAsync.callback = [this, &context](AsyncSocketReceive::Result& res)
    {
        Span<char> readData;
        SC_TEST_EXPECT(res.get(readData));
        Span<const char> constData = readData;
        SC_TEST_EXPECT(context.finalString.append(constData));
        SC_TEST_EXPECT(readData.sizeInBytes() == 8);
        if (context.finalString.size() < 16)
        {
            res.reactivateRequest(true);
        }
        else
        {
            SC_TEST_EXPECT(context.client.close()); // Causes EOF
        }
    };
    char       receiveBuffer[8] = {0};
    Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};
    SC_TEST_EXPECT(receiveAsync.start(eventLoop, serverSideClient, receiveData));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(sendCount == 2);
    StringView finalString(context.finalString.toSpanConst(), false, StringEncoding::Ascii);
    SC_TEST_EXPECT(finalString == "PINGPONGPENGPANG");
}

void SC::AsyncTest::socketTCPClose()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketDescriptor client, serverSideClient;
    createTCPSocketPair(eventLoop, client, serverSideClient);

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

void SC::AsyncTest::socketTCPSendReceiveError()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketDescriptor client, serverSideClient;
    createTCPSocketPair(eventLoop, client, serverSideClient);

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
    Span<const char> toSend = {sendBuffer, sizeof(sendBuffer)};
    SC_TEST_EXPECT(asyncSend.start(eventLoop, serverSideClient, toSend));

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

    SC_TEST_EXPECT(not asyncSend.stop(eventLoop));
    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(numOnSend == 1);
    SC_TEST_EXPECT(numOnReceive == 1);
}
