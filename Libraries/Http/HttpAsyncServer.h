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
    struct Configuration
    {
        size_t readQueueSize     = 3;
        size_t writeQueueSize    = 3;
        size_t buffersQueueSize  = 6;
        size_t headerBytesLength = 8 * 1024;
        size_t streamBytesLength = 512 * 1024;
    };

    struct SC_COMPILER_EXPORT Memory
    {
        Span<AsyncReadableStream::Request> allReadQueue;
        Span<AsyncWritableStream::Request> allWriteQueue;
        Span<AsyncBufferView>              allBuffers;
        Span<char>                         allHeaders;
        Span<char>                         allStreams;

        Result assignTo(HttpAsyncConnectionBase::Configuration  conf,
                        SpanWithStride<HttpAsyncConnectionBase> connections);
    };
    ReadableSocketStream readableSocketStream;
    WritableSocketStream writableSocketStream;
    SocketDescriptor     socket;
};

/// @brief Adds compile-time configurable read and write queues to HttpAsyncConnectionBase
template <int ReadQueue, int WriteQueue, int HeaderBytes, int StreamBytes>
struct SC_COMPILER_EXPORT HttpAsyncConnection : public HttpAsyncConnectionBase
{
    AsyncReadableStream::Request readQueue[ReadQueue];
    AsyncWritableStream::Request writeQueue[WriteQueue];
    AsyncBufferView              buffers[ReadQueue + WriteQueue];

    char headerStorage[HeaderBytes];
    char streamStorage[StreamBytes];

    constexpr HttpAsyncConnection()
    {
        constexpr const size_t NumSlices   = ReadQueue;
        constexpr const size_t SliceLength = StreamBytes / NumSlices;

        Span<char> memory = streamStorage;
        for (size_t idx = 0; idx < NumSlices; ++idx)
        {
            Span<char> slice;
            (void)memory.sliceStartLength(idx * SliceLength, SliceLength, slice);
            buffers[idx] = slice;
            buffers[idx].setReusable(true);
        }
        HttpConnection::setHeaderMemory(headerStorage);
        HttpConnection::buffersPool.setBuffers(buffers);
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
    Result init(Span<T> clients)
    {
        return initInternal({clients.data(), clients.sizeInElements(), sizeof(T)});
    }

    template <typename T,
              typename = typename TypeTraits::EnableIf<TypeTraits::IsBaseOf<HttpAsyncConnectionBase, T>::value>::type>
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
    [[nodiscard]] bool isStarted() const { return started; }

    [[nodiscard]] const HttpConnectionsPool& getConnections() const { return connections; }

    /// @brief Called after enough data from a newly connected client has arrived, causing all headers to be parsed.
    Function<void(HttpConnection&)> onRequest;

  private:
    HttpConnectionsPool connections;

    uint32_t maxHeaderSize = 8 * 1024;

    bool started  = false;
    bool stopping = false;

    void onNewClient(AsyncSocketAccept::Result& result);
    void closeAsync(HttpAsyncConnectionBase& requestClient);
    void onStreamReceive(HttpAsyncConnectionBase& client, AsyncBufferView::ID bufferID);

    Result waitForStopToFinish();
    Result initInternal(SpanWithStride<HttpAsyncConnectionBase> connections);
    Result resizeInternal(SpanWithStride<HttpAsyncConnectionBase> connections);

    AsyncEventLoop*   eventLoop = nullptr;
    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncServerAccept;

    struct EventDataListener;
};
} // namespace SC
