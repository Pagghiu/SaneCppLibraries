// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpConnection.h"

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
struct SC_COMPILER_EXPORT HttpAsyncConnection
    : public HttpStaticConnection<ReadQueue, WriteQueue, HeaderBytes, StreamBytes, 8, HttpConnection>
{
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
struct SC_COMPILER_EXPORT HttpAsyncServer
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

    /// @brief Stops http server asynchronously pushing cancel and close requests to the event loop
    /// @warning Consider calling HttpAsyncServer::close before reclaiming memory used by this class
    Result stop();

    /// @brief Returns true if the server has been started
    [[nodiscard]] bool isStarted() const { return state == State::Started; }

    [[nodiscard]] const HttpConnectionsPool& getConnections() const { return connections; }

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    Function<void(HttpConnection&)> onRequest;

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

    enum class State
    {
        Stopped,  // Server was not started at all, or it was stopped and wait(ed)ForStopToFinish
        Started,  // Server has been started (successfully)
        Stopping, // Server has stop() called and needs waitForStopToFinish() call
    };

    State state = State::Stopped;

    bool     defaultKeepAlive         = true; ///< Server-wide keep-alive default
    uint32_t maxRequestsPerConnection = 0;    ///< Max requests per connection (0 = unlimited)

    void onNewClient(AsyncSocketAccept::Result& result);
    void closeAsync(HttpConnection& requestClient);
    void deactivateConnection(HttpConnection& requestClient);
    void onStreamReceive(HttpConnection& client, AsyncBufferView::ID bufferID);
    void onRequestBodyData(HttpConnection& client, AsyncBufferView::ID bufferID);

    Result waitForStopToFinish();
    Result initInternal(SpanWithStride<HttpConnection> connections);
    Result resizeInternal(SpanWithStride<HttpConnection> connections);

    AsyncEventLoop*   eventLoop = nullptr;
    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncServerAccept;

    struct EventDataListener;
    struct EventBodyDataListener;
    struct EventEndListener;
};
} // namespace SC
