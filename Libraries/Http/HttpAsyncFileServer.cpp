// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncFileServer.h"
#include "../FileSystem/FileSystem.h"
#include "../Foundation/Assert.h"
#include "Internal/HttpStringIterator.h"

#include <stdio.h>
#include <time.h>
#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <sys/timeb.h>
#else
#include <math.h>
#endif

namespace SC
{
struct HttpAsyncFileServer::Internal
{
    static Result writeGMTHeaderTime(StringSpan headerName, HttpResponse& response, int64_t millisecondsSinceEpoch);
    static Result readFile(StringSpan initialDirectory, size_t index, StringSpan url, HttpResponse& response);
    static Result formatHttpDate(int64_t millisecondsSinceEpoch, char* buffer, size_t bufferSize, size_t& outLength);
    static Result extractSafeFilePath(StringSpan requestTarget, StringSpan& filePath);
    static bool   isSafeMultipartFileName(StringSpan fileName);
    static Result sendEmptyResponse(HttpResponse& response, int statusCode);
    static Result sendNotModified(HttpResponse& response, StringSpan lastModified);

    static int64_t    getCurrentTimeMilliseconds();
    static StringSpan getContentType(const StringSpan extension);
};

Result HttpAsyncFileServer::init(ThreadPool& pool, AsyncEventLoop& loop, StringSpan directoryToServe)
{
    SC_TRY_MSG(eventLoop == nullptr, "HttpAsyncFileServer::init - already inited")
    eventLoop  = &loop;
    threadPool = &pool;

    SC_TRY_MSG(FileSystem().existsAndIsDirectory(directoryToServe), "Invalid directory");
    SC_TRY(directory.assign(directoryToServe));
    return Result(true);
}

Result HttpAsyncFileServer::close()
{
    eventLoop  = nullptr;
    threadPool = nullptr;
    directory  = {};
    return Result(true);
}

void HttpAsyncFileServer::setUseAsyncFileSend(bool value) { useAsyncFileSend = value; }

Result HttpAsyncFileServer::handleRequest(HttpAsyncFileServer::Stream& stream, HttpConnection& connection)
{
    StringSpan filePath;
    const Result safePath = Internal::extractSafeFilePath(connection.request.getURL(), filePath);
    if (not safePath)
    {
        return Internal::sendEmptyResponse(connection.response, 400);
    }

    if (connection.request.isMultipart())
    {
        return postMultipart(stream, connection);
    }

    FileSystem fileSystem;
    SC_TRY(fileSystem.init(directory.view()));
    switch (connection.request.getParser().method)
    {
    case HttpParser::Method::HttpPOST:
    case HttpParser::Method::HttpPUT: return putFile(stream, connection, filePath);
    case HttpParser::Method::HttpGET: return getFile(stream, connection, filePath, true);
    case HttpParser::Method::HttpHEAD: return getFile(stream, connection, filePath, false);
    case HttpParser::Method::HttpOPTIONS:
        SC_TRY(connection.response.startResponse(204));
        SC_TRY(connection.response.addHeader("Allow", "GET, HEAD, PUT, POST, OPTIONS"));
        SC_TRY(connection.response.addHeader("Server", "SC"));
        SC_TRY(connection.response.addHeader("Content-Length", "0"));
        SC_TRY(connection.response.sendHeaders());
        SC_TRY(connection.response.end());
        break;
    default: {
        SC_TRY(connection.response.startResponse(405));
        SC_TRY(connection.response.addHeader("Allow", "GET, HEAD, PUT, POST, OPTIONS"));
        SC_TRY(connection.response.addHeader("Server", "SC"));
        SC_TRY(connection.response.addHeader("Content-Length", "0"));
        SC_TRY(connection.response.sendHeaders());
        SC_TRY(connection.response.end());
    }
    break;
    }
    return Result(true);
}

Result HttpAsyncFileServer::getFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection,
                                    StringSpan filePath, bool sendBody)
{
    FileSystem fileSystem;
    SC_TRY(fileSystem.init(directory.view()));
    if (fileSystem.existsAndIsFile(filePath))
    {
        FileSystem::FileStat fileStat;
        SC_TRY(fileSystem.getFileStat(filePath, fileStat));
        StringSpan name, extension;
        SC_TRY(HttpStringIterator::parseNameExtension(filePath, name, extension));
        StringPath path;
        SC_TRY(path.assign(directory.view()));
        SC_TRY(path.append("/"));
        SC_TRY(path.append(filePath));

        char      lastModifiedData[128];
        size_t    lastModifiedLength = 0;
        SC_TRY(Internal::formatHttpDate(fileStat.modifiedTime.milliseconds, lastModifiedData,
                                        sizeof(lastModifiedData), lastModifiedLength));
        StringSpan lastModified = {{lastModifiedData, lastModifiedLength}, false, StringEncoding::Ascii};

        StringSpan ifModifiedSince;
        if (connection.request.getHeader("If-Modified-Since", ifModifiedSince) and ifModifiedSince == lastModified)
        {
            return Internal::sendNotModified(connection.response, lastModified);
        }

        // Send HTTP headers first
        SC_TRY(connection.response.startResponse(200));
        char buffer[20];
        ::snprintf(buffer, sizeof(buffer), "%zu", fileStat.fileSize);
        StringSpan contentLength = {{buffer, ::strlen(buffer)}, true, StringEncoding::Ascii};
        SC_TRY(connection.response.addHeader("Content-Length", contentLength));
        SC_TRY(connection.response.addHeader("Content-Type", Internal::getContentType(extension)));
        SC_TRY(Internal::writeGMTHeaderTime("Date", connection.response, Internal::getCurrentTimeMilliseconds()));
        SC_TRY(connection.response.addHeader("Last-Modified", lastModified));
        SC_TRY(connection.response.addHeader("Server", "SC"));

        if (not sendBody)
        {
            SC_TRY(connection.response.sendHeaders());
            return connection.response.end();
        }

        if (useAsyncFileSend)
        {
            // Context for the callback
            stream.multipartListener.server     = this;
            stream.multipartListener.stream     = &stream;
            stream.multipartListener.connection = &static_cast<HttpConnection&>(connection);
            SC_TRY(stream.sourceFileDescriptor.open(path.view(), FileOpen::Read));

            auto onHeadersSent = [&stream, fileSize = fileStat.fileSize](AsyncBufferView::ID)
            {
                HttpConnection&      connection = *stream.multipartListener.connection;
                HttpAsyncFileServer* server     = stream.multipartListener.server;

                // Use AsyncFileSend for zero-copy file serving
                stream.asyncFileSend.callback = [&connection, &stream](AsyncFileSend::Result& result)
                {
                    if (not result.isValid())
                    {
                        SC_ASSERT_RELEASE(stream.sourceFileDescriptor.close());
                        // Error occurred during send, close the response
                        (void)connection.response.end();
                        return;
                    }

                    // Check if the entire file was sent
                    if (result.isComplete())
                    {
                        SC_ASSERT_RELEASE(stream.sourceFileDescriptor.close());
                        // File send complete, close the response
                        (void)connection.response.end();
                    }
                    else
                    {
                        // Partial send, reactivate to continue
                        result.reactivateRequest(true);
                    }
                };

                Result res = stream.asyncFileSend.start(*server->eventLoop, stream.sourceFileDescriptor,
                                                        connection.socket, 0, fileSize);
                if (not res)
                {
                    // Failed to start sending file
                    SC_ASSERT_RELEASE(stream.sourceFileDescriptor.close());
                    (void)connection.response.end();
                }
            };

            SC_TRY(connection.response.sendHeaders(onHeadersSent));
        }
        else
        {
            // Use legacy AsyncStreams approach
            FileDescriptor fd;
            SC_TRY(fd.open(path.view(), FileOpen::Read));
            Result initRes = stream.readableFileStream.init(connection.buffersPool, *eventLoop, fd);
            SC_ASSERT_RELEASE(initRes);
            SC_TRY(initRes);
            SC_TRY(stream.readableFileStream.request.executeOn(stream.readableFileStreamTask, *threadPool));
            fd.detach();
            stream.readableFileStream.setAutoCloseDescriptor(true);
            connection.pipeline.source   = &stream.readableFileStream;
            connection.pipeline.sinks[0] = &connection.response.getWritableStream();
            SC_TRY(connection.response.sendHeaders());
            SC_TRY(connection.pipeline.pipe());
            SC_TRY(connection.pipeline.start());
        }
    }
    else
    {
        return Internal::sendEmptyResponse(connection.response, 404);
    }
    return Result(true);
}

