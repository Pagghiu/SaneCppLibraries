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
        if (test_section("cancel sleep"))
        {
            cancelSleep();
        }
        if (test_section("callback and coroutine coexist"))
        {
            callbackAndCoroutineCoexist();
        }
        if (test_section("socket accept"))
        {
            socketAccept();
        }
        if (test_section("socket send receive"))
        {
            socketSendReceive();
        }
        if (test_section("socket send all"))
        {
            socketSendAll();
        }
        if (test_section("child task"))
        {
            childTask();
        }
        if (test_section("cancel child task"))
        {
            cancelChildTask();
        }
        if (test_section("arena"))
        {
            arena();
        }
    }

    AwaitTask immediate(AwaitEventLoop&) { co_return Result(true); }

    AwaitTask waitTwice(AwaitEventLoop& await)
    {
        SC_CO_TRY(co_await await.sleep(1_ms));
        SC_CO_TRY(co_await await.sleep(1_ms));
        co_return Result(true);
    }

    AwaitTask waitLong(AwaitEventLoop& await)
    {
        SC_CO_TRY(co_await await.sleep(1000_ms));
        co_return Result(true);
    }

    AwaitTask acceptOne(AwaitEventLoop& await, const SocketDescriptor& serverSocket, SocketDescriptor& acceptedClient)
    {
        SC_CO_TRY(co_await await.accept(serverSocket, acceptedClient));
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

    AwaitTask sendAllOnce(AwaitEventLoop& await, const SocketDescriptor& sender, const SocketDescriptor& receiver)
    {
        const char sendBuffer[]      = {1, 2, 3, 4, 5};
        char       receiveBuffer[16] = {0};

        AwaitSocketSendResult sendResult;
        SC_CO_TRY(co_await await.sendAll(sender, {sendBuffer, sizeof(sendBuffer)}, &sendResult));
        SC_TEST_EXPECT(sendResult.numBytes == sizeof(sendBuffer));

        AwaitSocketReceiveResult receiveResult;
        SC_CO_TRY(co_await await.receive(receiver, {receiveBuffer, sizeof(receiveBuffer)}, receiveResult));
        SC_TEST_EXPECT(not receiveResult.disconnected);
        SC_TEST_EXPECT(receiveResult.data.sizeInBytes() == sizeof(sendBuffer));
        for (size_t idx = 0; idx < sizeof(sendBuffer); ++idx)
        {
            SC_TEST_EXPECT(receiveResult.data.data()[idx] == sendBuffer[idx]);
        }

        co_return Result(true);
    }

    AwaitTask awaitChild(AwaitEventLoop& await, AwaitTask& child)
    {
        SC_CO_TRY(await.spawn(child));
        SC_CO_TRY(co_await child);
        co_return Result(true);
    }

    static AwaitTask arenaWait(AwaitEventLoop& await)
    {
        SC_CO_TRY(co_await await.sleep(1_ms));
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

    void createTCPServer(AsyncEventLoop& eventLoop, SocketDescriptor& serverSocket, SocketIPAddress& nativeAddress)
    {
        const uint16_t tcpPort = report.mapPort(5051);

        SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(1));
        }
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

    void cancelSleep()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        AwaitTask task = waitLong(await);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(task.cancel(await));
        SC_TEST_EXPECT(task.isCancellationRequested());

        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
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

    void socketAccept()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        SocketDescriptor serverSocket;
        SocketIPAddress  nativeAddress;
        createTCPServer(async, serverSocket, nativeAddress);

        SocketDescriptor acceptedClient;
        AwaitTask        task = acceptOne(await, serverSocket, acceptedClient);
        SC_TEST_EXPECT(await.spawn(task));

        SocketDescriptor client;
        SC_TEST_EXPECT(client.create(nativeAddress.getAddressFamily()));
        SC_TEST_EXPECT(SocketClient(client).connect("127.0.0.1", nativeAddress.getPort()));

        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(acceptedClient.isValid());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
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

    void socketSendAll()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(async, client, serverSideClient);

        AwaitTask task = sendAllOnce(await, client, serverSideClient);
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(async.close());
    }

    void childTask()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        AwaitTask child  = waitTwice(await);
        AwaitTask parent = awaitChild(await, child);

        SC_TEST_EXPECT(await.spawn(parent));
        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(child.result());
        SC_TEST_EXPECT(parent.result());
        SC_TEST_EXPECT(async.close());
    }

    void cancelChildTask()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());
        AwaitEventLoop await(async);

        AwaitTask child  = waitLong(await);
        AwaitTask parent = awaitChild(await, child);

        SC_TEST_EXPECT(await.spawn(parent));
        SC_TEST_EXPECT(parent.isActive());
        SC_TEST_EXPECT(child.isActive());
        SC_TEST_EXPECT(parent.cancel(await));

        SC_TEST_EXPECT(await.run());

        SC_TEST_EXPECT(child.isCompleted());
        SC_TEST_EXPECT(parent.isCompleted());
        SC_TEST_EXPECT(not child.result());
        SC_TEST_EXPECT(not parent.result());
        SC_TEST_EXPECT(async.close());
    }

    void arena()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());

        char           arenaMemory[16 * 1024] = {0};
        AwaitArena     arenaStorage({arenaMemory, sizeof(arenaMemory)});
        AwaitEventLoop await(async, &arenaStorage);

        AwaitTask task = arenaWait(await);
        SC_TEST_EXPECT(arenaStorage.used() > 0);

        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(async.close());
    }
};

namespace SC
{
void runAwaitTest(SC::TestReport& report) { AwaitTest test(report); }
} // namespace SC
