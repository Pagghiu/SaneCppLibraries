// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncServer.h"
#include "../Foundation/Assert.h"

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
    if (connections.getNumTotalConnections() > 0 and not connectionsSpan.empty())
    {
        SC_TRY_MSG(&connectionsSpan[0] == &connections.getConnectionAt(0), "HttpAsyncServer::resize changed address");
    }
    SC_TRY_MSG(connectionsSpan.sizeInElements() > connections.getHighestActiveConnection(),
               "HttpAsyncServer::resize connection in use");
    return initInternal(connectionsSpan);
}

Result HttpAsyncServer::start(AsyncEventLoop& loop, StringSpan address, uint16_t port)
{
    SC_TRY_MSG(state == State::Stopped, "Must be in stopped state");
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
    state = State::Started;
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
    SC_TRY_MSG(state == State::Started, "Must be in started state");

    state = State::Stopping;
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
    SC_TRY_MSG(state == State::Stopping, "Must be in stopping state");
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
    state = State::Stopped;
    return Result(true);
}

struct HttpAsyncServer::EventDataListener
{
    HttpAsyncServer&         pself;
    HttpAsyncConnectionBase& client;

    void operator()(AsyncBufferView::ID bufferID) { pself.onStreamReceive(client, bufferID); }
};

struct HttpAsyncServer::EventEndListener
{
    HttpAsyncServer&         pself;
    HttpAsyncConnectionBase& client;

    void operator()()
    {
        // other party disconnected
        pself.closeAsync(client);
    }
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

    SC_ASSERT_RELEASE(client.readableSocketStream.request.isFree());
    SC_ASSERT_RELEASE(client.writableSocketStream.request.isFree());

    client.socket = move(acceptedClient);
    SC_TRUST_RESULT(client.readableSocketStream.init(client.buffersPool, *eventLoop, client.socket));
    SC_TRUST_RESULT(client.writableSocketStream.init(client.buffersPool, *eventLoop, client.socket));
    client.readableSocketStream.setAutoDestroy(true);
    client.writableSocketStream.setAutoDestroy(false); // needed for keep-alive logic

    EventDataListener dataListener{*this, client};
    SC_TRUST_RESULT(client.readableSocketStream.eventData.addListener(dataListener));
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

        // Both with and without body we should stop listening to data events
        SC_ASSERT_RELEASE(client.readableSocketStream.eventData.removeListener(EventDataListener{*this, client}));
        if (client.requestCount > 0)
        {
            SC_ASSERT_RELEASE(client.readableSocketStream.eventEnd.removeListener(EventEndListener{*this, client}));
        }
        const size_t contentLength = client.request.getParser().contentLength;
        if (contentLength == 0)
        {
            // If there is no body, we pause the stream
            client.readableSocketStream.pause();
        }

        onRequest(client);

        // Using a struct instead of a lambda so it can unregister itself
        struct AfterWrite
        {
            HttpAsyncServer&         pself;
            HttpAsyncConnectionBase& client;

            void operator()()
            {
                SC_ASSERT_RELEASE(client.response.writableStream->eventFinish.removeListener(*this));

                // Determine if we should keep the connection alive
                const bool underMaxRequests =
                    (pself.maxRequestsPerConnection == 0) or (client.requestCount + 1 < pself.maxRequestsPerConnection);
                const bool shouldKeepAlive =
                    client.response.getKeepAlive() and underMaxRequests and not client.readableSocketStream.isEnded();

                if (shouldKeepAlive and pself.state == State::Started) // We may get some after-writes after server stop
                {
                    SC_ASSERT_RELEASE(client.socket.isValid());
                    // Increment request count
                    client.requestCount++;

                    // Reset request and response for next request
                    client.request.reset();
                    client.response.reset();
                    client.request.availableHeader = client.headerMemory;
                    (void)client.request.availableHeader.sliceStartLength(0, 0, client.request.readHeaders);

                    Result writableRes =
                        client.writableSocketStream.init(client.buffersPool, *pself.eventLoop, client.socket);
                    SC_TRUST_RESULT(writableRes);

                    // Resume reading in any case to avoid deadlocking
                    client.readableSocketStream.resumeReading();

                    // Re-register for next request headers
                    EventDataListener dataListener{pself, client};
                    SC_ASSERT_RELEASE(client.readableSocketStream.eventData.addListener(dataListener));
                    EventEndListener endListener{pself, client};
                    SC_ASSERT_RELEASE(client.readableSocketStream.eventEnd.addListener(endListener));
                }
                else
                {
                    pself.closeAsync(client);
                }
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
    client.requestCount = 0;

    // These events may or may not be registered depending on when the close request arrives
    EventDataListener dataListener{*this, client};
    (void)client.readableSocketStream.eventData.removeListener(dataListener);
    EventEndListener endListener{*this, client};
    (void)client.readableSocketStream.eventEnd.removeListener(endListener);

    const bool readWasDestroyed  = client.readableSocketStream.hasBeenDestroyed();
    const bool writeWasDestroyed = client.writableSocketStream.hasBeenDestroyed();
    struct OnCloseDeactivateReadable
    {
        HttpAsyncServer&         pself;
        HttpAsyncConnectionBase& client;

        void operator()()
        {
            SC_ASSERT_RELEASE(client.readableSocketStream.eventClose.removeListener(*this));
            if (client.writableSocketStream.hasBeenDestroyed())
            {
                if (client.state != HttpConnection::State::Inactive)
                {
                    pself.deactivateConnection(client);
                }
            }
        }
    };
    if (not readWasDestroyed)
    {
        SC_ASSERT_RELEASE(client.readableSocketStream.eventClose.addListener(OnCloseDeactivateReadable{*this, client}));
        client.readableSocketStream.destroy();
    }

    struct OnCloseDeactivateWritable
    {
        HttpAsyncServer&         pself;
        HttpAsyncConnectionBase& client;

        void operator()()
        {
            SC_ASSERT_RELEASE(client.writableSocketStream.eventClose.removeListener(*this));
            if (client.readableSocketStream.hasBeenDestroyed())
            {
                if (client.state != HttpConnection::State::Inactive)
                {
                    pself.deactivateConnection(client);
                }
            }
        }
    };
    if (not writeWasDestroyed)
    {
        SC_ASSERT_RELEASE(client.writableSocketStream.eventClose.addListener(OnCloseDeactivateWritable{*this, client}));
        client.writableSocketStream.destroy();
    }

    if (readWasDestroyed and writeWasDestroyed)
    {
        deactivateConnection(client);
    }
}

void HttpAsyncServer::deactivateConnection(HttpAsyncConnectionBase& client)
{
    SC_TRUST_RESULT(client.socket.close());
    const bool wasFull = connections.getNumActiveConnections() == connections.getNumTotalConnections();
    SC_TRUST_RESULT(connections.deactivate(client.getConnectionID()));
    if (wasFull and state == State::Started)
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
