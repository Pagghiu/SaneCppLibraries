// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncStreams/AsyncRequestStreams.h"
#include "HttpAsyncServer.h"

namespace SC
{
/// @brief Http file server statically serves files from a directory
///
/// This class registers the onRequest callback provided by HttpAsyncServer to serves files from a given directory.
///
/// Example using compile time set buffers for connections:
/// \snippet Tests/Libraries/Http/HttpAsyncFileServerTest.cpp HttpFileServerSnippet
///
/// Example using dynamically allocated buffers for connections:
/// \snippet Examples/SCExample/Examples/WebServerExample/WebServerExample.cpp WebServerExampleSnippet
struct SC_COMPILER_EXPORT HttpAsyncFileServer
{
    /// @brief Support class for HttpAsyncFileServer holding file stream and pipeline
    struct SC_COMPILER_EXPORT Stream
    {
        ReadableFileStream readableFileStream;
        AsyncTaskSequence  readableFileStreamTask;
    };

    template <int RequestsSize>
    struct SC_COMPILER_EXPORT StreamQueue : public Stream
    {
        StreamQueue() { readableFileStream.setReadQueue(requests); }
        AsyncReadableStream::Request requests[RequestsSize];
    };

    /// @brief Initialize the web server on the given file system directory to serve
    Result init(ThreadPool& threadPool, AsyncEventLoop& loop, StringSpan directoryToServe);

    /// @brief Removes any reference to the arguments passed during init
    Result close();

    /// @brief Serve the file requested by this Http Client on its channel
    /// Call this method in response to HttpConnectionsPool::onRequest to serve a file
    Result serveFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection);

  private:
    StringPath directory;

    AsyncEventLoop* eventLoop  = nullptr;
    ThreadPool*     threadPool = nullptr;
    struct Internal;
};

} // namespace SC