Result HttpAsyncFileServer::putFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection,
                                    StringSpan filePath)
{
    StringPath path;
    SC_TRY(path.assign(directory.view()));
    SC_TRY(path.append("/"));
    SC_TRY(path.append(filePath));
    const size_t   totalFileUploadBytes = static_cast<size_t>(connection.request.getBodyBytesRemaining());
    FileDescriptor fd;
    SC_TRY(fd.open(path.view(), FileOpen::Write));
    if (totalFileUploadBytes == 0)
    {
        SC_TRY(fd.close());
        return connection.response.sendEmpty(201);
    }

    SC_TRY(stream.writableFileStream.init(connection.buffersPool, *eventLoop, fd));
    SC_TRY(stream.writableFileStream.request.executeOn(stream.writableFileStreamTask, *threadPool));
    fd.detach();
    stream.writableFileStream.setAutoCloseDescriptor(true);

    HttpConnection& asyncConnection = static_cast<HttpConnection&>(connection);

    stream.putFileListener.connection     = &asyncConnection;
    stream.putFileListener.remainingBytes = totalFileUploadBytes;

    connection.pipeline.source   = &connection.request.getReadableStream();
    connection.pipeline.sinks[0] = &stream.writableFileStream;
    SC_TRY(connection.pipeline.pipe());
    // Add this listener after the pipeline one, so the body has already been dispatched to the writable stream.
    const bool addedBodyListener =
        connection.request.getReadableStream()
            .eventData.addListener<HttpAsyncFileServer::Stream::PutFileListener,
                                   &HttpAsyncFileServer::Stream::PutFileListener::onData>(stream.putFileListener);
    SC_ASSERT_RELEASE(addedBodyListener);
    SC_TRY(connection.pipeline.start());
    return Result(true);
}

