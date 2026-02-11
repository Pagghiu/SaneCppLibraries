// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/Socket/Socket.h"

void SC::AsyncTest::socketUDPSendReceive()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketIPAddress serverAddress;
    const uint16_t  port = report.mapPort(5051);
    SC_TEST_EXPECT(serverAddress.fromAddressPort("0.0.0.0", port)); // Bind to all interfaces on port
    SocketIPAddress clientAddress;
    SC_TEST_EXPECT(clientAddress.fromAddressPort("127.0.0.1", port)); // Connect to localhost on port

    SocketDescriptor serverSocket, clientSocket;
    SC_TEST_EXPECT(eventLoop.createAsyncUDPSocket(serverAddress.getAddressFamily(), serverSocket));
    SC_TEST_EXPECT(eventLoop.createAsyncUDPSocket(clientAddress.getAddressFamily(), clientSocket));
    SC_TEST_EXPECT(SocketServer(serverSocket).bind(serverAddress));

    struct Context
    {
        int  sendCount     = 0;
        int  recvCount     = 0;
        char recvBuffer[8] = {0};
    } context;

    // Prepare data to send
    const char       sendData[] = "PING";
    Span<const char> sendSpan   = {sendData, 4};

    // Async UDP send
    AsyncSocketSendTo asyncSendTo;
    asyncSendTo.callback = [this, &context](AsyncSocketSendTo::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        context.sendCount++;
    };
    SC_TEST_EXPECT(asyncSendTo.start(eventLoop, clientSocket, clientAddress, sendSpan));

    // Async UDP receive
    Span<char>             recvSpan = {context.recvBuffer, sizeof(context.recvBuffer)};
    AsyncSocketReceiveFrom asyncReceiveFrom;
    asyncReceiveFrom.callback = [this, &context](AsyncSocketReceiveFrom::Result& res)
    {
        SC_TEST_EXPECT(res.isValid());
        Span<char> data;
        SC_TEST_EXPECT(res.get(data));
        SC_TEST_EXPECT(data.sizeInBytes() == 4);
        SC_TEST_EXPECT(memcmp(data.data(), "PING", 4) == 0);
        SocketIPAddress sourceAddress = res.getSourceAddress();
        SC_TEST_EXPECT(sourceAddress.isValid());
        context.recvCount++;
        res.reactivateRequest(false); // Only receive once for this test
    };
    SC_TEST_EXPECT(asyncReceiveFrom.start(eventLoop, serverSocket, recvSpan));

    SC_TEST_EXPECT(eventLoop.run());

    SC_TEST_EXPECT(context.sendCount == 1);
    SC_TEST_EXPECT(context.recvCount == 1);
    SC_TEST_EXPECT(serverSocket.close());
    SC_TEST_EXPECT(clientSocket.close());
    SC_TEST_EXPECT(eventLoop.close());
}
