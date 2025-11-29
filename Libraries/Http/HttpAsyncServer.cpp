// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncServer.h"
#include "../Foundation/Deferred.h"

namespace SC
{
Result HttpAsyncServer::start(AsyncEventLoop& loop, StringSpan address, uint16_t port, HttpServer::Memory& memory)
{
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
    return httpServer.start(memory);
}

void HttpAsyncServer::setUseAsyncStreams(bool useAsyncStreams, Span<AsyncReadableStream::Request> readQueue,
                                         Span<AsyncWritableStream::Request> writeQueue, Span<AsyncBufferView> buffers)
{
    readQueues  = readQueue;
    writeQueues = writeQueue;
    useStreams  = useAsyncStreams;

    buffersPool.buffers = buffers;
}

Result HttpAsyncServer::stopAsync()
{
    stopping       = true;
    auto deferStop = MakeDeferred([this]() { stopping = false; });
    if (not asyncServerAccept.isFree())
    {
        SC_TRY(asyncServerAccept.stop(*eventLoop));
    }

    for (HttpServerClient& it : httpServer.clients)
    {
        closeAsync(it);
    }
    return Result(true);
}

Result HttpAsyncServer::stopSync()
{
    stopping       = true;
    auto deferStop = MakeDeferred([this]() { stopping = false; });
    SC_TRY(stopAsync());
    while (httpServer.getNumClients() > 0)
    {
        SC_TRY(eventLoop->runNoWait());
    }
    while (not asyncServerAccept.isFree())
    {
        SC_TRY(eventLoop->runNoWait());
    }
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
    size_t idx = 0;
    // Allocate always succeeds because we pause asyncAccept when the arena is full
    SC_ASSERT_RELEASE(httpServer.allocateClient(idx));

    HttpServerClient& client = httpServer.clients[idx];
    client.socket            = move(acceptedClient);
    if (useStreams)
    {
        const size_t readQueueLen  = readQueues.sizeInElements() / httpServer.clients.sizeInElements();
        const size_t writeQueueLen = writeQueues.sizeInElements() / httpServer.clients.sizeInElements();
        SC_TRUST_RESULT(readQueueLen > 0);
        SC_TRUST_RESULT(writeQueueLen > 0);
        Span<AsyncReadableStream::Request> readQueue;
        Span<AsyncWritableStream::Request> writeQueue;
        SC_TRUST_RESULT(readQueues.sliceStartLength(idx * readQueueLen, readQueueLen, readQueue));
        SC_TRUST_RESULT(writeQueues.sliceStartLength(idx * writeQueueLen, writeQueueLen, writeQueue));
        SC_TRUST_RESULT(client.readableSocketStream.init(buffersPool, readQueue, *eventLoop, client.socket));
        SC_TRUST_RESULT(client.writableSocketStream.init(buffersPool, writeQueue, *eventLoop, client.socket));

        auto onData = [this, idx](AsyncBufferView::ID bufferID) { onStreamReceive(httpServer.clients[idx], bufferID); };
        SC_TRUST_RESULT(client.readableSocketStream.eventData.addListener(onData));
        SC_TRUST_RESULT(client.readableSocketStream.start());
    }
    else
    {
        client.asyncSend.setDebugName(client.debugName);
        client.asyncReceive.setDebugName(client.debugName);
        client.asyncReceive.callback.bind<HttpAsyncServer, &HttpAsyncServer::onReceive>(*this);

        // This cannot fail because start reports only incorrect API usage (AsyncRequest already in use etc.)
        SC_TRUST_RESULT(client.asyncReceive.start(*eventLoop, client.socket, client.request.availableHeader));
    }

    // Only reactivate asyncAccept if arena is not full (otherwise it's being reactivated in closeAsync)
    result.reactivateRequest(httpServer.canAcceptMoreClients());
}

void HttpAsyncServer::onStreamReceive(HttpServerClient& client, AsyncBufferView::ID bufferID)
{
    Span<char> readData;
    buffersPool.getWritableData(bufferID, readData);
    // TODO: Handle error for available headers not big enough
    SC_ASSERT_RELEASE(readData.sizeInBytes() <= client.request.availableHeader.sizeInBytes());
    ::memcpy(client.request.availableHeader.data(), readData.data(), readData.sizeInBytes());

    if (not client.request.parse(httpServer.maxHeaderSize, readData))
    {
        // TODO: Invoke on error
        return;
    }
    else if (client.request.allHeadersReceived())
    {
        httpServer.onRequest(client.request, client.response);
    }

    if (client.response.mustBeFlushed())
    {
        client.readableSocketStream.destroy();      // emits 'eventClose' cancelling pending reads
        client.readableSocketStream.eventData = {}; // De-register data event

        auto   onAfterWrite = [this, &client](AsyncBufferView::ID) { closeAsync(client); };
        Result res          = client.writableStream->write(client.response.getSpan(), onAfterWrite);
        SC_TRUST_RESULT(res);
        client.writableStream->end(); // TODO: This must be called only if actually ended...
    }
}

void HttpAsyncServer::onReceive(AsyncSocketReceive::Result& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF
    HttpServerClient& client = SC_COMPILER_FIELD_OFFSET(HttpServerClient, asyncReceive, result.getAsync());
    SC_COMPILER_WARNING_POP
    SC_ASSERT_RELEASE(&client.asyncReceive == &result.getAsync());
    Span<char> readData;
    if (not result.get(readData))
    {
        // TODO: Invoke on error
        return;
    }

    if (not client.request.parse(httpServer.maxHeaderSize, readData))
    {
        // TODO: Invoke on error
        return;
    }
    else if (client.request.allHeadersReceived())
    {
        httpServer.onRequest(client.request, client.response);
    }

    if (client.response.mustBeFlushed())
    {
        Span<const char> outspan = client.response.getSpan();
        client.asyncSend.setDebugName(client.debugName);
        client.asyncSend.callback.bind<HttpAsyncServer, &HttpAsyncServer::onAfterSend>(*this);
        auto res = client.asyncSend.start(*eventLoop, client.socket, outspan);
        if (not res)
        {
            // TODO: Invoke on error
            return;
        }
    }
    else if (not result.completionData.disconnected)
    {
        result.reactivateRequest(true);
    }
}

void HttpAsyncServer::onAfterSend(AsyncSocketSend::Result& result)
{
    if (result.isValid())
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF
        HttpServerClient& requestClient = SC_COMPILER_FIELD_OFFSET(HttpServerClient, asyncSend, result.getAsync());
        SC_COMPILER_WARNING_POP
        closeAsync(requestClient);
    }
}

void HttpAsyncServer::closeAsync(HttpServerClient& requestClient)
{
    if (requestClient.state == HttpServerClient::State::Free)
    {
        return;
    }

    if (not useStreams)
    {
        if (not requestClient.asyncSend.isFree())
        {
            (void)requestClient.asyncSend.stop(*eventLoop);
        }
        if (not requestClient.asyncReceive.isFree())
        {
            (void)requestClient.asyncReceive.stop(*eventLoop);
        }
    }
    SC_TRUST_RESULT(requestClient.socket.close());
    const bool wasFull = not httpServer.canAcceptMoreClients();

    SC_TRUST_RESULT(httpServer.deallocateClient(requestClient));
    if (wasFull and not stopping)
    {
        // Arena was full and in onNewClient asyncAccept has been paused (by avoiding reactivation).
        // Now a slot has been freed so it's possible to start accepting clients again.
        SC_TRUST_RESULT(asyncServerAccept.start(*eventLoop, serverSocket));
    }
}
} // namespace SC
