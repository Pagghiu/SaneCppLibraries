// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncServer.h"

namespace SC
{

Result HttpAsyncServer::initInternal(SpanWithStride<HttpConnection> connectionsSpan)
{
    for (size_t idx = 0; idx < connectionsSpan.sizeInElements(); ++idx)
    {
        HttpConnection& connection = connectionsSpan[idx];
        if (connection.readableSocketStream.getReadQueueSize() == 0)
        {
            return Result::Error("HttpConnection::readableSocketStream::readQueue is empty");
        }
        if (connection.writableSocketStream.getWriteQueueSize() == 0)
        {
            return Result::Error("HttpConnection::writableSocketStream::writeQueue is empty");
        }
        SC_TRY_MSG(connection.buffersPool.getNumBuffers() > 0, "HttpAsyncServer - AsyncBuffersPool is empty");
    }
    SC_TRY(connections.init(connectionsSpan.castTo<HttpConnection>()));
    return Result(true);
}

Result HttpAsyncServer::resizeInternal(SpanWithStride<HttpConnection> connectionsSpan)
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
    SC_TRY_MSG(state == State::Stopped, "HttpAsyncServer::start requires stopped state");
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
    externalListener = false;
    state            = State::Started;
    return Result(true);
}

Result HttpAsyncServer::startExternal(AsyncEventLoop& loop)
{
    SC_TRY_MSG(state == State::Stopped, "HttpAsyncServer::startExternal requires stopped state");
    SC_TRY_MSG(connections.getNumTotalConnections() > 0, "HttpAsyncServer::startExternal - init not called");
    eventLoop        = &loop;
    externalListener = true;
    state            = State::Started;
    return Result(true);
}

Result HttpAsyncServer::acceptExternalConnection(HttpConnection& connection, AsyncReadableStream& readable,
                                                 AsyncWritableStream& writable)
{
    SC_TRY_MSG(state == State::Started and externalListener,
               "HttpAsyncServer::acceptExternalConnection requires an external listener");
    HttpConnection::ID connectionID;
    SC_TRY_MSG(connections.activate(connection, connectionID),
               "HttpAsyncServer::acceptExternalConnection slot unavailable");

    connection.resetTransportStreams();
    connection.setTransportStreams(readable, writable);
    Result setup = beginTransportConnection(connection);
    if (not setup)
    {
        closeAsync(connection);
        return setup;
    }
    return Result(true);
}

Result HttpAsyncServer::close()
{
    SC_TRY_MSG(state == State::Stopping, "HttpAsyncServer::close requires stop before close");
    SC_TRY(waitForStopToFinish());
    SC_TRY(connections.close());
    externalListener = false;
    eventLoop        = nullptr;
    return Result(true);
}

Result HttpAsyncServer::stop()
{
    SC_TRY_MSG(state == State::Started, "HttpAsyncServer::stop requires started state");

    state = State::Stopping;
    if (not externalListener and not asyncServerAccept.isFree())
    {
        SC_TRY(asyncServerAccept.stop(*eventLoop));
    }

    for (size_t idx = 0; idx < connections.getNumTotalConnections(); ++idx)
    {
        HttpConnection& client = static_cast<HttpConnection&>(connections.getConnectionAt(idx));
        closeAsync(client);
    }
    return Result(true);
}

Result HttpAsyncServer::waitForStopToFinish()
{
    SC_TRY_MSG(state == State::Stopping, "HttpAsyncServer::waitForStopToFinish requires stopping state");
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
            HttpConnection& client = static_cast<HttpConnection&>(connections.getConnectionAt(idx));
            while (not client.readableSocketStream.request.isFree() or not client.writableSocketStream.request.isFree())
            {
                SC_TRY(eventLoop->runNoWait());
                checkAgainAllClients = true;
            }
            SC_HTTP_ASSERT_RELEASE(client.pipeline.unpipe());
        }
    } while (checkAgainAllClients);
    state = State::Stopped;
    return Result(true);
}

