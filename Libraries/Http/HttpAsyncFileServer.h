// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncStreams/AsyncRequestStreams.h"
#include "HttpServer.h"

namespace SC
{

/// @brief Support class for HttpAsyncFileServer holding file stream and pipeline
struct SC_COMPILER_EXPORT HttpAsyncFileServerStream
{
    AsyncPipeline      pipeline;
    ReadableFileStream readableFileStream;
    AsyncTaskSequence  readStreamTask;

    AsyncReadableStream::Request requests[3];
};

/// @brief Http web server helps statically serves files from a directory.
/// @n
/// It can be used in conjunction with SC::HttpServer, by calling SC::HttpAsyncFileServer::serveFile
/// inside the SC::HttpServer::onRequest callback to statically serve files.
///
/// @see SC::HttpServer
///
/// \snippet Tests/Libraries/Http/HttpAsyncFileServerTest.cpp HttpFileServerSnippet
struct SC_COMPILER_EXPORT HttpAsyncFileServer
{
    /// @brief Initialize the web server on the given file system directory to serve
    Result init(StringSpan directoryToServe, Span<HttpAsyncFileServerStream> fileStreams, AsyncBuffersPool& buffersPool,
                AsyncEventLoop& eventLoop, ThreadPool* threadPool = nullptr);

    /// @brief Serve the file requested by this Http Client on its channel
    /// Call this method in response to HttpServer::onRequest to serve a file
    Result serveFile(HttpServerClient::ID index, StringSpan url, HttpResponse& response);

    /// @brief Registers to HttpServer::onRequest callback to serve files from this file server
    void serveFilesOn(HttpServer& server);

  private:
    StringPath directory;

    Span<HttpAsyncFileServerStream> fileStreams;

    AsyncBuffersPool* buffersPool = nullptr;
    AsyncEventLoop*   eventLoop   = nullptr;
    ThreadPool*       threadPool;
    struct Internal;
};
} // namespace SC
