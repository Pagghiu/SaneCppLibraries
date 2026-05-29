// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// A tiny C++20 coroutine line protocol using receiveLine(), sendAll(), task groups, and caller-owned allocator storage.
//---------------------------------------------------------------------------------------------------------------------
// Instructions:
// Run `./SC.sh build configure` from repo root, then build/run the `AwaitLineProtocol` console executable.
//---------------------------------------------------------------------------------------------------------------------
#include "../../Libraries/Await/Await.h"
#include "../../Libraries/Socket/Socket.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Strings/StringView.h"

namespace SC
{
static bool equalsAscii(Span<char> data, Span<const char> expected)
{
    return StringView({data.data(), data.sizeInBytes()}, false, StringEncoding::Ascii) ==
           StringView({expected.data(), expected.sizeInBytes()}, false, StringEncoding::Ascii);
}

static Result createLineProtocolServer(AsyncEventLoop& eventLoop, SocketDescriptor& serverSocket,
                                       SocketIPAddress& address)
{
    constexpr uint16_t firstPort = 39131;
    constexpr uint16_t numPorts  = 64;

    Result lastError = Result::Error("Could not bind AwaitLineProtocol socket");
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
            return serverSocket.assign(move(candidate));
        }
    }
    return lastError;
}

static AwaitTask lineServer(AwaitEventLoop& await, const SocketDescriptor& serverSocket, SocketDescriptor& accepted)
{
    char                         requestBuffer[64] = {};
    char                         ackBuffer[32]     = {};
    AwaitSocketReceiveLineResult request;
    AwaitSocketReceiveLineResult ack;

    constexpr char expectedRequest[] = "READ temperature";
    constexpr char valueReply[]      = "VALUE temperature=23.5C\r\n";
    constexpr char expectedAck[]     = "ACK temperature";
    constexpr char doneReply[]       = "DONE\r\n";

    SC_CO_TRY(co_await await.accept(serverSocket, accepted));
    SC_CO_TRY(co_await await.receiveLine(accepted, requestBuffer, request));
    if (not request.lineComplete or request.disconnected or
        not equalsAscii(request.line, {expectedRequest, sizeof(expectedRequest) - 1}))
    {
        co_return Result::Error("AwaitLineProtocol server received unexpected request");
    }

    SC_CO_TRY(co_await await.sendAll(accepted, {valueReply, sizeof(valueReply) - 1}));
    SC_CO_TRY(co_await await.receiveLine(accepted, ackBuffer, ack));
    if (not ack.lineComplete or ack.disconnected or not equalsAscii(ack.line, {expectedAck, sizeof(expectedAck) - 1}))
    {
        co_return Result::Error("AwaitLineProtocol server received unexpected ack");
    }

    SC_CO_TRY(co_await await.sendAll(accepted, {doneReply, sizeof(doneReply) - 1}));

    co_return Result(true);
}

static AwaitTask lineClient(AwaitEventLoop& await, const SocketDescriptor& client, SocketIPAddress address,
                            Span<char> valueBuffer, AwaitSocketReceiveLineResult& valueLine, Span<char> doneBuffer,
                            AwaitSocketReceiveLineResult& doneLine)
{
    constexpr char request[]       = "READ temperature\r\n";
    constexpr char expectedValue[] = "VALUE temperature=23.5C";
    constexpr char ack[]           = "ACK temperature\r\n";
    constexpr char expectedDone[]  = "DONE";

    SC_CO_TRY(co_await await.connect(client, address));
    SC_CO_TRY(co_await await.sendAll(client, {request, sizeof(request) - 1}));
    SC_CO_TRY(co_await await.receiveLine(client, valueBuffer, valueLine));
    if (not valueLine.lineComplete or valueLine.disconnected or
        not equalsAscii(valueLine.line, {expectedValue, sizeof(expectedValue) - 1}))
    {
        co_return Result::Error("AwaitLineProtocol client received unexpected value");
    }

    SC_CO_TRY(co_await await.sendAll(client, {ack, sizeof(ack) - 1}));
    SC_CO_TRY(co_await await.receiveLine(client, doneBuffer, doneLine));
    if (not doneLine.lineComplete or doneLine.disconnected or
        not equalsAscii(doneLine.line, {expectedDone, sizeof(expectedDone) - 1}))
    {
        co_return Result::Error("AwaitLineProtocol client received unexpected completion");
    }

    co_return Result(true);
}

static AwaitTask lineProtocolConversation(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                                          const SocketDescriptor& client, SocketIPAddress address,
                                          SocketDescriptor& accepted, Span<char> valueBuffer,
                                          AwaitSocketReceiveLineResult& valueLine, Span<char> doneBuffer,
                                          AwaitSocketReceiveLineResult& doneLine)
{
    AwaitTask server     = lineServer(await, serverSocket, accepted);
    AwaitTask clientTask = lineClient(await, client, address, valueBuffer, valueLine, doneBuffer, doneLine);

    AwaitTask*     children[2] = {};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawn(server));
    SC_CO_TRY(group.spawn(clientTask));
    SC_CO_TRY(co_await group.waitAll());

    co_return Result(true);
}

static Result runAwaitLineProtocol()
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
    SC_TRY(createLineProtocolServer(async, serverSocket, address));

    SocketDescriptor client;
    SC_TRY(async.createAsyncTCPSocket(address.getAddressFamily(), client));

    SocketDescriptor             accepted;
    char                         valueBuffer[64] = {};
    char                         doneBuffer[16]  = {};
    AwaitSocketReceiveLineResult valueLine;
    AwaitSocketReceiveLineResult doneLine;
    AwaitTask task = lineProtocolConversation(await, serverSocket, client, address, accepted, valueBuffer, valueLine,
                                              doneBuffer, doneLine);

    SC_TRY(await.spawn(task));
    Result runResult = await.run();
    if (not runResult)
    {
        return task.isCompleted() ? task.result()
                                  : Result::Error("AwaitLineProtocol event loop stopped before task completed");
    }
    SC_TRY(task.result());

    StringView value({valueLine.line.data(), valueLine.line.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("AwaitLineProtocol read: {}\n", value);
    console.print("AwaitLineProtocol allocator peak/largest/capacity: {}/{}/{} bytes\n", allocator.peakUsed(),
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
    SC::Result result = SC::runAwaitLineProtocol();
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        console.print("AwaitLineProtocol failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
