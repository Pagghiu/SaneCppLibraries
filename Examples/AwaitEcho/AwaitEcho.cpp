// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A tiny C++20 coroutine echo conversation using the draft Await library.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitEcho` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Socket/Socket.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

namespace SC
{
static Result createServer(AsyncEventLoop& eventLoop, SocketDescriptor& serverSocket, SocketIPAddress& address)
{
    constexpr uint16_t firstPort = 39091;
    constexpr uint16_t numPorts  = 64;

    Result lastError = Result::Error("Could not bind AwaitEcho socket");
    for (uint16_t offset = 0; offset < numPorts; ++offset)
    {
        const uint16_t port = static_cast<uint16_t>(firstPort + offset);
        SC_TRY(address.fromAddressPort("127.0.0.1", port));

        SocketDescriptor candidate;
        SC_TRY(eventLoop.createAsyncTCPSocket(address.getAddressFamily(), candidate));

        SocketServer server(candidate);
        lastError = server.bind(address);
        if (lastError)
        {
            SC_TRY(server.listen(1));
            return Result::Explicit(serverSocket.assign(move(candidate)));
        }
    }
    return lastError;
}

static AwaitTask echoServer(AwaitEventLoop& await, const SocketDescriptor& serverSocket, SocketDescriptor& accepted)
{
    char                     buffer[64] = {};
    AwaitSocketReceiveResult received;

    Result acceptResult = co_await await.accept(serverSocket, accepted);
    if (not acceptResult)
    {
        co_return Result::Error("AwaitEcho server accept failed");
    }
    Result receiveResult = co_await await.receive(accepted, {buffer, sizeof(buffer)}, received);
    if (not receiveResult)
    {
        co_return Result::Error("AwaitEcho server receive failed");
    }
    Result sendResult = co_await await.sendAll(accepted, received.data);
    if (not sendResult)
    {
        co_return Result::Error("AwaitEcho server sendAll failed");
    }

    co_return Result(true);
}

static AwaitTask echoClient(AwaitEventLoop& await, const SocketDescriptor& client, SocketIPAddress address,
                            Span<char> replyBuffer, AwaitSocketReceiveResult& reply)
{
    const char message[] = "niche readable await";

    Result connectResult = co_await await.connect(client, address);
    if (not connectResult)
    {
        co_return Result::Error("AwaitEcho client connect failed");
    }
    Result sendResult = co_await await.sendAll(client, {message, sizeof(message) - 1});
    if (not sendResult)
    {
        co_return Result::Error("AwaitEcho client sendAll failed");
    }
    Result receiveResult = co_await await.receive(client, replyBuffer, reply);
    if (not receiveResult)
    {
        co_return Result::Error("AwaitEcho client receive failed");
    }

    co_return Result(true);
}

static AwaitTask echoConversation(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                                  const SocketDescriptor& client, SocketIPAddress address, SocketDescriptor& accepted,
                                  Span<char> replyBuffer, AwaitSocketReceiveResult& reply)
{
    AwaitTask server     = echoServer(await, serverSocket, accepted);
    AwaitTask clientTask = echoClient(await, client, address, replyBuffer, reply);

    AwaitTask*     children[2] = {&server, &clientTask};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawnAll(children));
    Result waitResult = co_await group.waitAll();
    if (not waitResult)
    {
        co_return Result::Error("AwaitEcho task group failed");
    }

    co_return Result(true);
}

static Result runAwaitEcho()
{
    Console console;
    Console::tryAttachingToParentConsole();
    SocketNetworking::initNetworking();

    AsyncEventLoop async;
    SC_TRY(async.create());

    char           allocatorStorage[16 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    SocketDescriptor serverSocket;
    SocketIPAddress  address;
    SC_TRY(createServer(async, serverSocket, address));

    SocketDescriptor client;
    SC_TRY(async.createAsyncTCPSocket(address.getAddressFamily(), client));

    SocketDescriptor         accepted;
    char                     replyBuffer[64] = {};
    AwaitSocketReceiveResult reply;

    AwaitTask task = echoConversation(await, serverSocket, client, address, accepted, replyBuffer, reply);
    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result() : Result::Error("AwaitEcho event loop stopped before task completed");
    }
    SC_TRY(task.result());

    StringView echoed({reply.data.data(), reply.data.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("Await echo replied: {}\n", echoed);
    console.print("Await allocator peak/largest/capacity: {}/{}/{} bytes\n", allocator.peakUsed(),
                  allocator.largestAllocationSize(), allocator.capacity());

    SC_TRY(client.close());
    SC_TRY(accepted.close());
    SC_TRY(serverSocket.close());
    SC_TRY(async.close());
    SocketNetworking::shutdownNetworking();
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitEcho();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitEcho failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
