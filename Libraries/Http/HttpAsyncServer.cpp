// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncServer.h"
#include "../Foundation/Assert.h"
#include "../Foundation/Deferred.h"

namespace SC
{

Result HttpAsyncServer::initInternal(SpanWithStride<HttpAsyncConnectionBase> connectionsSpan)
{
    for (size_t idx = 0; idx < connectionsSpan.sizeInElements(); ++idx)
    {
        HttpAsyncConnectionBase& connection = connectionsSpan[idx];
        if (connection.readableSocketStream.getReadQueueSize() == 0)
        {
            return Result::Error("HttpAsyncConnectionBase::readableSocketStream::readQueue is empty");
        }
        if (connection.writableSocketStream.getWriteQueueSize() == 0)
        {
            return Result::Error("HttpAsyncConnectionBase::readableSocketStream::writeQueue is empty");
        }
        SC_TRY_MSG(connection.buffersPool.getNumBuffers() > 0, "HttpAsyncServer - AsyncBuffersPool is empty");
    }
    SC_TRY(connections.init(connectionsSpan.castTo<HttpConnection>()));
    return Result(true);
}

Result HttpAsyncServer::resizeInternal(SpanWithStride<HttpAsyncConnectionBase> connectionsSpan)
{
    SC_TRY_MSG(&connectionsSpan[0] == &connections.getConnectionAt(0), "HttpAsyncServer::resize changed address");
    SC_TRY_MSG(connectionsSpan.sizeInElements() > connections.getHighestActiveConnection(),
               "HttpAsyncServer::resize connection in use");
    return initInternal(connectionsSpan);
}

Result HttpAsyncServer::start(AsyncEventLoop& loop, StringSpan address, uint16_t port)
{
    SC_TRY_MSG(connections.getNumTotalConnections() > 0, "HttpAsyncServer::start - init not called");
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    eventLoop = &loop;
    SC_TRY(eventLoop->createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    SocketServer socketServer(serverSocket);
    SC_TRY(socketServer.bind(nativeAddress));
    SC_TRY(socketServer.listen(511));

    asyncServerAccept.setDebugName("HttpConnectionsPool");
    asyncServerAccept.callback.bind<HttpAsyncServer, &HttpAsyncServer::onNewClient>(*this);
    SC_TRY(asyncServerAccept.start(*eventLoop, serverSocket));
    started = true;
    return Result(true);
}

Result HttpAsyncServer::close()
{
    SC_TRY(waitForStopToFinish());
    SC_TRY(connections.close());
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

    for (size_t idx = 0; idx < connections.getNumTotalConnections(); ++idx)
    {
        HttpAsyncConnectionBase& client = static_cast<HttpAsyncConnectionBase&>(connections.getConnectionAt(idx));
        // Destroy can be safely called in any state (including already destroyed)
        client.readableSocketStream.destroy();
        client.writableSocketStream.destroy();
        closeAsync(client);
    }
    return Result(true);
}

Result HttpAsyncServer::waitForStopToFinish()
{
    while (connections.getNumActiveConnections() > 0)
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
        for (size_t idx = 0; idx < connections.getNumTotalConnections(); ++idx)
        {
            HttpAsyncConnectionBase& client = static_cast<HttpAsyncConnectionBase&>(connections.getConnectionAt(idx));
            while (not client.readableSocketStream.request.isFree() or not client.writableSocketStream.request.isFree())
            {
                SC_TRY(eventLoop->runNoWait());
                checkAgainAllClients = true;
            }
            SC_ASSERT_RELEASE(client.pipeline.unpipe());
        }
    } while (checkAgainAllClients);
    started = false;
    return Result(true);
}

struct HttpAsyncServer::EventDataListener
{
    HttpAsyncServer&         pself;
    HttpAsyncConnectionBase& client;

