// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncStreams/AsyncRequestStreams.h"
#include "HttpAsyncServer.h"
#include "HttpMultipartParser.h"

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
        ReadableFileStream  readableFileStream;
        WritableFileStream  writableFileStream;
        AsyncTaskSequence   readableFileStreamTask;
        HttpMultipartParser multipartParser;

        struct MultipartListener
        {
            HttpAsyncFileServer*     server     = nullptr;
            Stream*                  stream     = nullptr;
            HttpAsyncConnectionBase* connection = nullptr;

            FileDescriptor currentFd;
            StringPath     currentFilePath;
            StringSpan     currentFileName;
            StringSpan     currentHeaderName;
            bool           isContentDisposition = false;

            void onData(AsyncBufferView::ID bufferID);
        } multipartListener;
    };

    template <int RequestsSize>
    struct SC_COMPILER_EXPORT StreamQueue : public Stream
    {
        StreamQueue()
        {
            readableFileStream.setReadQueue(readQueue);
            writableFileStream.setWriteQueue(writeQueue);
        }
        AsyncReadableStream::Request readQueue[RequestsSize];
        AsyncWritableStream::Request writeQueue[RequestsSize];
    };

    /// @brief Initialize the web server on the given file system directory to serve
    Result init(ThreadPool& threadPool, AsyncEventLoop& loop, StringSpan directoryToServe);

    /// @brief Removes any reference to the arguments passed during init
    Result close();

    /// @brief Handles the request, serving the requested file (GET) or creating a new one (PUT/POST)
    /// Call this method in response to HttpConnectionsPool::onRequest.
    Result handleRequest(HttpAsyncFileServer::Stream& stream, HttpConnection& connection);

  private:
    Result putFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection, StringSpan filePath);
    Result getFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection, StringSpan filePath);
    Result postMultipart(HttpAsyncFileServer::Stream& stream, HttpConnection& connection);

    StringPath directory;

    AsyncEventLoop* eventLoop  = nullptr;
    ThreadPool*     threadPool = nullptr;
    struct Internal;
};

} // namespace SC
