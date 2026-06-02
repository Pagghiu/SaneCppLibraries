// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// An application-shaped C++20 coroutine example using Await task groups, socket I/O, cancellation, and allocator stats.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build run AwaitServiceProbe` from repo root.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Socket/Socket.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

namespace SC
{
static Result createProbeServer(AsyncEventLoop& eventLoop, SocketDescriptor& serverSocket, SocketIPAddress& address)
{
    constexpr uint16_t firstPort = 39155;
    constexpr uint16_t numPorts  = 64;

    Result lastError = Result::Error("Could not bind AwaitServiceProbe socket");
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

static AwaitTask probeServer(AwaitEventLoop& await, const SocketDescriptor& serverSocket, SocketDescriptor& accepted)
{
    char                     requestBuffer[32] = {};
    AwaitSocketReceiveResult request;
    const char               response[] = "PONG";

    SC_CO_TRY(co_await await.accept(serverSocket, accepted));
    SC_CO_TRY(co_await await.receive(accepted, {requestBuffer, sizeof(requestBuffer)}, request));
    SC_CO_TRY(co_await await.sendAll(accepted, {response, sizeof(response) - 1}));

    co_return Result(true);
}

static AwaitTask probeClient(AwaitEventLoop& await, const SocketDescriptor& client, SocketIPAddress address,
                             Span<char> replyBuffer, AwaitSocketReceiveResult& reply)
{
    const char request[] = "PING";

    SC_CO_TRY(co_await await.connect(client, address));
    SC_CO_TRY(co_await await.sendAll(client, {request, sizeof(request) - 1}));
    SC_CO_TRY(co_await await.receive(client, replyBuffer, reply));

    StringView text({reply.data.data(), reply.data.sizeInBytes()}, false, StringEncoding::Ascii);
    if (text != "PONG")
    {
        co_return Result::Error("AwaitServiceProbe received an unexpected response");
    }

    co_return Result(true);
}

static AwaitTask slowMaintenance(AwaitEventLoop& await)
{
    SC_CO_TRY(co_await await.sleep({1000}));
    co_return Result(true);
}

static AwaitTask networkExchange(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                                 const SocketDescriptor& client, SocketIPAddress address, SocketDescriptor& accepted,
                                 Span<char> replyBuffer, AwaitSocketReceiveResult& reply)
{
    AwaitTask server     = probeServer(await, serverSocket, accepted);
    AwaitTask clientTask = probeClient(await, client, address, replyBuffer, reply);

    AwaitTask*     children[2] = {&server, &clientTask};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawnAll(children));
    SC_CO_TRY(co_await group.waitAll());

    co_return Result(true);
}

static AwaitTask boundedMaintenance(AwaitEventLoop& await, AwaitTimeoutResult& maintenanceTimeout)
{
    AwaitTask maintenance = slowMaintenance(await);
    SC_CO_TRY(await.spawn(maintenance));

    Result waitResult = co_await await.waitFor(maintenance, {1}, &maintenanceTimeout);
    if (waitResult or not maintenanceTimeout.timedOut or not AwaitIsCancelled(maintenance.result()))
    {
        co_return Result::Error("AwaitServiceProbe expected maintenance timeout cancellation");
    }

    co_return Result(true);
}

static AwaitTask serviceProbe(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                              const SocketDescriptor& client, SocketIPAddress address, SocketDescriptor& accepted,
                              Span<char> replyBuffer, AwaitSocketReceiveResult& reply,
                              AwaitTimeoutResult& maintenanceTimeout)
{
    AwaitTask networkTask     = networkExchange(await, serverSocket, client, address, accepted, replyBuffer, reply);
    AwaitTask maintenanceTask = boundedMaintenance(await, maintenanceTimeout);

    AwaitTask*     children[2] = {&networkTask, &maintenanceTask};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawnAll(children));
    SC_CO_TRY(co_await group.waitAll());

    co_return Result(true);
}

static Result runAwaitServiceProbe()
{
    Console console;
    Console::tryAttachingToParentConsole();
    SocketNetworking::initNetworking();

    AsyncEventLoop async;
    SC_TRY(async.create());

    char           allocatorStorage[20 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

    SocketDescriptor serverSocket;
    SocketIPAddress  address;
    SC_TRY(createProbeServer(async, serverSocket, address));

    SocketDescriptor client;
    SC_TRY(async.createAsyncTCPSocket(address.getAddressFamily(), client));

    SocketDescriptor         accepted;
    char                     replyBuffer[32] = {};
    AwaitSocketReceiveResult reply;
    AwaitTimeoutResult       maintenanceTimeout;

    AwaitTask task =
        serviceProbe(await, serverSocket, client, address, accepted, replyBuffer, reply, maintenanceTimeout);
    SC_TRY(await.spawn(task));

    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitServiceProbe event loop stopped before task completed");
    }
    SC_TRY(task.result());

    StringView                     text({reply.data.data(), reply.data.sizeInBytes()}, false, StringEncoding::Ascii);
    const AwaitAllocatorStatistics stats = allocator.statistics();
    console.print("AwaitServiceProbe response: {}\n", text);
    console.print("AwaitServiceProbe maintenance timed out: {}\n", maintenanceTimeout.timedOut ? 1 : 0);
    console.print("AwaitServiceProbe allocator peak/largest/capacity: {}/{}/{} bytes\n", stats.peakBytesInUse,
                  stats.largestRequestedAllocationSize, allocator.capacity());

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
    SC::Result result = SC::runAwaitServiceProbe();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitServiceProbe failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
