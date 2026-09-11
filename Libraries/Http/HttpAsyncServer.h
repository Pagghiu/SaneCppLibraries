// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpConnection.h"
#include "HttpExport.h"

namespace SC
{
namespace TypeTraits
{
/// IsBaseOf evaluates to `true` if the type `Base` is a base class of `Derived`, `false` otherwise.
template <typename Base, typename Derived>
struct IsBaseOf
{
    static constexpr bool value = __is_base_of(Base, Derived);
};

} // namespace TypeTraits

/// @brief Adds compile-time configurable read and write queues to HttpConnection
template <int ReadQueue, int WriteQueue, int HeaderBytes, int StreamBytes>
struct SC_HTTP_EXPORT HttpAsyncConnection
    : public HttpStaticConnection<ReadQueue, WriteQueue, HeaderBytes, StreamBytes, 8, HttpConnection>
{
};

/// @brief Mutable transport setup view used by `HttpAsyncServer` after accepting a TCP socket.
///
/// The default setup keeps `connection` using its socket streams. A custom setup can install alternate active streams
/// with `HttpConnectionBase::setTransportStreams()` and must call `complete` when the transport is ready for HTTP.
/// After successful setup, the transport calls `fail` once if a terminal transport error must close the connection.
struct SC_HTTP_EXPORT HttpAsyncServerTransportSetup
{
    HttpConnection* connection = nullptr;
    AsyncEventLoop* eventLoop  = nullptr;

    Function<void(Result)> complete;
    Function<void(Result)> fail;
};

/// @brief Async Http Server
///
/// This class handles a fully asynchronous http server staying inside 5 fixed memory regions passed during init.
///
/// Usage:
/// - Use SC::HttpAsyncServer::onRequest callback to intercept requests from http clients
/// - Write to SC::HttpResponse using the writable stream obtained from HttpConnection::response.getWritableStream()
/// - Alternatively use SC::HttpAsyncFileServer to statically serve files
///
/// @see SC::HttpAsyncFileServer, SC::HttpConnectionsPool
///
/// \snippet Tests/Libraries/Http/HttpAsyncServerTest.cpp HttpAsyncServerSnippet
struct SC_HTTP_EXPORT HttpAsyncServer
{
    /// @brief Initializes the async server with all needed memory buffers
    template <typename T,
              typename = typename TypeTraits::EnableIf<TypeTraits::IsBaseOf<HttpConnection, T>::value>::type>
    Result init(Span<T> clients)
    {
        return initInternal({clients.data(), clients.sizeInElements(), sizeof(T)});
    }

    template <typename T,
              typename = typename TypeTraits::EnableIf<TypeTraits::IsBaseOf<HttpConnection, T>::value>::type>
    Result resize(Span<T> clients)
    {
        return resizeInternal({clients.data(), clients.sizeInElements(), sizeof(T)});
    }

    /// @brief Closes the server, removing references to the memory buffers passed during init
    /// @note This call will wait until all async operations will be finished before returning
    Result close();

    /// @brief Starts the http server on the given AsyncEventLoop, address and port
    /// @param loop The event loop to be used, where to add the listening socket
    /// @param address The address of local interface where to listen to
    /// @param port The local port where to start listening to
    /// @return Valid Result if http listening has been started successfully
    Result start(AsyncEventLoop& loop, StringSpan address, uint16_t port);

    /// @brief Starts the HTTP state machine without creating a native listener.
    ///
    /// An external listener must inject accepted plaintext streams with `acceptExternalConnection()`.
    Result startExternal(AsyncEventLoop& loop);

    /// @brief Activates a specific connection slot for externally accepted plaintext streams.
    ///
    /// This must be called on the server event-loop thread. Both streams and the connection storage remain
    /// caller-owned until Http has destroyed the streams and deactivated the slot.
    Result acceptExternalConnection(HttpConnection& connection, AsyncReadableStream& readable,
                                    AsyncWritableStream& writable);

    /// @brief Stops http server asynchronously pushing cancel and close requests to the event loop
    /// @warning Consider calling HttpAsyncServer::close before reclaiming memory used by this class
    Result stop();

    /// @brief Sets the maximum accepted request-header size in bytes.
    /// The limit must fit inside the per-connection header storage passed to init().
    void setMaxHeaderSize(uint32_t bytes) { maxHeaderSize = bytes; }

    /// @brief Gets the maximum accepted request-header size in bytes.
    [[nodiscard]] uint32_t getMaxHeaderSize() const { return maxHeaderSize; }

