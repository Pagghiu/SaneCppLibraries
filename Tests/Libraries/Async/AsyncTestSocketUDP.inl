// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/StringBuilder.h"

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

void SC::AsyncTest::socketUnixDatagramSendReceive()
{
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_EMSCRIPTEN
    StringPath receiverPath;
    StringPath senderPath;
    SC_TEST_EXPECT(StringBuilder::format(receiverPath, "/tmp/sc-async-dgram-r-{0}.sock", report.mapPort(5064)));
    SC_TEST_EXPECT(StringBuilder::format(senderPath, "/tmp/sc-async-dgram-s-{0}.sock", report.mapPort(5065)));

    FileSystem fileSystem;
    if (fileSystem.exists(receiverPath.view()))
    {
        SC_TEST_EXPECT(fileSystem.removeFile(receiverPath.view()));
    }
    if (fileSystem.exists(senderPath.view()))
    {
        SC_TEST_EXPECT(fileSystem.removeFile(senderPath.view()));
    }

    SocketAddress receiverAddress;
    SocketAddress senderAddress;
    SC_TEST_EXPECT(receiverAddress.fromUnixPath(receiverPath.view()));
    SC_TEST_EXPECT(senderAddress.fromUnixPath(senderPath.view()));

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create(options));
    SocketDescriptor receiverSocket;
    SocketDescriptor senderSocket;
    SC_TEST_EXPECT(eventLoop.createAsyncSocket(SocketFlags::AddressFamilyUnix, SocketFlags::SocketDgram,
                                               SocketFlags::ProtocolDefault, receiverSocket));
    SC_TEST_EXPECT(eventLoop.createAsyncSocket(SocketFlags::AddressFamilyUnix, SocketFlags::SocketDgram,
                                               SocketFlags::ProtocolDefault, senderSocket));
    SC_TEST_EXPECT(SocketServer(receiverSocket).bind(receiverAddress));
    SC_TEST_EXPECT(SocketServer(senderSocket).bind(senderAddress));

    struct Context
    {
        StringSpan senderPath;
        int        sendCount        = 0;
        int        receiveCount     = 0;
        char       receiveBuffer[8] = {0};
    } context;
    context.senderPath  = senderPath.view();
    Context* contextPtr = &context;

    const char        payload = 91;
    AsyncSocketSendTo send;
    send.callback = [this, contextPtr](AsyncSocketSendTo::Result& result)
    {
        SC_TEST_EXPECT(result.isValid());
        contextPtr->sendCount++;
    };
    SC_TEST_EXPECT(send.start(eventLoop, senderSocket, receiverAddress, {&payload, 0}));

    AsyncSocketReceiveFrom receive;
    receive.callback = [this, contextPtr](AsyncSocketReceiveFrom::Result& result)
    {
        Span<char> data;
        SC_TEST_EXPECT(result.get(data));
        SC_TEST_EXPECT(data.empty());
        SC_TEST_EXPECT(not result.isEnded());

        SocketAddress                sourceAddress = result.getSourceSocketAddress();
        Span<const char>             sourceName;
        SocketAddress::UnixNamespace sourceNamespace = SocketAddress::UnixNamespace::Unnamed;
        SC_TEST_EXPECT(sourceAddress.getUnixName(sourceName, sourceNamespace));
        SC_TEST_EXPECT(sourceNamespace == SocketAddress::UnixNamespace::Pathname);
        SC_TEST_EXPECT(StringView(sourceName, false, StringEncoding::Native) == contextPtr->senderPath);
        contextPtr->receiveCount++;
    };
    SC_TEST_EXPECT(receive.start(eventLoop, receiverSocket, context.receiveBuffer));

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(context.sendCount == 1);
    SC_TEST_EXPECT(context.receiveCount == 1);

    SC_TEST_EXPECT(senderSocket.close());
    SC_TEST_EXPECT(receiverSocket.close());
    SC_TEST_EXPECT(eventLoop.close());
    SC_TEST_EXPECT(fileSystem.removeFile(senderPath.view()));
    SC_TEST_EXPECT(fileSystem.removeFile(receiverPath.view()));
#endif
}
