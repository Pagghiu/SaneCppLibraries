// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncStreams/AsyncRequestStreams.h"
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

/// @brief Contains fields used by HttpAsyncServer for each connection
struct SC_COMPILER_EXPORT HttpAsyncConnectionBase : public HttpConnection
{
    ReadableSocketStream readableSocketStream;
    WritableSocketStream writableSocketStream;
    SocketDescriptor     socket;
};

/// @brief Adds compile-time configurable read and write queues to HttpAsyncConnectionBase
template <int ReadQueue, int WriteQueue>
struct SC_COMPILER_EXPORT HttpAsyncConnection : public HttpAsyncConnectionBase
{
    ReadableFileStream::Request readQueue[ReadQueue];
    WritableFileStream::Request writeQueue[WriteQueue];

    HttpAsyncConnection()
    {
        readableSocketStream.setReadQueue(readQueue);
        writableSocketStream.setWriteQueue(writeQueue);
    }
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
              typename = typename TypeTraits::EnableIf<TypeTraits::IsBaseOf<HttpAsyncConnectionBase, T>::value>::type>
    Result init(AsyncBuffersPool& pool, Span<T> clients, Span<char> headersMemory)
    {
        return initInternal(pool, {clients.data(), clients.sizeInElements(), sizeof(T)}, headersMemory);
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
    /// @warning Consider calling waitForStopToFinish before reclaiming memory used by this class
    Result stop();

    /// @brief Returns true if the server has been started
    [[nodiscard]] bool isStarted() const { return started; }

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    Function<void(HttpConnection&)> onRequest;

  private:
    HttpConnectionsPool connections;
    AsyncBuffersPool*   buffersPool = nullptr;

    uint32_t maxHeaderSize = 8 * 1024;

    bool started  = false;
    bool stopping = false;

    void onNewClient(AsyncSocketAccept::Result& result);
    void closeAsync(HttpAsyncConnectionBase& requestClient);
    void onStreamReceive(HttpAsyncConnectionBase& client, AsyncBufferView::ID bufferID);

    Result waitForStopToFinish();
    Result initInternal(AsyncBuffersPool& pool, SpanWithStride<HttpConnection> connections, Span<char> headersMemory);

    AsyncEventLoop*   eventLoop = nullptr;
    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncServerAccept;
};
} // namespace SC
