// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Libraries/Await/Await.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Time/Time.h"

namespace SC
{
struct AwaitTest;
}

struct SC::AwaitTest : public SC::TestCase
{
    AwaitTest(SC::TestReport& report) : TestCase(report, "AwaitTest")
    {
        if (test_section("immediate task"))
        {
            immediateTask();
        }
        if (test_section("move task"))
        {
            moveTask();
        }
        if (test_section("sleep twice"))
        {
            sleepTwice();
        }
        if (test_section("callback and coroutine coexist"))
        {
            callbackAndCoroutineCoexist();
        }
        if (test_section("socket send receive"))
        {
            socketSendReceive();
        }
    }

    AwaitTask immediate(AwaitEventLoop&) { co_return Result(true); }

    AwaitTask waitTwice(AwaitEventLoop& await)
    {
        SC_CO_TRY(co_await await.sleep(1_ms));
        SC_CO_TRY(co_await await.sleep(1_ms));
        co_return Result(true);
    }

    AwaitTask sendReceiveOnce(AwaitEventLoop& await, const SocketDescriptor& sender, const SocketDescriptor& receiver)
    {
        const char sendBuffer[]      = {1, 2, 3};
        char       receiveBuffer[16] = {0};

        AwaitSocketSendResult sendResult;
        SC_CO_TRY(co_await await.send(sender, {sendBuffer, sizeof(sendBuffer)}, &sendResult));
        SC_TEST_EXPECT(sendResult.numBytes == sizeof(sendBuffer));

        AwaitSocketReceiveResult receiveResult;
        SC_CO_TRY(co_await await.receive(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        SC_TEST_EXPECT(not receiveResult.disconnected);
        SC_TEST_EXPECT(receiveResult.data.sizeInBytes() == sizeof(sendBuffer));
        SC_TEST_EXPECT(receiveResult.data.data()[0] == sendBuffer[0]);
        SC_TEST_EXPECT(receiveResult.data.data()[1] == sendBuffer[1]);
        SC_TEST_EXPECT(receiveResult.data.data()[2] == sendBuffer[2]);

        co_return Result(true);
    }

    void createTCPSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& client, SocketDescriptor& serverSideClient)
    {
        SocketDescriptor serverSocket;
        const uint16_t   tcpPort        = report.mapPort(5050);
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

    void immediateTask()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        AwaitTask task = immediate(await);
        SC_TEST_EXPECT(task.isValid());
        SC_TEST_EXPECT(not task.isStarted());
        SC_TEST_EXPECT(not task.isCompleted());

        SC_TEST_EXPECT(await.spawn(task));

        SC_TEST_EXPECT(task.isStarted());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.isActive());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());
    }

    void moveTask()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        AwaitTask task  = waitTwice(await);
        AwaitTask moved = move(task);

        SC_TEST_EXPECT(not task.isValid());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(moved.isValid());

        SC_TEST_EXPECT(await.spawn(moved));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(moved.result());
        SC_TEST_EXPECT(async.close());
    }

    void sleepTwice()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        AwaitTask task = waitTwice(await);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(task.isStarted());
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(not task.isCompleted());

        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.isActive());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());
    }

    void callbackAndCoroutineCoexist()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        int              callbackCount = 0;
        AsyncLoopTimeout callbackTimeout;
        callbackTimeout.callback = [&](AsyncLoopTimeout::Result& result)
        {
            SC_TEST_EXPECT(result.isValid());
            callbackCount++;
        };

        AwaitTask task = waitTwice(await);
        SC_TEST_EXPECT(callbackTimeout.start(async, 1_ms));
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(callbackCount == 1);
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());
    }

    void socketSendReceive()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(async, client, serverSideClient);

        AwaitTask task = sendReceiveOnce(await, client, serverSideClient);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(async.close());
    }
};

namespace SC
{
void runAwaitTest(SC::TestReport& report) { AwaitTest test(report); }
} // namespace SC