struct HttpAsyncServer::EventDataListener
{
    HttpAsyncServer& pself;
    HttpConnection&  client;

    void operator()(AsyncBufferView::ID bufferID) { pself.onStreamReceive(client, bufferID); }
};

struct HttpAsyncServer::EventBodyDataListener
{
    HttpAsyncServer& pself;
    HttpConnection&  client;

    void operator()(AsyncBufferView::ID bufferID) { pself.onRequestBodyData(client, bufferID); }
};

struct HttpAsyncServer::EventEndListener
{
    HttpAsyncServer& pself;
    HttpConnection&  client;

    void operator()()
    {
        // A peer may half-close after sending a complete request and still wait for the response.
        if (client.request.hasReceivedHeaders() and client.request.isBodyComplete())
        {
            return;
        }
        pself.closeAsync(client);
    }
};

struct HttpAsyncServer::EventCloseListener
{
    HttpAsyncServer& pself;
    HttpConnection&  client;

    void operator()()
    {
        // Upgraded owners may destroy streams directly on protocol errors.
        pself.closeAsync(client);
    }
};

void HttpAsyncServer::onNewClient(AsyncSocketAccept::Result& result)
{
    SocketDescriptor acceptedClient;
    Result           accepted = result.moveTo(acceptedClient);
    if (not accepted)
    {
        if (onError.isValid())
        {
            onError(accepted);
        }
        return;
    }
    HttpConnection::ID idx;
    // Activation always succeeds because we pause asyncAccept when the there are not available clients
    SC_HTTP_ASSERT_RELEASE(connections.activateNew(idx));

    HttpConnection& client = static_cast<HttpConnection&>(connections.getConnection(idx));

    SC_HTTP_ASSERT_RELEASE(client.readableSocketStream.request.isFree());
    SC_HTTP_ASSERT_RELEASE(client.writableSocketStream.request.isFree());

    client.socket = move(acceptedClient);
    SC_HTTP_TRUST_RESULT(client.readableSocketStream.init(client.buffersPool, *eventLoop, client.socket));
    SC_HTTP_TRUST_RESULT(client.writableSocketStream.init(client.buffersPool, *eventLoop, client.socket));
    client.resetTransportStreams();
    client.readableSocketStream.setAutoDestroy(true);
    client.writableSocketStream.setAutoDestroy(false); // needed for keep-alive logic

    Result setup = beginTransportConnection(client);
    if (not setup)
    {
        if (onError.isValid())
        {
            onError(setup);
        }
        closeAsync(client);
    }

    // Only reactivate asyncAccept if there are available clients (otherwise it's being reactivated in closeAsync)
    result.reactivateRequest(connections.getNumActiveConnections() < connections.getNumTotalConnections());
}

Result HttpAsyncServer::beginHttpConnection(HttpConnection& client)
{
    client.response.setWritableStream(client.getWritableTransportStream());

    EventDataListener dataListener{*this, client};
    SC_TRY_MSG(client.getReadableTransportStream().eventData.addListener(dataListener),
               "HttpAsyncServer readable data listener unavailable");
    if (client.getReadableTransportStream().canStart())
    {
        SC_TRY(client.getReadableTransportStream().start());
    }
    else
    {
        client.getReadableTransportStream().resumeReading();
    }
    return Result(true);
}

Result HttpAsyncServer::beginTransportConnection(HttpConnection& client)
{
    if (not transportSetup.isValid())
    {
        return beginHttpConnection(client);
    }

    HttpAsyncServerTransportSetup setup;
    setup.connection = &client;
    setup.eventLoop  = eventLoop;
    setup.complete   = {[this, &client](Result result) { onTransportSetupComplete(client, result); }};
    setup.fail       = {[this, &client](Result result)
                        {
                      if (onError.isValid())
                      {
                          onError(result);
                      }
                      closeAsync(client);
                  }};
    return transportSetup(setup);
}

