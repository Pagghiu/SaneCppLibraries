// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
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
        using namespace SC;
        // TODO: Add EventLoop::resetTimeout
        if (test_section("timeout"))
        {
            AsyncTimeout timeout1, timeout2;
            EventLoop    eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            int  timeout1Called   = 0;
            int  timeout2Called   = 0;
            auto timeout1Callback = [&](AsyncResult& res)
            {
                // As we are in a timeout callback we know for fact that we could easily access
                // what we need with res.async.operation.fields.timeout, playing at the edge of UB.
                // unionAs<...>() however is more safe, as it will return nullptr when casted to wrong
                // type.
                Async::Timeout* timeout = res.async.operation.unionAs<Async::Timeout>();
                SC_TEST_EXPECT(timeout and timeout->timeout.ms == 1);
                timeout1Called++;
            };
            SC_TEST_EXPECT(eventLoop.startTimeout(timeout1, 1_ms, move(timeout1Callback)));
            auto timeout2Callback = [&](AsyncResult&)
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
        if (test_section("wakeUp"))
        {
            int       wakeUp1Called   = 0;
            int       wakeUp2Called   = 0;
            uint64_t  wakeUp1ThreadID = 0;
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());
            AsyncWakeUp wakeUp1;
            AsyncWakeUp wakeUp2;

            auto lambda1 = [&](AsyncResult&)
            {
                wakeUp1ThreadID = Thread::CurrentThreadID();
                wakeUp1Called++;
            };
            SC_TEST_EXPECT(eventLoop.startWakeUp(wakeUp1, lambda1));
            SC_TEST_EXPECT(eventLoop.startWakeUp(wakeUp2, [&](AsyncResult&) { wakeUp2Called++; }));
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

            auto eventLoopLambda = [&](AsyncResult&)
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
        if (test_section("accept"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            constexpr uint32_t numWaitingConnections = 2;
            SocketDescriptor   server;
            uint16_t           tcpPort = 0;
            SC_TEST_EXPECT(listenToAvailablePort(server, "127.0.0.1", numWaitingConnections, 5050, 5060, tcpPort));

            int              acceptedCount = 0;
            SocketDescriptor acceptedClient[3];

            auto onAccepted = [&](AsyncResult& res)
            {
                SC_TEST_EXPECT(acceptedClient[acceptedCount].assign(move(res.result.fields.accept.acceptedClient)));
                acceptedCount++;
            };
            AsyncAccept::Support support;
            AsyncAccept          accept;
            SC_TEST_EXPECT(eventLoop.startAccept(accept, support, server, onAccepted));

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
            SC_TEST_EXPECT(server.close());
            SC_TEST_EXPECT(eventLoop.close());
        }
        if (test_section("connect"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor server;
            uint16_t         tcpPort;
            StringView       connectAddress = "::1";
            SC_TEST_EXPECT(listenToAvailablePort(server, connectAddress, 0, 5050, 5060, tcpPort));

            int              acceptedCount = 0;
            SocketDescriptor acceptedClient[3];

            auto onAccepted = [&](AsyncResult& res)
            {
                SC_TEST_EXPECT(acceptedClient[acceptedCount].assign(move(res.result.fields.accept.acceptedClient)));
                acceptedCount++;
            };
            AsyncAccept::Support acceptSupport;
            AsyncAccept          accept;
            SC_TEST_EXPECT(eventLoop.startAccept(accept, acceptSupport, server, onAccepted));

            int  connectedCount = 0;
            auto onConnected    = [&](AsyncResult& res)
            {
                connectedCount++;
                if (connectedCount == 2)
                {
                    SC_TEST_EXPECT(res.eventLoop.stopAsync(res.async));
                }
            };
            SocketIPAddress localHost;

            SC_TEST_EXPECT(localHost.fromAddressPort(connectAddress, tcpPort));

            AsyncConnect::Support connectSupport[2];
            AsyncConnect          connect[2];
            SocketDescriptor      clients[2];

            SC_TEST_EXPECT(clients[0].createAsyncTCPSocketIPV6());
            SC_TEST_EXPECT(eventLoop.startConnect(connect[0], connectSupport[0], clients[0], localHost, onConnected));

            SC_TEST_EXPECT(clients[1].createAsyncTCPSocketIPV6());
            SC_TEST_EXPECT(eventLoop.startConnect(connect[1], connectSupport[1], clients[1], localHost, onConnected));

            SC_TEST_EXPECT(connectedCount == 0);
            SC_TEST_EXPECT(acceptedCount == 0);
            SC_TEST_EXPECT(eventLoop.run());
            SC_TEST_EXPECT(acceptedCount == 2);
            SC_TEST_EXPECT(connectedCount == 2);
        }
        if (test_section("send/receive"))
        {
            EventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor server;
            uint16_t         tcpPort;
            StringView       connectAddress = "::1";
            SC_TEST_EXPECT(listenToAvailablePort(server, connectAddress, 0, 5050, 5060, tcpPort));

            SocketDescriptor client;
            SC_TEST_EXPECT(SocketClient(client).connect(connectAddress, tcpPort));
            SocketDescriptor serverSideClient;
            SC_TEST_EXPECT(SocketServer(server).accept(SocketFlags::AddressFamilyIPV6, serverSideClient));
            SC_TEST_EXPECT(client.setBlocking(false));
            SC_TEST_EXPECT(serverSideClient.setBlocking(false));

            AsyncSend::Support sendSupport;
            AsyncSend          sendAsync;

            const char sendBuffer[1] = {123};

            Span<const char> sendData = {sendBuffer, sizeof(sendBuffer)};

            int  sendCount = 0;
            auto onSend    = [&](AsyncResult& res)
            {
                SC_UNUSED(res);
                sendCount++;
            };

            SC_TEST_EXPECT(eventLoop.startSend(sendAsync, sendSupport, client, sendData, onSend));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(sendCount == 1);
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(sendCount == 1);

            int  receiveCount = 0;
            auto onReceive    = [&](AsyncResult& res)
            {
                SC_UNUSED(res);
                receiveCount++;
            };

            char       receiveBuffer[1] = {0};
            Span<char> receiveData      = {receiveBuffer, sizeof(receiveBuffer)};

            AsyncReceive::Support receiveSupport;
            AsyncReceive          receiveAsync;
            SC_TEST_EXPECT(
                eventLoop.startReceive(receiveAsync, receiveSupport, serverSideClient, receiveData, onReceive));
            SC_TEST_EXPECT(eventLoop.runOnce());
            SC_TEST_EXPECT(sendCount == 1);
            SC_TEST_EXPECT(eventLoop.runNoWait());
            SC_TEST_EXPECT(sendCount == 1);
            SC_TEST_EXPECT(receiveBuffer[0] == sendBuffer[0]);
        }
    }

    [[nodiscard]] ReturnCode listenToAvailablePort(SocketDescriptor& server, StringView address,
                                                   uint32_t numWaitingConnections, const uint16_t startTcpPort,
                                                   const uint16_t endTcpPort, uint16_t& tcpPort)
    {
        ReturnCode bound = false;
        for (tcpPort = startTcpPort; tcpPort < endTcpPort; ++tcpPort)
        {
            bound = SocketServer(server).listen(address, tcpPort, numWaitingConnections);
            if (bound)
            {
                return true;
            }
        }
        return bound;
    }
};
