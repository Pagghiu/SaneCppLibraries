// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpServer.h"
namespace SC
{

/// @brief Async Http Server
///
/// This class handles a fully asynchronous http server staying inside 5 fixed memory regions passed during init.
///
/// Usage:
/// - Use the SC::HttpServer::onRequest callback to intercept new clients connecting
/// - Write to SC::HttpResponse or use SC::HttpAsyncFileServer to statically serve files
///
/// @see SC::HttpAsyncFileServer, SC::HttpServer
///
/// \snippet Tests/Libraries/Http/HttpAsyncServerTest.cpp HttpAsyncServerSnippet
struct SC_COMPILER_EXPORT HttpAsyncServer
{
    /// @brief Initializes the async server with all needed memory buffers
    Result init(Span<HttpServerClient> clients, Span<char> headersMemory, Span<AsyncReadableStream::Request> readQueue,
                Span<AsyncWritableStream::Request> writeQueue, Span<AsyncBufferView> buffers);

    /// @brief Closes the server, removing references to the memory buffers passed during init
    /// @warning If server is started, you must call HttpAsyncServer::stop and HttpAsyncServer::waitForStopToFinish
    /// before this class can be safely destroyed, because it needs to shutdown all in-flight async requests
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

    /// @brief Blocks current thread waiting for the event loop to shutdown active async requests
    /// @note HttpAsyncServer can be destroyed after this function returns
    Result waitForStopToFinish();

    /// @brief Returns true if the server has been started
    [[nodiscard]] bool isStarted() const { return started; }

    /// @brief Access the underlying buffers pool used by the async streams
    AsyncBuffersPool& getBuffersPool() { return buffersPool; }

    /// @brief Access the underlying http server
    HttpServer& getHttpServer() { return httpServer; }

  private:
    HttpServer       httpServer;
    AsyncBuffersPool buffersPool;

    uint32_t maxHeaderSize = 8 * 1024;

    Span<AsyncReadableStream::Request> readQueues;
    Span<AsyncWritableStream::Request> writeQueues;

    bool started  = false;
    bool stopping = false;
    bool memory   = false;

    void onNewClient(AsyncSocketAccept::Result& result);
    void closeAsync(HttpServerClient& requestClient);

    void onStreamReceive(HttpServerClient& client, AsyncBufferView::ID bufferID);

    AsyncEventLoop*   eventLoop = nullptr;
    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncServerAccept;
};
} // namespace SC
