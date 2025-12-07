// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpServer.h"
namespace SC
{
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
struct SC_COMPILER_EXPORT HttpAsyncServer
{
    /// @brief Starts the http server on the given AsyncEventLoop, address and port
    /// @param loop The event loop to be used, where to add the listening socket
    /// @param address The address of local interface where to listen to
    /// @param port The local port where to start listening to
    /// @param memory Memory buffers to be used by the http server
    /// @return Valid Result if http listening has been started successfully
    Result start(AsyncEventLoop& loop, StringSpan address, uint16_t port, HttpServer::Memory& memory);

    /// @brief Stops http server asynchronously pushing cancel and close requests for next SC::AsyncEventLoop::runOnce
    Result stopAsync();

    /// @brief Stops http server synchronously waiting for SC::AsyncEventLoop::runNoWait to cancel or close all requests
    Result stopSync();

    /// @brief Returns true if the server has been started
    [[nodiscard]] bool isStarted() const { return started; }

    /// @brief Enables using AsyncStreams instead of raw Async Send and Receive
    void setupStreamsMemory(Span<AsyncReadableStream::Request> readQueue, Span<AsyncWritableStream::Request> writeQueue,
                            Span<AsyncBufferView> buffers);

    /// @brief The underlying http server
    HttpServer httpServer;

  private:
    AsyncBuffersPool buffersPool;

    Span<AsyncReadableStream::Request> readQueues;
    Span<AsyncWritableStream::Request> writeQueues;

    bool started  = false;
    bool stopping = false;

    void onNewClient(AsyncSocketAccept::Result& result);
    void closeAsync(HttpServerClient& requestClient);

    void onStreamReceive(HttpServerClient& client, AsyncBufferView::ID bufferID);

    AsyncEventLoop*   eventLoop = nullptr;
    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncServerAccept;
};
#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif
} // namespace SC
