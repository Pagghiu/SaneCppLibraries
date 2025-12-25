// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncServer.h"
#include "../Foundation/Assert.h"
#include "../Foundation/Deferred.h"

namespace SC
{

Result HttpAsyncServer::initInternal(AsyncBuffersPool& pool, SpanWithStride<HttpAsyncConnectionBase> connectionsSpan)
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
    }
    SC_TRY_MSG(pool.getNumBuffers() > 0, "HttpAsyncServer - AsyncBuffersPool is empty");
    SC_TRY(connections.init(connectionsSpan.castTo<HttpConnection>()));
    buffersPool = &pool;
    return Result(true);
}

Result HttpAsyncServer::start(AsyncEventLoop& loop, StringSpan address, uint16_t port)
{
    SC_TRY_MSG(buffersPool != nullptr, "HttpAsyncServer::start - init not called");
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
    buffersPool = nullptr;
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
    HttpConnection::ID idx;
    // Activation always succeeds because we pause asyncAccept when the there are not available clients
    SC_ASSERT_RELEASE(connections.activateNew(idx));

    HttpAsyncConnectionBase& client = static_cast<HttpAsyncConnectionBase&>(connections.getConnection(idx));
    client.socket                   = move(acceptedClient);
    SC_TRUST_RESULT(client.readableSocketStream.init(*buffersPool, *eventLoop, client.socket));
    SC_TRUST_RESULT(client.writableSocketStream.init(*buffersPool, *eventLoop, client.socket));

    auto onData = [this, idx](AsyncBufferView::ID bufferID)
    { onStreamReceive(static_cast<HttpAsyncConnectionBase&>(connections.getConnection(idx)), bufferID); };
    SC_TRUST_RESULT(client.readableSocketStream.eventData.addListener(onData));
    SC_TRUST_RESULT(client.readableSocketStream.start());

    client.response.writableStream = &client.writableSocketStream;

    // Only reactivate asyncAccept if there are available clients (otherwise it's being reactivated in closeAsync)
    result.reactivateRequest(connections.getNumActiveConnections() < connections.getNumTotalConnections());
}

void HttpAsyncServer::onStreamReceive(HttpAsyncConnectionBase& client, AsyncBufferView::ID bufferID)
{
    Span<char> readData;
    SC_ASSERT_RELEASE(buffersPool->getWritableData(bufferID, readData));

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

} // namespace SC
