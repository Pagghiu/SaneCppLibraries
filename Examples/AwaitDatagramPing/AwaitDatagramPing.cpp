// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A tiny UDP request/reply conversation using the draft Await library.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitDatagramPing` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Socket/Socket.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

namespace SC
{
static Result createDatagramServer(AsyncEventLoop& eventLoop, SocketDescriptor& serverSocket,
                                   SocketIPAddress& serverAddress)
{
    constexpr uint16_t firstPort = 39191;
    constexpr uint16_t numPorts  = 64;

    Result lastError = Result::Error("Could not bind AwaitDatagramPing socket");
    for (uint16_t offset = 0; offset < numPorts; ++offset)
    {
        const uint16_t port = static_cast<uint16_t>(firstPort + offset);
        SC_TRY(serverAddress.fromAddressPort("127.0.0.1", port));

        SocketDescriptor candidate;
        SC_TRY(eventLoop.createAsyncUDPSocket(serverAddress.getAddressFamily(), candidate));

        SocketServer server(candidate);
        lastError = server.bind(serverAddress);
        if (lastError)
        {
            return serverSocket.assign(move(candidate));
        }
    }
    return lastError;
}

static AwaitTask datagramServer(AwaitEventLoop& await, const SocketDescriptor& server)
{
    char                         requestBuffer[64] = {};
    AwaitSocketReceiveFromResult request;
    SC_CO_TRY(co_await await.receiveFrom(server, {requestBuffer, sizeof(requestBuffer)}, request));

    StringView requestText({request.data.data(), request.data.sizeInBytes()}, false, StringEncoding::Ascii);
    if (requestText != "ping await udp")
    {
        co_return Result::Error("AwaitDatagramPing server received an unexpected request");
    }

    const char            reply[] = "pong await udp";
    AwaitSocketSendResult sendResult;
    SC_CO_TRY(co_await await.sendTo(server, request.sourceAddress, {reply, sizeof(reply) - 1}, &sendResult));
    if (sendResult.numBytes != sizeof(reply) - 1)
    {
        co_return Result::Error("AwaitDatagramPing server sent a partial reply");
    }

    co_return Result(true);
}

static AwaitTask datagramClient(AwaitEventLoop& await, const SocketDescriptor& client, SocketIPAddress serverAddress,
                                Span<char> replyBuffer, AwaitSocketReceiveFromResult& reply)
{
    const char            request[] = "ping await udp";
    AwaitSocketSendResult sendResult;
    SC_CO_TRY(co_await await.sendTo(client, serverAddress, {request, sizeof(request) - 1}, &sendResult));
    if (sendResult.numBytes != sizeof(request) - 1)
    {
        co_return Result::Error("AwaitDatagramPing client sent a partial request");
    }

    SC_CO_TRY(co_await await.receiveFrom(client, replyBuffer, reply));
    StringView replyText({reply.data.data(), reply.data.sizeInBytes()}, false, StringEncoding::Ascii);
    if (replyText != "pong await udp")
    {
        co_return Result::Error("AwaitDatagramPing client received an unexpected reply");
    }

    co_return Result(true);
}

static AwaitTask datagramConversation(AwaitEventLoop& await, const SocketDescriptor& server,
                                      const SocketDescriptor& client, SocketIPAddress serverAddress,
                                      Span<char> replyBuffer, AwaitSocketReceiveFromResult& reply)
{
    AwaitTask serverTask = datagramServer(await, server);
    AwaitTask clientTask = datagramClient(await, client, serverAddress, replyBuffer, reply);

    AwaitTask*     children[2] = {};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawn(serverTask));
    SC_CO_TRY(group.spawn(clientTask));
    SC_CO_TRY(co_await group.waitAll());

    co_return Result(true);
}

static Result runAwaitDatagramPing()
{
    Console console;
    Console::tryAttachingToParentConsole();
    SocketNetworking::initNetworking();

    AsyncEventLoop async;
    SC_TRY(async.create());

    char           arenaMemory[16 * 1024] = {};
    AwaitArena     arena({arenaMemory, sizeof(arenaMemory)});
    AwaitEventLoop await(async, &arena);

    SocketDescriptor server;
    SocketIPAddress  serverAddress;
    SC_TRY(createDatagramServer(async, server, serverAddress));

    SocketDescriptor client;
    SC_TRY(async.createAsyncUDPSocket(serverAddress.getAddressFamily(), client));

    char                         replyBuffer[64] = {};
    AwaitSocketReceiveFromResult reply;
    AwaitTask                    task =
        datagramConversation(await, server, client, serverAddress, {replyBuffer, sizeof(replyBuffer)}, reply);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitDatagramPing event loop stopped before task completed");
    }
    SC_TRY(task.result());

    StringView replyText({reply.data.data(), reply.data.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("Await UDP reply: {}\n", replyText);
    console.print("Await arena capacity: {} bytes\n", arena.capacity());

    SC_TRY(client.close());
    SC_TRY(server.close());
    SC_TRY(async.close());
    SocketNetworking::shutdownNetworking();
    return Result(true);
}
} // namespace SC

int main()
{
    SC::Result result = SC::runAwaitDatagramPing();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitDatagramPing failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
