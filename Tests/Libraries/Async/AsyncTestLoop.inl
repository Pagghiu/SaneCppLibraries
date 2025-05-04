// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncTest.h"

void SC::AsyncTest::loopFreeSubmittingOnClose()
{
    // This test checks that on close asyncs being submitted are being removed for submission queue and set as Free.
    AsyncLoopTimeout  loopTimeout[2];
    AsyncLoopWakeUp   loopWakeUp[2];
    AsyncSocketAccept socketAccept[2];

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    SC_TEST_EXPECT(loopTimeout[0].start(eventLoop, 12_ms));
    SC_TEST_EXPECT(loopTimeout[1].start(eventLoop, 122_ms));
    SC_TEST_EXPECT(loopWakeUp[0].start(eventLoop));
    SC_TEST_EXPECT(loopWakeUp[1].start(eventLoop));
    constexpr uint32_t numWaitingConnections = 2;
    SocketDescriptor   serverSocket[2];
    SocketIPAddress    serverAddress[2];
    SC_TEST_EXPECT(serverAddress[0].fromAddressPort("127.0.0.1", 5052));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[0].getAddressFamily(), serverSocket[0]));
    {
        SocketServer server(serverSocket[0]);
        SC_TEST_EXPECT(server.bind(serverAddress[0]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    SC_TEST_EXPECT(serverAddress[1].fromAddressPort("127.0.0.1", 5053));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[1].getAddressFamily(), serverSocket[1]));
    {
        SocketServer server(serverSocket[1]);
        SC_TEST_EXPECT(server.bind(serverAddress[1]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    SC_TEST_EXPECT(socketAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(socketAccept[1].start(eventLoop, serverSocket[1]));

    // All the above requests are in submitting state, but we just abruptly close the loop
    SC_TEST_EXPECT(eventLoop.close());

    // So let's try using them again, and we should get no errors of anything "in use"
    SC_TEST_EXPECT(eventLoop.create());
    SC_TEST_EXPECT(loopTimeout[0].start(eventLoop, 12_ms));
    SC_TEST_EXPECT(loopTimeout[1].start(eventLoop, 123_ms));
    SC_TEST_EXPECT(loopWakeUp[0].start(eventLoop));
    SC_TEST_EXPECT(loopWakeUp[1].start(eventLoop));
    SC_TEST_EXPECT(socketAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(socketAccept[1].start(eventLoop, serverSocket[1]));
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::loopFreeActiveOnClose()
{
    // This test checks that on close active asyncs are being removed for submission queue and set as Free.
    AsyncEventLoop   eventLoop;
    SocketDescriptor acceptedClient[3];

    SC_TEST_EXPECT(eventLoop.create(options));

    constexpr uint32_t numWaitingConnections = 2;
    SocketDescriptor   serverSocket[2];
    SocketIPAddress    serverAddress[2];
    SC_TEST_EXPECT(serverAddress[0].fromAddressPort("127.0.0.1", 5052));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[0].getAddressFamily(), serverSocket[0]));
    {
        SocketServer server(serverSocket[0]);
        SC_TEST_EXPECT(server.bind(serverAddress[0]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    SC_TEST_EXPECT(serverAddress[1].fromAddressPort("127.0.0.1", 5053));
    SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(serverAddress[1].getAddressFamily(), serverSocket[1]));

    {
        SocketServer server(serverSocket[1]);
        SC_TEST_EXPECT(server.bind(serverAddress[1]));
        SC_TEST_EXPECT(server.listen(numWaitingConnections));
    }

    AsyncSocketAccept asyncAccept[2];
    SC_TEST_EXPECT(asyncAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(asyncAccept[1].start(eventLoop, serverSocket[1]));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    // After runNoWait now the two AsyncSocketAccept are active
    SC_TEST_EXPECT(eventLoop.close()); // but closing should make them available again

    // So let's try using them again, and we should get no errors
    SC_TEST_EXPECT(eventLoop.create(options));
    SC_TEST_EXPECT(asyncAccept[0].start(eventLoop, serverSocket[0]));
    SC_TEST_EXPECT(asyncAccept[1].start(eventLoop, serverSocket[1]));
    SC_TEST_EXPECT(eventLoop.runNoWait());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::AsyncTest::loopInterrupt()
{
    AsyncEventLoop   eventLoop;
    AsyncLoopTimeout timeout;

    SC_TEST_EXPECT(eventLoop.create());
    int numTimeouts  = 0;
    timeout.callback = [&](AsyncLoopTimeout::Result& res)
    {
        res.reactivateRequest(true);
        numTimeouts++;
        if (numTimeouts == 2)
        {
            eventLoop.interrupt();
        }
    };
    SC_TEST_EXPECT(timeout.start(eventLoop, 1_ms));
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(numTimeouts == 2);
    SC_TEST_EXPECT(eventLoop.getNumberOfActiveRequests() + eventLoop.getNumberOfSubmittedRequests() == 1);
}