void HttpAsyncServer::onTransportSetupComplete(HttpConnection& client, Result result)
{
    if (not result)
    {
        if (onError.isValid())
        {
            onError(result);
        }
        closeAsync(client);
        return;
    }

    Result setup = beginHttpConnection(client);
    if (not setup)
    {
        if (onError.isValid())
        {
            onError(setup);
        }
        closeAsync(client);
    }
}

void HttpAsyncServer::onStreamReceive(HttpConnection& client, AsyncBufferView::ID bufferID)
{
    Span<char> readData;
    SC_HTTP_ASSERT_RELEASE(client.buffersPool.getWritableData(bufferID, readData));

    Result headerWrite =
        client.request.writeHeaders(maxHeaderSize, readData, client.getReadableTransportStream(), bufferID);
    if (not headerWrite)
    {
        if (onError.isValid())
        {
            onError(headerWrite);
        }
        closeAsync(client);
        return;
    }
    else if (client.request.hasReceivedHeaders())
    {
        client.response.grabUnusedHeaderMemory(client.request);
        client.response.reset();

        // Both with and without body we should stop listening to data events
        SC_HTTP_ASSERT_RELEASE(
            client.getReadableTransportStream().eventData.removeListener(EventDataListener{*this, client}));
        if (client.requestCount > 0)
        {
            SC_HTTP_ASSERT_RELEASE(
                client.getReadableTransportStream().eventEnd.removeListener(EventEndListener{*this, client}));
        }
        Result prepareBody =
            client.request.prepareBodyStream(client.buffersPool,
                                             {[&client]() -> Result
                                              {
                                                  if (not client.request.isBodyComplete())
                                                  {
                                                      client.getReadableTransportStream().resumeReading();
                                                  }
                                                  return Result(true);
                                              }},
                                             false);
        if (not prepareBody)
        {
            if (onError.isValid())
            {
                onError(prepareBody);
            }
            closeAsync(client);
            return;
        }

        if (client.request.getBodyFramingKind() == HttpBodyFramingKind::Chunked or
            client.request.getBodyFramingKind() == HttpBodyFramingKind::ContentLength)
        {
            const bool addedBodyData =
                client.getReadableTransportStream().eventData.addListener(EventBodyDataListener{*this, client});
            SC_HTTP_ASSERT_RELEASE(addedBodyData);
        }

        onRequest(client);

        if (client.isWebSocketUpgraded())
        {
            EventCloseListener closeListener{*this, client};
            if (not client.getReadableTransportStream().eventClose.addListener(closeListener))
            {
                closeAsync(client);
            }
            return;
        }

        // Register before starting the body stream, which can synchronously emit buffered body data.
        struct AfterWrite
        {
            HttpAsyncServer& pself;
            HttpConnection&  client;

            void operator()()
            {
                SC_HTTP_ASSERT_RELEASE(client.response.getWritableStream().eventFinish.removeListener(*this));

                // Determine if we should keep the connection alive
                const bool underMaxRequests =
                    (pself.maxRequestsPerConnection == 0) or (client.requestCount + 1 < pself.maxRequestsPerConnection);
                const bool shouldKeepAlive = client.response.getKeepAlive() and underMaxRequests and
                                             not client.getReadableTransportStream().isEnded();

                if (shouldKeepAlive and pself.state == State::Started) // We may get some after-writes after server stop
                {
                    // Increment request count
                    client.requestCount++;

                    // Reset request and response for next request
                    client.request.setHeaderMemory(client.getHeaderMemory());
                    client.response.reset();

                    if (&client.getWritableTransportStream() == &client.writableSocketStream)
                    {
                        SC_HTTP_ASSERT_RELEASE(client.socket.isValid());
                        Result writableRes =
                            client.writableSocketStream.init(client.buffersPool, *pself.eventLoop, client.socket);
                        SC_HTTP_TRUST_RESULT(writableRes);
                    }
                    else if (pself.transportReuse.isValid())
                    {
                        Result reuseResult = pself.transportReuse(client);
                        if (not reuseResult)
                        {
                            if (pself.onError.isValid())
                            {
                                pself.onError(reuseResult);
                            }
                            pself.closeAsync(client);
                            return;
                        }
                    }

                    // Resume reading in any case to avoid deadlocking
                    client.getReadableTransportStream().resumeReading();

                    // Re-register for next request headers
                    EventDataListener dataListener{pself, client};
                    SC_HTTP_ASSERT_RELEASE(client.getReadableTransportStream().eventData.addListener(dataListener));
                    EventEndListener endListener{pself, client};
                    SC_HTTP_ASSERT_RELEASE(client.getReadableTransportStream().eventEnd.addListener(endListener));
                }
                else
                {
                    pself.shutdownTransport(client);
                }
            }
        };
        SC_HTTP_ASSERT_RELEASE(client.response.getWritableStream().eventFinish.addListener(AfterWrite{*this, client}));

        if (client.request.getBodyFramingKind() == HttpBodyFramingKind::None)
        {
            client.getReadableTransportStream().pause();
        }
        else if (client.request.getBodyFramingKind() == HttpBodyFramingKind::Chunked or
                 client.request.getBodyFramingKind() == HttpBodyFramingKind::ContentLength)
        {
            SC_HTTP_TRUST_RESULT(client.request.startBodyStream());
        }
    }
}

