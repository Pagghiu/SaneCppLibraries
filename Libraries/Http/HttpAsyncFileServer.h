// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncStreams/AsyncRequestStreams.h"
#include "HttpAsyncServer.h"

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

/// @brief Http file server statically serves files from a directory
///
/// This class registers the onRequest callback provided by HttpAsyncServer to serves files from a given directory.
///
/// \snippet Tests/Libraries/Http/HttpAsyncFileServerTest.cpp HttpFileServerSnippet
struct SC_COMPILER_EXPORT HttpAsyncFileServer
{
    /// @brief Initialize the web server on the given file system directory to serve
    Result init(StringSpan directoryToServe, Span<HttpAsyncFileServerStream> fileStreams, AsyncBuffersPool& buffersPool,
                AsyncEventLoop& eventLoop, ThreadPool& threadPool);

    /// @brief Serve the file requested by this Http Client on its channel
    /// Call this method in response to HttpConnectionsPool::onRequest to serve a file
    Result serveFile(HttpConnection::ID index, StringSpan url, HttpResponse& response);

    /// @brief Registers to HttpConnectionsPool::onRequest callback to serve files from this file server
    void registerToServeFilesOn(HttpAsyncServer& server);

  private:
    StringPath directory;

    Span<HttpAsyncFileServerStream> fileStreams;

    AsyncBuffersPool* buffersPool = nullptr;
    AsyncEventLoop*   eventLoop   = nullptr;
    ThreadPool*       threadPool;
    struct Internal;
};
} // namespace SC