void HttpAsyncFileServer::Stream::PutFileListener::onData(AsyncBufferView::ID bufferID)
{
    SC_ASSERT_RELEASE(connection != nullptr);
    AsyncReadableStream& readable = *connection->pipeline.source;
    AsyncWritableStream& writable = *connection->pipeline.sinks[0];
    AsyncBuffersPool&    buffers  = readable.getBuffersPool();

    Span<const char> data;
    SC_ASSERT_RELEASE(buffers.getReadableData(bufferID, data));
    if (remainingBytes == data.sizeInBytes())
    {
        // Last chunk, we can remove this listener and terminate the writable stream
        const bool removedBodyListener =
            readable.eventData.removeListener<HttpAsyncFileServer::Stream::PutFileListener,
                                              &HttpAsyncFileServer::Stream::PutFileListener::onData>(*this);
        SC_ASSERT_RELEASE(removedBodyListener);
        const bool addedDrainListener =
            writable.eventDrain.addListener<HttpAsyncFileServer::Stream::PutFileListener,
                                            &HttpAsyncFileServer::Stream::PutFileListener::onDrain>(*this);
        SC_ASSERT_RELEASE(addedDrainListener);
        if (not writable.isStillWriting())
        {
            onDrain();
        }
    }
    else if (remainingBytes > data.sizeInBytes())
    {
        // Intermediate chunk, we need to continue streaming data
        remainingBytes -= data.sizeInBytes();
    }
    else if (remainingBytes < data.sizeInBytes())
    {
        // HTTP Pipelining: excess data belongs to next request
        // Create a child view for the excess bytes and unshift it back into the stream
        const size_t excessOffset = remainingBytes;
        const size_t excessLength = data.sizeInBytes() - remainingBytes;

        AsyncBufferView::ID childID;
        SC_ASSERT_RELEASE(buffers.createChildView(bufferID, excessOffset, excessLength, childID));
        SC_ASSERT_RELEASE(readable.unshift(childID));
        buffers.unrefBuffer(childID);

        // Last chunk, we can remove this listener and terminate the writable stream
        const bool removedBodyListener =
            readable.eventData.removeListener<HttpAsyncFileServer::Stream::PutFileListener,
                                              &HttpAsyncFileServer::Stream::PutFileListener::onData>(*this);
        SC_ASSERT_RELEASE(removedBodyListener);
        const bool addedDrainListener =
            writable.eventDrain.addListener<HttpAsyncFileServer::Stream::PutFileListener,
                                            &HttpAsyncFileServer::Stream::PutFileListener::onDrain>(*this);
        SC_ASSERT_RELEASE(addedDrainListener);
        if (not writable.isStillWriting())
        {
            onDrain();
        }
    }
}

