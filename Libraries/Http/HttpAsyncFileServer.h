// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Async/Async.h"
#include "../AsyncStreams/AsyncRequestStreams.h"
#include "HttpConnection.h"
#include "HttpExport.h"
#include "HttpMultipartParser.h"

namespace SC
{
/// @brief Options controlling optional HttpAsyncFileServer semantics.
struct SC_HTTP_EXPORT HttpAsyncFileServerOptions
{
    /// @brief Optional SPA fallback path. The StringSpan must outlive the file server.
    StringSpan spaFallbackPath = {};

    /// @brief Enables Last-Modified / ETag validators (default: true).
    bool enableValidators = true;

    /// @brief Enables byte range requests once supported (default: true).
    bool enableRangeRequests = true;
};

/// @brief Http file server statically serves files from a directory
///
/// This class registers the onRequest callback provided by HttpAsyncServer to serves files from a given directory.
///
/// Example using compile time set buffers for connections:
/// \snippet Tests/Libraries/Http/HttpAsyncFileServerTest.cpp HttpFileServerSnippet
///
/// Example using dynamically allocated buffers for connections:
/// \snippet Examples/SCExample/Examples/WebServerExample/WebServerExample.cpp WebServerExampleSnippet
struct SC_HTTP_EXPORT HttpAsyncFileServer
{
    using ReadableFileStream = AsyncReadableFileStream<AsyncEventLoop>;
    using WritableFileStream = AsyncWritableFileStream<AsyncEventLoop>;
    /// @brief Support class for HttpAsyncFileServer holding file stream and pipeline
    struct SC_HTTP_EXPORT Stream
    {
        ReadableFileStream  readableFileStream;
        WritableFileStream  writableFileStream;
        AsyncTaskSequence   readableFileStreamTask;
        AsyncTaskSequence   writableFileStreamTask;
        HttpMultipartParser multipartParser;
        AsyncFileSend       asyncFileSend;
        FileDescriptor      sourceFileDescriptor;
        size_t              fileSendOffset = 0;
        size_t              fileSendLength = 0;

        struct MultipartListener
        {
            HttpAsyncFileServer* server     = nullptr;
            Stream*              stream     = nullptr;
            HttpConnection*      connection = nullptr;

            FileDescriptor currentFd; // TODO: Reuse the above sourceFileDescriptor
            StringPath     currentFilePath;
            StringSpan     currentFileName;
            StringSpan     currentHeaderName;
            bool           isContentDisposition = false;
            bool           rejectedFileName     = false;

            void onData(AsyncBufferView::ID bufferID);
        } multipartListener;

        struct PutFileListener
        {
            HttpConnection* connection = nullptr;

            void onFinish();
        } putFileListener;
    };

    template <int RequestsSize>
    struct SC_HTTP_EXPORT StreamQueue : public Stream
    {
        static_assert(RequestsSize >= 2, "HttpAsyncFileServer::StreamQueue requires RequestsSize >= 2");

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

    /// @brief Controls whether to use AsyncFileSend optimization for GET requests (default: true)
    void setUseAsyncFileSend(bool value);

    /// @brief Gets whether AsyncFileSend optimization is used for GET requests
    [[nodiscard]] bool getUseAsyncFileSend() const { return useAsyncFileSend; }

    /// @brief Sets optional file server behavior.
    Result setOptions(const HttpAsyncFileServerOptions& options);

    /// @brief Handles the request, serving the requested file (GET) or creating a new one (PUT/POST)
    /// Call this method in response to HttpConnectionsPool::onRequest.
    Result handleRequest(HttpAsyncFileServer::Stream& stream, HttpConnection& connection);

  private:
    bool                       useAsyncFileSend = true;
    HttpAsyncFileServerOptions options          = {};

    Result putFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection, StringSpan filePath);
    Result getFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection, StringSpan filePath, bool sendBody);
    Result postMultipart(HttpAsyncFileServer::Stream& stream, HttpConnection& connection);

    StringPath directory;

    AsyncEventLoop* eventLoop  = nullptr;
    ThreadPool*     threadPool = nullptr;
    struct Internal;
};

} // namespace SC
