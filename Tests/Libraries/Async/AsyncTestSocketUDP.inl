// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Time/Time.h"
#include <stdio.h>
#include <stdlib.h>

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

void SC::AsyncTest::socketUDPDatagramTruncation(bool queued)
{
    Atomic<bool> finished = false;
    Atomic<int>  phase    = 0;
    Thread       deadline;
    SC_TEST_EXPECT(deadline.start(
        [&finished, &phase](Thread&)
        {
            const auto started = Time::Monotonic::now();
            while (not finished.load())
            {
                if (Time::Monotonic::now().getMonotonicMilliseconds() - started.getMonotonicMilliseconds() > 8000)
                {
                    fprintf(stderr, "UDP truncation deadline: case %d\n", phase.load());
                    fflush(stderr);
                    abort();
                }
                Thread::Sleep(10);
            }
        }));
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create(options));
    SocketIPAddress serverAddress;
    SocketIPAddress clientAddress;
    SC_TEST_EXPECT(serverAddress.fromAddressPort("127.0.0.1", report.mapPort(5054)));
    SC_TEST_EXPECT(clientAddress.fromAddressPort("127.0.0.1", report.mapPort(5055)));
    SocketDescriptor server;
    SocketDescriptor client;
    SC_TEST_EXPECT(loop.createAsyncUDPSocket(serverAddress.getAddressFamily(), server));
    SC_TEST_EXPECT(loop.createAsyncUDPSocket(clientAddress.getAddressFamily(), client));
    SC_TEST_EXPECT(SocketServer(server).bind(serverAddress));
    SC_TEST_EXPECT(SocketServer(client).bind(clientAddress));
    struct State
    {
        size_t      expectedBytes = 0;
        size_t      expectedSent  = 0;
        bool        truncated     = false;
        const char* prefix        = nullptr;
        uint16_t    sourcePort    = 0;
        int         receives      = 0;
        int         sends         = 0;
    } state;
    state.sourcePort = clientAddress.getPort();
    AsyncSocketSendTo send;
    send.callback = [this, &state](AsyncSocketSendTo::Result& result)
    {
        SC_TEST_EXPECT(result.isValid());
        SC_TEST_EXPECT(result.completionData.numBytes == state.expectedSent);
        ++state.sends;
    };
    AsyncSocketReceiveFrom receive;
    receive.callback = [this, &state](AsyncSocketReceiveFrom::Result& result)
    {
        Span<char> bytes;
        SC_TEST_EXPECT(result.get(bytes));
        SC_TEST_EXPECT(bytes.sizeInBytes() == state.expectedBytes);
        SC_TEST_EXPECT(result.isTruncated() == state.truncated);
        SC_TEST_EXPECT(not result.isEnded());
        SC_TEST_EXPECT(result.getSourceAddress().getPort() == state.sourcePort);
        if (not bytes.empty())
            SC_TEST_EXPECT(memcmp(bytes.data(), state.prefix, state.expectedBytes) == 0);
        ++state.receives;
    };
    char                   buffer[3]  = {};
    const Span<const char> messages[] = {
        StringView("abcdef").toCharSpan(), StringView("xy").toCharSpan(), {}, StringView("abcdef").toCharSpan()};
    for (size_t index = 0; index < 4; ++index)
    {
        phase.store(static_cast<int>(index));
        state.expectedSent    = messages[index].sizeInBytes();
        state.expectedBytes   = index == 0 ? 3 : index == 1 ? 2 : 0;
        state.truncated       = index == 0 or index == 3;
        state.prefix          = messages[index].data();
        const Span<char> loan = {buffer, index == 3 ? size_t(0) : sizeof(buffer)};
        if (queued)
        {
            SC_TEST_EXPECT(send.start(loop, client, serverAddress, messages[index]));
            SC_TEST_EXPECT(loop.run());
            SC_TEST_EXPECT(receive.start(loop, server, loan));
        }
        else
        {
            SC_TEST_EXPECT(receive.start(loop, server, loan));
            SC_TEST_EXPECT(loop.runNoWait());
            SC_TEST_EXPECT(loop.runNoWait());
            SC_TEST_EXPECT(send.start(loop, client, serverAddress, messages[index]));
        }
        SC_TEST_EXPECT(loop.run());
        SC_TEST_EXPECT(state.receives == static_cast<int>(index + 1));
        SC_TEST_EXPECT(state.sends == static_cast<int>(index + 1));
        SC_TEST_EXPECT(receive.isFree() and send.isFree());
    }
    SC_TEST_EXPECT(server.close());
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(loop.close());
    finished.store(true);
    SC_TEST_EXPECT(deadline.join());
}