void HttpAsyncServer::onRequestBodyData(HttpConnection& client, AsyncBufferView::ID bufferID)
{
    Span<const char> readData;
    Result           readable = client.buffersPool.getReadableData(bufferID, readData);
    if (not readable)
    {
        if (onError.isValid())
        {
            onError(readable);
        }
        closeAsync(client);
        return;
    }

    Result process = client.request.processBodyData(client.getReadableTransportStream(), bufferID, readData, true);
    if (not process)
    {
        if (onError.isValid())
        {
            onError(process);
        }
        closeAsync(client);
        return;
    }

    if (client.request.isBodyComplete())
    {
        const bool removed =
            client.getReadableTransportStream().eventData.removeListener(EventBodyDataListener{*this, client});
        (void)(removed);
        client.getReadableTransportStream().pause();
    }
}

void HttpAsyncServer::shutdownTransport(HttpConnection& client)
{
    if (not transportShutdown.isValid())
    {
        closeAsync(client);
        return;
    }

    Function<void(Result)> complete = {[this, &client](Result result)
                                       {
                                           if (not result and onError.isValid())
                                           {
                                               onError(result);
                                           }
                                           closeAsync(client);
                                       }};
    Result                 shutdown = transportShutdown(client, move(complete));
    if (not shutdown)
    {
        if (onError.isValid())
        {
            onError(shutdown);
        }
        closeAsync(client);
    }
}