void HttpAsyncFileServer::Stream::PutFileListener::onDrain()
{
    SC_ASSERT_RELEASE(connection != nullptr);
    AsyncWritableStream& writable = *connection->pipeline.sinks[0];
    const bool           removedDrainListener =
        writable.eventDrain.removeListener<HttpAsyncFileServer::Stream::PutFileListener,
                                           &HttpAsyncFileServer::Stream::PutFileListener::onDrain>(*this);
    SC_ASSERT_RELEASE(removedDrainListener);
    writable.destroy();
    SC_ASSERT_RELEASE(connection->response.sendEmpty(201));
}

StringSpan HttpAsyncFileServer::Internal::getContentType(const StringSpan extension)
{
    if (extension == "htm" or extension == "html")
    {
        return "text/html";
    }
    if (extension == "woff")
    {
        return "font/woff";
    }
    if (extension == "woff2")
    {
        return "font/woff2";
    }
    if (extension == "css")
    {
        return "text/css";
    }
    if (extension == "png")
    {
        return "image/png";
    }
    if (extension == "webp")
    {
        return "image/webp";
    }
    if (extension == "gif")
    {
        return "image/gif";
    }
    if (extension == "jpeg" or extension == "jpg")
    {
        return "image/jpeg";
    }
    if (extension == "svg")
    {
        return "image/svg+xml";
    }
    if (extension == "js" or extension == "mjs")
    {
        return "application/javascript";
    }
    if (extension == "json")
    {
        return "application/json";
    }
    if (extension == "xml")
    {
        return "application/xml";
    }
    if (extension == "pdf")
    {
        return "application/pdf";
    }
    if (extension == "ico")
    {
        return "image/x-icon";
    }
    if (extension == "txt")
    {
        return "text/plain";
    }
    if (extension == "wasm")
    {
        return "application/wasm";
    }
    if (extension == "map")
    {
        return "application/json";
    }
    return "text/html";
}

Result HttpAsyncFileServer::Internal::extractSafeFilePath(StringSpan requestTarget, StringSpan& filePath)
{
    SC_TRY_MSG(HttpStringIterator::startsWith(requestTarget, "/"), "HttpAsyncFileServer request target must be path");

    const char* data       = requestTarget.bytesWithoutTerminator();
    size_t      pathLength = requestTarget.sizeInBytes();
    for (size_t idx = 0; idx < requestTarget.sizeInBytes(); ++idx)
    {
        if (data[idx] == '?' or data[idx] == '#')
        {
            pathLength = idx;
            break;
        }
    }

    StringSpan path = {{data, pathLength}, false, requestTarget.getEncoding()};
    filePath        = HttpStringIterator::sliceStart(path, 1);
    if (filePath.isEmpty())
    {
        filePath = "index.html";
        return Result(true);
    }

    const char* fileData     = filePath.bytesWithoutTerminator();
    size_t      segmentStart = 0;
    for (size_t idx = 0; idx <= filePath.sizeInBytes(); ++idx)
    {
        const bool atEnd     = idx == filePath.sizeInBytes();
        const char current   = atEnd ? '/' : fileData[idx];
        const bool separator = current == '/';
        SC_TRY_MSG(current != '\\' and current != ':', "HttpAsyncFileServer invalid path character");
        if (separator)
        {
            const size_t segmentLength = idx - segmentStart;
            if (segmentLength == 1)
            {
                SC_TRY_MSG(fileData[segmentStart] != '.', "HttpAsyncFileServer dot path segment rejected");
            }
            else if (segmentLength == 2)
            {
                SC_TRY_MSG(fileData[segmentStart] != '.' or fileData[segmentStart + 1] != '.',
                           "HttpAsyncFileServer parent path segment rejected");
            }
            segmentStart = idx + 1;
        }
    }
    return Result(true);
}