    /// @brief Sets an optional transport setup hook invoked after accepting TCP and before HTTP reads request bytes.
    void setTransportSetup(Function<Result(HttpAsyncServerTransportSetup&)>&& setup) { transportSetup = move(setup); }

    /// @brief Sets an optional transport teardown hook invoked before HTTP destroys the installed transport streams.
    /// The hook may start asynchronous stream destruction but must not reset the connection's transport streams.
    void setTransportClose(Function<void(HttpConnection&)>&& close) { transportClose = move(close); }

    /// @brief Sets an optional hook that reopens an alternate writable transport for the next keep-alive response.
    void setTransportReuse(Function<Result(HttpConnection&)>&& reuse) { transportReuse = move(reuse); }

    /// @brief Sets an optional asynchronous hook that drains a terminal response before closing its socket.
    void setTransportShutdown(Function<Result(HttpConnection&, Function<void(Result)>)>&& shutdown)
    {
        transportShutdown = move(shutdown);
    }

    /// @brief Clears optional transport setup, reuse, shutdown, and teardown hooks.
    void clearTransportSetup()
    {
        transportSetup    = {};
        transportReuse    = {};
        transportShutdown = {};
        transportClose    = {};
    }

    /// @brief Returns true if the server has been started
    [[nodiscard]] bool isStarted() const { return state == State::Started; }

    [[nodiscard]] const HttpConnectionsPool& getConnections() const { return connections; }

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    Function<void(HttpConnection&)> onRequest;

    /// @brief Called on accept, parse, protocol, or streaming errors before the affected connection is closed.
    Function<void(Result)> onError;

    /// @brief Set default keep-alive behavior for all connections
    /// @param enabled true to keep connections alive by default (HTTP/1.1 default)
    /// @note Can be overridden per-response via HttpResponse::setKeepAlive()
    void setDefaultKeepAlive(bool enabled) { defaultKeepAlive = enabled; }

    /// @brief Get the default keep-alive setting
    [[nodiscard]] bool getDefaultKeepAlive() const { return defaultKeepAlive; }

    /// @brief Set maximum requests per keep-alive connection
    /// @param maxRequests Maximum requests before closing connection (0 = unlimited)
    void setMaxRequestsPerConnection(uint32_t maxRequests) { maxRequestsPerConnection = maxRequests; }

    /// @brief Get the maximum requests per connection
    [[nodiscard]] uint32_t getMaxRequestsPerConnection() const { return maxRequestsPerConnection; }

  private:
    HttpConnectionsPool connections;

    uint32_t maxHeaderSize = 8 * 1024;

    Function<Result(HttpAsyncServerTransportSetup&)>          transportSetup;
    Function<Result(HttpConnection&)>                         transportReuse;
    Function<Result(HttpConnection&, Function<void(Result)>)> transportShutdown;
    Function<void(HttpConnection&)>                           transportClose;

    enum class State
    {
        Stopped,  // Server was not started at all, or it was stopped and wait(ed)ForStopToFinish
        Started,  // Server has been started (successfully)
        Stopping, // Server has stop() called and needs waitForStopToFinish() call
    };

    State state = State::Stopped;

    bool     defaultKeepAlive         = true; ///< Server-wide keep-alive default
    uint32_t maxRequestsPerConnection = 0;    ///< Max requests per connection (0 = unlimited)

    void   onNewClient(AsyncSocketAccept::Result& result);
    void   closeAsync(HttpConnection& requestClient);
    void   shutdownTransport(HttpConnection& requestClient);
    void   tryDeactivateConnection(HttpConnection& requestClient);
    void   deactivateConnection(HttpConnection& requestClient);
    Result beginHttpConnection(HttpConnection& client);
    Result beginTransportConnection(HttpConnection& client);
    void   onStreamReceive(HttpConnection& client, AsyncBufferView::ID bufferID);
    void   onRequestBodyData(HttpConnection& client, AsyncBufferView::ID bufferID);
    void   onTransportSetupComplete(HttpConnection& client, Result result);

    Result waitForStopToFinish();
    Result initInternal(SpanWithStride<HttpConnection> connections);
    Result resizeInternal(SpanWithStride<HttpConnection> connections);

    AsyncEventLoop*   eventLoop = nullptr;
    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncServerAccept;
    bool              externalListener = false;

    struct EventDataListener;
    struct EventBodyDataListener;
    struct EventEndListener;
    struct EventCloseListener;
};
} // namespace SC