void HttpAsyncServer::closeAsync(HttpConnection& client)
{
    if (client.state == HttpConnection::State::Inactive)
    {
        return;
    }
    client.requestCount = 0;

    // These events may or may not be registered depending on when the close request arrives
    EventDataListener dataListener{*this, client};
    (void)client.getReadableTransportStream().eventData.removeListener(dataListener);
    EventBodyDataListener bodyDataListener{*this, client};
    (void)client.getReadableTransportStream().eventData.removeListener(bodyDataListener);
    EventEndListener endListener{*this, client};
    (void)client.getReadableTransportStream().eventEnd.removeListener(endListener);
    EventCloseListener closeListener{*this, client};
    (void)client.getReadableTransportStream().eventClose.removeListener(closeListener);

    AsyncReadableStream* readable = &client.getReadableTransportStream();
    AsyncWritableStream* writable = &client.getWritableTransportStream();

    struct OnCloseDeactivateReadableTransport
    {
        HttpAsyncServer& pself;
        HttpConnection&  client;

        void operator()()
        {
            SC_HTTP_ASSERT_RELEASE(client.getReadableTransportStream().eventClose.removeListener(*this));
            pself.tryDeactivateConnection(client);
        }
    };
    if (not readable->hasBeenDestroyed())
    {
        SC_HTTP_ASSERT_RELEASE(readable->eventClose.addListener(OnCloseDeactivateReadableTransport{*this, client}));
    }

    struct OnCloseDeactivateWritableTransport
    {
        HttpAsyncServer& pself;
        HttpConnection&  client;

        void operator()()
        {
            SC_HTTP_ASSERT_RELEASE(client.getWritableTransportStream().eventClose.removeListener(*this));
            pself.tryDeactivateConnection(client);
        }
    };
    if (not writable->hasBeenDestroyed())
    {
        SC_HTTP_ASSERT_RELEASE(writable->eventClose.addListener(OnCloseDeactivateWritableTransport{*this, client}));
    }

    struct OnCloseDeactivateReadableSocket
    {
        HttpAsyncServer& pself;
        HttpConnection&  client;

        void operator()()
        {
            SC_HTTP_ASSERT_RELEASE(client.readableSocketStream.eventClose.removeListener(*this));
            pself.tryDeactivateConnection(client);
        }
    };
    if (readable != &client.readableSocketStream and not client.readableSocketStream.hasBeenDestroyed())
    {
        SC_HTTP_ASSERT_RELEASE(
            client.readableSocketStream.eventClose.addListener(OnCloseDeactivateReadableSocket{*this, client}));
    }

    struct OnCloseDeactivateWritableSocket
    {
        HttpAsyncServer& pself;
        HttpConnection&  client;

        void operator()()
        {
            SC_HTTP_ASSERT_RELEASE(client.writableSocketStream.eventClose.removeListener(*this));
            pself.tryDeactivateConnection(client);
        }
    };
    if (writable != &client.writableSocketStream and not client.writableSocketStream.hasBeenDestroyed())
    {
        SC_HTTP_ASSERT_RELEASE(
            client.writableSocketStream.eventClose.addListener(OnCloseDeactivateWritableSocket{*this, client}));
    }

    if (transportClose.isValid())
    {
        transportClose(client);
    }
    if (not readable->hasBeenDestroyed())
    {
        readable->destroy();
    }
    if (not writable->hasBeenDestroyed())
    {
        writable->destroy();
    }
    if (readable != &client.readableSocketStream and not client.readableSocketStream.hasBeenDestroyed())
    {
        client.readableSocketStream.destroy();
    }
    if (writable != &client.writableSocketStream and not client.writableSocketStream.hasBeenDestroyed())
    {
        client.writableSocketStream.destroy();
    }

    tryDeactivateConnection(client);
}

void HttpAsyncServer::tryDeactivateConnection(HttpConnection& client)
{
    if (client.state != HttpConnection::State::Inactive and client.getReadableTransportStream().hasBeenDestroyed() and
        client.getWritableTransportStream().hasBeenDestroyed() and client.readableSocketStream.hasBeenDestroyed() and
        client.writableSocketStream.hasBeenDestroyed())
    {
        deactivateConnection(client);
    }
}

void HttpAsyncServer::deactivateConnection(HttpConnection& client)
{
    SC_HTTP_TRUST_RESULT(client.socket.close());
    client.resetTransportStreams();
    const bool wasFull = connections.getNumActiveConnections() == connections.getNumTotalConnections();
    SC_HTTP_TRUST_RESULT(connections.deactivate(client.getConnectionID()));
    if (wasFull and state == State::Started and not externalListener)
    {
        // onNewClient has paused asyncAccept (by avoiding reactivation) for lack of available clients.
        // Now a client has just been made available so it's possible to start accepting again.
        SC_HTTP_TRUST_RESULT(asyncServerAccept.start(*eventLoop, serverSocket));
    }
}

} // namespace SC
