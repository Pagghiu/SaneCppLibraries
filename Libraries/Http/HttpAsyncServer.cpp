// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncServer.h"
#include "../Foundation/Assert.h"
#include "../Foundation/Deferred.h"

namespace SC
{
Result HttpAsyncServer::start(AsyncEventLoop& loop, StringSpan address, uint16_t port)
{
    SC_TRY_MSG(memory, "HttpAsyncServer::start - init not called");
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    eventLoop = &loop;
    SC_TRY(eventLoop->createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    SocketServer socketServer(serverSocket);
    SC_TRY(socketServer.bind(nativeAddress));
    SC_TRY(socketServer.listen(511));

    asyncServerAccept.setDebugName("HttpServer");
    asyncServerAccept.callback.bind<HttpAsyncServer, &HttpAsyncServer::onNewClient>(*this);
    SC_TRY(asyncServerAccept.start(*eventLoop, serverSocket));
    started = true;
    return Result(true);
}

Result HttpAsyncServer::init(Span<HttpServerClient> clients, Span<char> headersMemory,
                             Span<AsyncReadableStream::Request> readQueue,
                             Span<AsyncWritableStream::Request> writeQueue, Span<AsyncBufferView> buffers)
{
    // TODO: Add some validation of minimum sizes for the queues and the buffers
    SC_TRY(httpServer.init(clients, headersMemory));
    readQueues  = readQueue;
    writeQueues = writeQueue;

    buffersPool.buffers = buffers;

    memory = true;
    return Result(true);
}

Result HttpAsyncServer::close()
{
    SC_TRY(httpServer.close());
    readQueues  = {};
    writeQueues = {};

    buffersPool.buffers = {};

    memory = false;
    return Result(true);
}

Result HttpAsyncServer::stop()
{
    stopping = true;

    auto deferStop = MakeDeferred([this]() { stopping = false; });
    if (not asyncServerAccept.isFree())
    {
        SC_TRY(asyncServerAccept.stop(*eventLoop));
    }

    for (size_t idx = 0; idx < httpServer.getNumTotalClients(); ++idx)
    {
        HttpServerClient& client = httpServer.getClientAt(idx);
        // Destroy can be safely called in any state (including already destroyed)
        client.readableSocketStream.destroy();
        client.writableSocketStream.destroy();
        closeAsync(client);
    }
    return Result(true);
}

Result HttpAsyncServer::waitForStopToFinish()
{
    while (httpServer.getNumActiveClients() > 0)
    {
        SC_TRY(eventLoop->runNoWait());
    }
    while (not asyncServerAccept.isFree())
    {
        SC_TRY(eventLoop->runNoWait());
    }
    bool checkAgainAllClients;
    do
    {
        checkAgainAllClients = false;
        for (size_t idx = 0; idx < httpServer.getNumTotalClients(); ++idx)
        {
            HttpServerClient& client = httpServer.getClientAt(idx);
            while (not client.readableSocketStream.request.isFree() or not client.writableSocketStream.request.isFree())
            {
                SC_TRY(eventLoop->runNoWait());
                checkAgainAllClients = true;
            }
        }
    } while (checkAgainAllClients);
    started = false;
    return Result(true);
}

void HttpAsyncServer::onNewClient(AsyncSocketAccept::Result& result)
{
    SocketDescriptor acceptedClient;
    if (not result.moveTo(acceptedClient))
    {
        // TODO: Invoke an error
        return;
    }
    HttpServerClient::ID idx;
    // Activation always succeeds because we pause asyncAccept when the there are not available clients
    SC_ASSERT_RELEASE(httpServer.activateAvailableClient(idx));

    HttpServerClient& client   = httpServer.getClient(idx);
    client.socket              = move(acceptedClient);
    const size_t readQueueLen  = readQueues.sizeInElements() / httpServer.getNumTotalClients();
    const size_t writeQueueLen = writeQueues.sizeInElements() / httpServer.getNumTotalClients();
    SC_TRUST_RESULT(readQueueLen > 0);
    SC_TRUST_RESULT(writeQueueLen > 0);
    Span<AsyncReadableStream::Request> readQueue;
    Span<AsyncWritableStream::Request> writeQueue;
    SC_TRUST_RESULT(readQueues.sliceStartLength(idx.getIndex() * readQueueLen, readQueueLen, readQueue));
    SC_TRUST_RESULT(writeQueues.sliceStartLength(idx.getIndex() * writeQueueLen, writeQueueLen, writeQueue));
    SC_TRUST_RESULT(client.readableSocketStream.init(buffersPool, readQueue, *eventLoop, client.socket));
    SC_TRUST_RESULT(client.writableSocketStream.init(buffersPool, writeQueue, *eventLoop, client.socket));

    auto onData = [this, idx](AsyncBufferView::ID bufferID) { onStreamReceive(httpServer.getClient(idx), bufferID); };
    SC_TRUST_RESULT(client.readableSocketStream.eventData.addListener(onData));
    SC_TRUST_RESULT(client.readableSocketStream.start());

    client.response.writableStream = &client.writableSocketStream;

    // Only reactivate asyncAccept if there are available clients (otherwise it's being reactivated in closeAsync)
    result.reactivateRequest(httpServer.getNumActiveClients() < httpServer.getNumTotalClients());
}

void HttpAsyncServer::onStreamReceive(HttpServerClient& client, AsyncBufferView::ID bufferID)
{
    Span<char> readData;
    SC_ASSERT_RELEASE(buffersPool.getWritableData(bufferID, readData));

    if (not client.request.writeHeaders(maxHeaderSize, readData))
    {
        // TODO: Invoke on error
        return;
    }
    else if (client.request.headersEndReceived)
    {
        client.response.grabUnusedHeaderMemory(client.request);
        client.readableSocketStream.destroy();      // emits 'eventClose' cancelling pending reads
        client.readableSocketStream.eventData = {}; // De-register data event
        httpServer.onRequest(client);

        // Using a struct instead of a lambda so it can unregister itself
        struct AfterWrite
        {
            HttpAsyncServer&  pself;
            HttpServerClient& client;

            void operator()()
            {
                SC_ASSERT_RELEASE(client.response.writableStream->eventFinish.removeListener(*this));
                pself.closeAsync(client);
            }
        };
        SC_ASSERT_RELEASE(client.response.writableStream->eventFinish.addListener(AfterWrite{*this, client}));
    }
}

void HttpAsyncServer::closeAsync(HttpServerClient& client)
{
    if (client.state == HttpServerClient::State::Inactive)
    {
        return;
    }
    SC_TRUST_RESULT(client.socket.close());
    const bool wasFull = httpServer.getNumActiveClients() == httpServer.getNumTotalClients();

    SC_TRUST_RESULT(httpServer.deactivateClient(client.getClientID()));
    if (wasFull and not stopping)
    {
        // onNewClient has paused asyncAccept (by avoiding reactivation) for lack of available clients.
        // Now a client has just been made available so it's possible to start accepting again.
        SC_TRUST_RESULT(asyncServerAccept.start(*eventLoop, serverSocket));
    }
}
} // namespace SC