bool HttpAsyncFileServer::Internal::isSafeMultipartFileName(StringSpan fileName)
{
    if (fileName.isEmpty() or fileName == "." or fileName == "..")
    {
        return false;
    }

    const char* data = fileName.bytesWithoutTerminator();
    for (size_t idx = 0; idx < fileName.sizeInBytes(); ++idx)
    {
        if (data[idx] == '/' or data[idx] == '\\' or data[idx] == ':')
        {
            return false;
        }
    }
    return true;
}

Result HttpAsyncFileServer::Internal::sendEmptyResponse(HttpResponse& response, int statusCode)
{
    SC_TRY(response.startResponse(statusCode));
    SC_TRY(response.addHeader("Content-Length", "0"));
    SC_TRY(response.addHeader("Server", "SC"));
    SC_TRY(response.sendHeaders());
    return response.end();
}

Result HttpAsyncFileServer::Internal::sendNotModified(HttpResponse& response, StringSpan lastModified)
{
    SC_TRY(response.startResponse(304));
    SC_TRY(writeGMTHeaderTime("Date", response, getCurrentTimeMilliseconds()));
    SC_TRY(response.addHeader("Last-Modified", lastModified));
    SC_TRY(response.addHeader("Server", "SC"));
    SC_TRY(response.sendHeaders());
    return response.end();
}

int64_t HttpAsyncFileServer::Internal::getCurrentTimeMilliseconds()
{
#if SC_PLATFORM_WINDOWS
    struct _timeb t;
    _ftime_s(&t);
    return static_cast<int64_t>(t.time) * 1000 + t.millitm;
#else
    struct timespec nowTimeSpec;
    clock_gettime(CLOCK_REALTIME, &nowTimeSpec);
    return static_cast<int64_t>(round(nowTimeSpec.tv_nsec / 1.0e6) + nowTimeSpec.tv_sec * 1000);
#endif
}

Result HttpAsyncFileServer::Internal::formatHttpDate(int64_t millisecondsSinceEpoch, char* buffer, size_t bufferSize,
                                                     size_t& outLength)
{
    const time_t seconds = static_cast<time_t>(millisecondsSinceEpoch / 1000);
    struct tm    parsedTm;
#if SC_PLATFORM_WINDOWS
    if (_gmtime64_s(&parsedTm, &seconds) != 0)
    {
        return Result::Error("Failed to convert time");
    }
#else
    if (gmtime_r(&seconds, &parsedTm) == nullptr)
    {
        return Result::Error("Failed to convert time");
    }
#endif

    // Format as HTTP date: "Wed, 21 Oct 2015 07:28:00 GMT"
    const char* days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    outLength = static_cast<size_t>(
        snprintf(buffer, bufferSize, "%s, %02d %s %04d %02d:%02d:%02d GMT", days[parsedTm.tm_wday], parsedTm.tm_mday,
                 months[parsedTm.tm_mon], parsedTm.tm_year + 1900, parsedTm.tm_hour, parsedTm.tm_min, parsedTm.tm_sec));

    if (outLength == 0 || outLength >= bufferSize)
    {
        return Result::Error("Failed to format time");
    }

    return Result(true);
}

Result HttpAsyncFileServer::Internal::writeGMTHeaderTime(StringSpan headerName, HttpResponse& response,
                                                         int64_t millisecondsSinceEpoch)
{
    char   bufferData[128];
    size_t len;
    SC_TRY(formatHttpDate(millisecondsSinceEpoch, bufferData, sizeof(bufferData), len));

    SC_TRY(response.addHeader(headerName, {{bufferData, len}, true, StringEncoding::Ascii}));
    return Result(true);
}

Result HttpAsyncFileServer::postMultipart(HttpAsyncFileServer::Stream& stream, HttpConnection& connection)
{
    SC_TRY(stream.multipartParser.initWithBoundary(connection.request.getBoundary()));

    stream.multipartListener.server     = this;
    stream.multipartListener.stream     = &stream;
    stream.multipartListener.connection = &connection;
    stream.multipartListener.currentFileName      = {};
    stream.multipartListener.currentHeaderName    = {};
    stream.multipartListener.isContentDisposition = false;
    stream.multipartListener.rejectedFileName     = false;

    SC_ASSERT_RELEASE((
        connection.request.getReadableStream()
            .eventData.addListener<HttpAsyncFileServer::Stream::MultipartListener,
                                   &HttpAsyncFileServer::Stream::MultipartListener::onData>(stream.multipartListener)));

    return Result(true);
}

