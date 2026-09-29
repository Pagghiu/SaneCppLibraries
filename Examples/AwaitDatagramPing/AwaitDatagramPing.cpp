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

#include "../../Libraries/Async/AsyncErrorFormatter.h"
#include "../../Libraries/Await/AwaitErrorFormatter.h"
#include "../../Libraries/Socket/SocketErrorFormatter.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"

namespace SC
{
static constexpr ResultCategory AwaitDatagramPingResultCategory = ResultCategory(0x80000014u);
enum class AwaitDatagramPingError : uint32_t
{
    UnexpectedRequest = 1,
    IncompleteReply,
    IncompleteRequest,
    UnexpectedReply,
};
static constexpr Result AwaitDatagramPingFailure(AwaitDatagramPingError error)
{
    return Result::Error(AwaitDatagramPingResultCategory, error);
}
static ResultErrorFormat formatAwaitDatagramPingError(Result result, Span<char> output)
{
    if (result.category() == AwaitResultCategory)
        return formatAwaitError(result, output);
    if (result.category() == AsyncResultCategory)
        return formatAsyncError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() == SocketResultCategory)
        return formatSocketError(result, output);
    if (result.category() != AwaitDatagramPingResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<AwaitDatagramPingError>(result.errorValue()))
    {
    case AwaitDatagramPingError::UnexpectedRequest: formatter.append("Server received an unexpected request"); break;
    case AwaitDatagramPingError::IncompleteReply: formatter.append("Server sent a partial reply"); break;
    case AwaitDatagramPingError::IncompleteRequest: formatter.append("Client sent a partial request"); break;
    case AwaitDatagramPingError::UnexpectedReply: formatter.append("Client received an unexpected reply"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

static Result createDatagramServer(AsyncEventLoop& eventLoop, SocketDescriptor& serverSocket,
                                   SocketIPAddress& serverAddress)
{
    constexpr uint16_t firstPort = 39191;
    constexpr uint16_t numPorts  = 64;

    Result lastError = Result::Error(SocketResultCategory, SocketError::BindFailed);
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
            return Result::Explicit(serverSocket.assign(move(candidate)));
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
        co_return AwaitDatagramPingFailure(AwaitDatagramPingError::UnexpectedRequest);
    }

    const char            reply[] = "pong await udp";
    AwaitSocketSendResult sendResult;
    SC_CO_TRY(co_await await.sendTo(server, request.sourceAddress, {reply, sizeof(reply) - 1}, &sendResult));
    if (sendResult.numBytes != sizeof(reply) - 1)
    {
        co_return AwaitDatagramPingFailure(AwaitDatagramPingError::IncompleteReply);
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
        co_return AwaitDatagramPingFailure(AwaitDatagramPingError::IncompleteRequest);
    }

    SC_CO_TRY(co_await await.receiveFrom(client, replyBuffer, reply));
    StringView replyText({reply.data.data(), reply.data.sizeInBytes()}, false, StringEncoding::Ascii);
    if (replyText != "pong await udp")
    {
        co_return AwaitDatagramPingFailure(AwaitDatagramPingError::UnexpectedReply);
    }

    co_return Result(true);
}

static AwaitTask datagramConversation(AwaitEventLoop& await, const SocketDescriptor& server,
                                      const SocketDescriptor& client, SocketIPAddress serverAddress,
                                      Span<char> replyBuffer, AwaitSocketReceiveFromResult& reply)
{
    AwaitTask serverTask = datagramServer(await, server);
    AwaitTask clientTask = datagramClient(await, client, serverAddress, replyBuffer, reply);

    AwaitTask*     children[2] = {&serverTask, &clientTask};
    AwaitTaskGroup group(await, children);
    SC_CO_TRY(group.spawnAll(children));
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

    char           allocatorStorage[16 * 1024] = {};
    AwaitAllocator allocator;
    SC_TRY(allocator.createFixed(allocatorStorage));
    AwaitEventLoop await(async, allocator);

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
        return task.isCompleted() ? task.result() : runResult;
    }
    SC_TRY(task.result());

    StringView replyText({reply.data.data(), reply.data.sizeInBytes()}, false, StringEncoding::Ascii);
    console.print("Await UDP reply: {}\n", replyText);
    console.print("Await allocator peak/largest/capacity: {}/{}/{} bytes\n", allocator.peakUsed(),
                  allocator.largestAllocationSize(), allocator.capacity());

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
        char                        message[256] = {};
        const SC::ResultErrorFormat formatted    = SC::formatAwaitDatagramPingError(result, message);
        if (formatted.status == SC::ResultErrorFormatStatus::Success)
            console.print("AwaitDatagramPing failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("AwaitDatagramPing failed: category {} error {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
