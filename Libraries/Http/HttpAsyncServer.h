// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpConnection.h"
namespace SC
{
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
    Result init(AsyncBuffersPool& buffersPool, Span<HttpConnection> clients, Span<char> headersMemory,
                Span<AsyncReadableStream::Request> readQueue, Span<AsyncWritableStream::Request> writeQueue);

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
    /// @warning Consider calling waitForStopToFinish before reclaiming memory used by this class
    Result stop();

    /// @brief Returns true if the server has been started
    [[nodiscard]] bool isStarted() const { return started; }

    /// @brief Access the underlying http connections
    HttpConnectionsPool& getConnectionsPool() { return connections; }

    /// @brief Access the underlying AsyncEventLoop
    AsyncEventLoop* getEventLoop() const { return eventLoop; }

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    Function<void(HttpConnection&)> onRequest;

    /// @brief Slices a span of memory used for request in equal parts for all buffers, marking them as re-usable
    /// @param buffers The buffers to be sliced
    /// @param memory The span of memory to be sliced into buffers
    /// @param maxConnections The maximum number of connections / buffers to be sliced
    /// @param numSlicesPerConnection The number of slices to be created for a single connection
    /// @param sizeInBytesPerConnection The size of each slice for a single connection
    [[nodiscard]] static bool sliceReusableEqualMemoryBuffers(Span<AsyncBufferView> buffers, Span<char> memory,
                                                              size_t maxConnections, size_t numSlicesPerConnection,
                                                              size_t sizeInBytesPerConnection);

  private:
    HttpConnectionsPool connections;
    AsyncBuffersPool*   buffersPool = nullptr;

    uint32_t maxHeaderSize = 8 * 1024;

    Span<AsyncReadableStream::Request> readQueues;
    Span<AsyncWritableStream::Request> writeQueues;

    bool started  = false;
    bool stopping = false;
    bool memory   = false;

    void onNewClient(AsyncSocketAccept::Result& result);
    void closeAsync(HttpConnection& requestClient);
    void onStreamReceive(HttpConnection& client, AsyncBufferView::ID bufferID);

    Result waitForStopToFinish();

    AsyncEventLoop*   eventLoop = nullptr;
    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncServerAccept;
};
} // namespace SC