    void operator()(AsyncBufferView::ID bufferID) { pself.onStreamReceive(client, bufferID); }
};

void HttpAsyncServer::onNewClient(AsyncSocketAccept::Result& result)
{
    SocketDescriptor acceptedClient;
    if (not result.moveTo(acceptedClient))
    {
        // TODO: Invoke an error
        return;
    }
    HttpConnection::ID idx;
    // Activation always succeeds because we pause asyncAccept when the there are not available clients
    SC_ASSERT_RELEASE(connections.activateNew(idx));

    HttpAsyncConnectionBase& client = static_cast<HttpAsyncConnectionBase&>(connections.getConnection(idx));

    client.socket = move(acceptedClient);
    SC_TRUST_RESULT(client.readableSocketStream.init(client.buffersPool, *eventLoop, client.socket));
    SC_TRUST_RESULT(client.writableSocketStream.init(client.buffersPool, *eventLoop, client.socket));
    SC_TRUST_RESULT(client.readableSocketStream.eventData.addListener(EventDataListener{*this, client}));
    SC_TRUST_RESULT(client.readableSocketStream.start());

    client.response.writableStream = &client.writableSocketStream;

    // Only reactivate asyncAccept if there are available clients (otherwise it's being reactivated in closeAsync)
    result.reactivateRequest(connections.getNumActiveConnections() < connections.getNumTotalConnections());
}

void HttpAsyncServer::onStreamReceive(HttpAsyncConnectionBase& client, AsyncBufferView::ID bufferID)
{
    Span<char> readData;
    SC_ASSERT_RELEASE(client.buffersPool.getWritableData(bufferID, readData));

    if (not client.request.writeHeaders(maxHeaderSize, readData, client.readableSocketStream, bufferID))
    {
        // TODO: Invoke on error
        return;
    }
    else if (client.request.headersEndReceived)
    {
        client.response.grabUnusedHeaderMemory(client.request);
        if (client.request.getParser().contentLength == 0)
        {
            // If there is no body, we destroy the stream to cancel pending reads
            client.readableSocketStream.destroy(); // emits 'eventClose' cancelling pending reads
        }
        // Both with and without body we should stop listening to data events
        SC_ASSERT_RELEASE(client.readableSocketStream.eventData.removeListener(EventDataListener{*this, client}));
        onRequest(client);

        // Using a struct instead of a lambda so it can unregister itself
        struct AfterWrite
        {
            HttpAsyncServer&         pself;
            HttpAsyncConnectionBase& client;

            void operator()()
            {
                SC_ASSERT_RELEASE(client.response.writableStream->eventFinish.removeListener(*this));
                pself.closeAsync(client);
            }
        };
        SC_ASSERT_RELEASE(client.response.writableStream->eventFinish.addListener(AfterWrite{*this, client}));
    }
}

void HttpAsyncServer::closeAsync(HttpAsyncConnectionBase& client)
{
    if (client.state == HttpConnection::State::Inactive)
    {
        return;
    }
    SC_TRUST_RESULT(client.socket.close());
    const bool wasFull = connections.getNumActiveConnections() == connections.getNumTotalConnections();

    SC_TRUST_RESULT(connections.deactivate(client.getConnectionID()));
    if (wasFull and not stopping)
    {
        // onNewClient has paused asyncAccept (by avoiding reactivation) for lack of available clients.
        // Now a client has just been made available so it's possible to start accepting again.
        SC_TRUST_RESULT(asyncServerAccept.start(*eventLoop, serverSocket));
    }
}

Result HttpAsyncConnectionBase::Memory::assignTo(HttpAsyncConnectionBase::Configuration  conf,
                                                 SpanWithStride<HttpAsyncConnectionBase> connections)
{
    const size_t numClients = connections.sizeInElements();
    SC_TRY_MSG(allReadQueue.sizeInElements() >= numClients * conf.readQueueSize, "Insufficient read queue");
    SC_TRY_MSG(allWriteQueue.sizeInElements() >= numClients * conf.writeQueueSize, "Insufficient write queue");
    SC_TRY_MSG(allBuffers.sizeInElements() >= numClients * conf.buffersQueueSize, "Insufficient buffers queue");
    SC_TRY_MSG(allHeaders.sizeInElements() >= numClients * conf.headerBytesLength, "Insufficient headers storage");
    SC_TRY_MSG(allStreams.sizeInElements() >= numClients * conf.streamBytesLength, "Insufficient streams storage");
    for (size_t idx = 0; idx < numClients; ++idx)
    {
        HttpAsyncConnectionBase& connection = connections[idx];

        const size_t NumSlices   = conf.readQueueSize;
        const size_t SliceLength = conf.streamBytesLength / NumSlices;

        Span<AsyncBufferView> buffers;
        SC_TRY(allBuffers.sliceStartLength(idx * conf.buffersQueueSize, conf.buffersQueueSize, buffers));
        Span<char> streamStorage;
        SC_TRY(allStreams.sliceStartLength(idx * conf.streamBytesLength, conf.streamBytesLength, streamStorage));
        for (size_t sliceIdx = 0; sliceIdx < NumSlices; ++sliceIdx)
        {
            Span<char> slice;
            SC_TRY(streamStorage.sliceStartLength(sliceIdx * SliceLength, SliceLength, slice));
            buffers[sliceIdx] = slice;
            buffers[sliceIdx].setReusable(true);
        }
        connection.buffersPool.setBuffers(buffers);
        Span<char> headerStorage;
        SC_TRY(allHeaders.sliceStartLength(idx * conf.headerBytesLength, conf.headerBytesLength, headerStorage));
        connection.setHeaderMemory(headerStorage);
        Span<AsyncReadableStream::Request> readQueue;
        SC_TRY(allReadQueue.sliceStartLength(idx * conf.readQueueSize, conf.readQueueSize, readQueue));
        Span<AsyncWritableStream::Request> writeQueue;
        SC_TRY(allWriteQueue.sliceStartLength(idx * conf.writeQueueSize, conf.writeQueueSize, writeQueue));
        connection.readableSocketStream.setReadQueue(readQueue);
        connection.writableSocketStream.setWriteQueue(writeQueue);
    }
    return Result(true);
}
} // namespace SC