void HttpAsyncFileServer::Stream::MultipartListener::onData(AsyncBufferView::ID bufferID)
{
    AsyncReadableStream& readable = connection->request.getReadableStream();
    AsyncBuffersPool&    buffers  = readable.getBuffersPool();

    Span<const char> data;
    SC_ASSERT_RELEASE(buffers.getReadableData(bufferID, data));

    size_t           readBytes;
    Span<const char> parsedData;

    while (not data.empty() and stream->multipartParser.state != HttpMultipartParser::State::Finished)
    {
        SC_ASSERT_RELEASE(stream->multipartParser.parse(data, readBytes, parsedData));

        const bool streamedBody =
            stream->multipartParser.token == HttpMultipartParser::Token::PartBody and parsedData.sizeInBytes() > 0;
        if (streamedBody and currentFd.isValid())
        {
            SC_ASSERT_RELEASE(currentFd.write(parsedData));
        }

        bool tokenProcessed = false;
        if (stream->multipartParser.state != HttpMultipartParser::State::Parsing)
        {
            tokenProcessed = true;
            switch (stream->multipartParser.token)
            {
            case HttpMultipartParser::Token::Boundary: {
                if (currentFd.isValid())
                {
                    (void)currentFd.close();
                }
                currentFileName      = {};
                currentHeaderName    = {};
                isContentDisposition = false;
            }
            break;

            case HttpMultipartParser::Token::HeaderName: {
                currentHeaderName    = {parsedData, false, StringEncoding::Ascii};
                isContentDisposition = HttpStringIterator::equalsIgnoreCase(currentHeaderName, "Content-Disposition");
            }
            break;

            case HttpMultipartParser::Token::HeaderValue: {
                if (isContentDisposition)
                {
                    HttpStringIterator it = {{parsedData, false, StringEncoding::Ascii}};
                    if (it.advanceUntilMatchesIgnoreCase("filename="))
                    {
                        for (int i = 0; i < 9; ++i)
                            (void)it.stepForward();
                        if (it.advanceIfMatches('"'))
                        {
                            auto start = it;
                            while (!it.isAtEnd() && !it.match('"'))
                                (void)it.stepForward();
                            currentFileName = HttpStringIterator::fromIterators(start, it, StringEncoding::Ascii);
                            if (not Internal::isSafeMultipartFileName(currentFileName))
                            {
                                rejectedFileName = true;
                                currentFileName  = {};
                            }
                        }
                    }
                }
            }
            break;

            case HttpMultipartParser::Token::PartHeaderEnd: {
                if (!currentFileName.isEmpty())
                {
                    (void)currentFilePath.assign(server->directory.view());
                    (void)currentFilePath.append("/");
                    (void)currentFilePath.append(currentFileName);
                    (void)currentFd.open(currentFilePath.view(), FileOpen::Write);
                }
            }
            break;

            case HttpMultipartParser::Token::PartBody: {
                if (currentFd.isValid() and not streamedBody)
                {
                    SC_ASSERT_RELEASE(currentFd.write(parsedData));
                }
            }
            break;

            case HttpMultipartParser::Token::Finished: {
                if (currentFd.isValid())
                {
                    (void)currentFd.close();
                }
                SC_ASSERT_RELEASE((
                    readable.eventData.removeListener<HttpAsyncFileServer::Stream::MultipartListener,
                                                      &HttpAsyncFileServer::Stream::MultipartListener::onData>(*this)));

                SC_ASSERT_RELEASE(connection->response.sendEmpty(rejectedFileName ? 400 : 201));
            }
            break;
            default: break;
            }
        }
        if (readBytes > 0)
        {
            SC_ASSERT_RELEASE(data.sliceStart(readBytes, data));
        }
        else if (not tokenProcessed)
        {
            break; // Stall
        }

        if (stream->multipartParser.state == HttpMultipartParser::State::Finished)
            break;
    }
}

} // namespace SC
