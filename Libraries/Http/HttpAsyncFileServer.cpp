// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncFileServer.h"
#include "../FileSystem/FileSystem.h"
#include "../Foundation/Assert.h"
#include "HttpURLParser.h"
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
    struct ByteRange
    {
        size_t offset  = 0;
        size_t length  = 0;
        bool   partial = false;
    };

    static Result writeGMTHeaderTime(StringSpan headerName, HttpResponse& response, int64_t millisecondsSinceEpoch);
    static Result readFile(StringSpan initialDirectory, size_t index, StringSpan url, HttpResponse& response);
    static Result formatHttpDate(int64_t millisecondsSinceEpoch, char* buffer, size_t bufferSize, size_t& outLength);
    static Result formatWeakETag(const FileSystem::FileStat& fileStat, char* buffer, size_t bufferSize,
                                 size_t& outLength);
    static Result formatContentRange(const ByteRange& range, size_t fileSize, char* buffer, size_t bufferSize,
                                     size_t& outLength);
    static Result formatUnsatisfiedContentRange(size_t fileSize, char* buffer, size_t bufferSize, size_t& outLength);
    static Result extractSafeFilePath(StringSpan requestTarget, StringSpan& filePath);
    static Result normalizeOptionFilePath(StringSpan filePath, StringSpan& normalizedPath);
    static Result validateSafeRelativePath(StringSpan filePath);
    static bool   parseDecimalSize(StringSpan text, size_t& value);
    static bool   parseSingleByteRange(StringSpan header, size_t fileSize, ByteRange& range);
    static bool   etagMatchesIfNoneMatch(StringSpan ifNoneMatch, StringSpan etag);
    static bool   ifRangeMatches(StringSpan ifRange, StringSpan etag, StringSpan lastModified);
    static Result sendEmptyResponse(HttpResponse& response, int statusCode);
    static Result sendNotModified(HttpResponse& response, StringSpan lastModified, StringSpan etag);
    static Result sendRangeNotSatisfiable(HttpResponse& response, size_t fileSize);

    static int64_t    getCurrentTimeMilliseconds();
    static StringSpan getContentType(const HttpAsyncFileServerOptions& options, const StringSpan extension);
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

Result HttpAsyncFileServer::setOptions(const HttpAsyncFileServerOptions& newOptions)
{
    StringSpan normalizedFallback;
    SC_TRY(Internal::normalizeOptionFilePath(newOptions.spaFallbackPath, normalizedFallback));
    SC_COMPILER_UNUSED(normalizedFallback);
    options = newOptions;
    return Result(true);
}

Result HttpAsyncFileServer::handleRequest(HttpAsyncFileServer::Stream& stream, HttpConnection& connection)
{
    StringSpan   filePath;
    const Result safePath = Internal::extractSafeFilePath(connection.request.getRequestTarget(), filePath);
    if (not safePath)
    {
        return Internal::sendEmptyResponse(connection.response, 400);
    }

    const bool uploadRequest = connection.request.isMultipart() or
                               connection.request.getParser().method == HttpParser::Method::HttpPOST or
                               connection.request.getParser().method == HttpParser::Method::HttpPUT;
    if (uploadRequest)
    {
        if (not options.enableUploads)
        {
            return Internal::sendEmptyResponse(connection.response, 403);
        }
        if (options.maxUploadBytes > 0 and
            connection.request.getBodyFramingKind() == HttpBodyFramingKind::ContentLength and
            connection.request.getBodyBytesRemaining() > options.maxUploadBytes)
        {
            return Internal::sendEmptyResponse(connection.response, 413);
        }
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
    case HttpParser::Method::HttpGET: return getFile(stream, connection, filePath, true, true);
    case HttpParser::Method::HttpHEAD: return getFile(stream, connection, filePath, false, true);
    case HttpParser::Method::HttpOPTIONS:
        SC_TRY(connection.response.startResponse(204));
        SC_TRY(connection.response.addHeader("Allow", "GET, HEAD, PUT, POST, OPTIONS"));
        SC_TRY(connection.response.addHeader("Server", "SC"));
        SC_TRY(connection.response.addContentLength(0));
        SC_TRY(connection.response.sendHeaders());
        SC_TRY(connection.response.end());
        break;
    default: {
        SC_TRY(connection.response.startResponse(405));
        SC_TRY(connection.response.addHeader("Allow", "GET, HEAD, PUT, POST, OPTIONS"));
        SC_TRY(connection.response.addHeader("Server", "SC"));
        SC_TRY(connection.response.addContentLength(0));
        SC_TRY(connection.response.sendHeaders());
        SC_TRY(connection.response.end());
    }
    break;
    }
    return Result(true);
}

Result HttpAsyncFileServer::getFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection,
                                    StringSpan filePath, bool sendBody, bool allowSpaFallback)
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

        char   lastModifiedData[128];
        size_t lastModifiedLength = 0;
        SC_TRY(Internal::formatHttpDate(fileStat.modifiedTime.milliseconds, lastModifiedData, sizeof(lastModifiedData),
                                        lastModifiedLength));
        StringSpan lastModified = {{lastModifiedData, lastModifiedLength}, false, StringEncoding::Ascii};

        char       etagData[64];
        size_t     etagLength = 0;
        StringSpan etag;
        if (options.enableValidators)
        {
            SC_TRY(Internal::formatWeakETag(fileStat, etagData, sizeof(etagData), etagLength));
            etag = {{etagData, etagLength}, false, StringEncoding::Ascii};
        }

        if (options.enableValidators)
        {
            StringSpan ifNoneMatch;
            if (connection.request.getHeader("If-None-Match", ifNoneMatch))
            {
                if (Internal::etagMatchesIfNoneMatch(ifNoneMatch, etag))
                {
                    return Internal::sendNotModified(connection.response, lastModified, etag);
                }
            }
            else
            {
                StringSpan ifModifiedSince;
                if (connection.request.getHeader("If-Modified-Since", ifModifiedSince) and
                    ifModifiedSince == lastModified)
                {
                    return Internal::sendNotModified(connection.response, lastModified, etag);
                }
            }
        }

        Internal::ByteRange byteRange;
        byteRange.length = fileStat.fileSize;
        if (options.enableRangeRequests)
        {
            StringSpan rangeHeader;
            if (connection.request.getHeader("Range", rangeHeader))
            {
                bool rangeAllowed = true;
                if (options.enableValidators)
                {
                    StringSpan ifRange;
                    if (connection.request.getHeader("If-Range", ifRange))
                    {
                        rangeAllowed = Internal::ifRangeMatches(ifRange, etag, lastModified);
                    }
                }
                if (rangeAllowed and not Internal::parseSingleByteRange(rangeHeader, fileStat.fileSize, byteRange))
                {
                    return Internal::sendRangeNotSatisfiable(connection.response, fileStat.fileSize);
                }
            }
        }

        char       contentRangeData[64];
        size_t     contentRangeLength = 0;
        StringSpan contentRange;
        if (byteRange.partial)
        {
            SC_TRY(Internal::formatContentRange(byteRange, fileStat.fileSize, contentRangeData,
                                                sizeof(contentRangeData), contentRangeLength));
            contentRange = {{contentRangeData, contentRangeLength}, false, StringEncoding::Ascii};
        }

        // Send HTTP headers first
        SC_TRY(connection.response.startResponse(byteRange.partial ? 206 : 200));
        SC_TRY(connection.response.addContentLength(byteRange.length));
        SC_TRY(connection.response.addHeader("Content-Type", Internal::getContentType(options, extension)));
        SC_TRY(Internal::writeGMTHeaderTime("Date", connection.response, Internal::getCurrentTimeMilliseconds()));
        if (options.enableValidators)
        {
            SC_TRY(connection.response.addHeader("Last-Modified", lastModified));
            SC_TRY(connection.response.addHeader("ETag", etag));
        }
        if (options.enableRangeRequests)
        {
            SC_TRY(connection.response.addHeader("Accept-Ranges", "bytes"));
        }
        if (byteRange.partial)
        {
            SC_TRY(connection.response.addHeader("Content-Range", contentRange));
        }
        SC_TRY(connection.response.addHeader("Server", "SC"));

        if (not sendBody)
        {
            SC_TRY(connection.response.sendHeaders());
            return connection.response.end();
        }

        if (useAsyncFileSend or byteRange.partial)
        {
            // Context for the callback
            stream.multipartListener.server     = this;
            stream.multipartListener.stream     = &stream;
            stream.multipartListener.connection = &static_cast<HttpConnection&>(connection);
            stream.fileSendOffset               = byteRange.offset;
            stream.fileSendLength               = byteRange.length;
            SC_TRY(stream.sourceFileDescriptor.open(path.view(), FileOpen::Read));

            auto onHeadersSent = [&stream](AsyncBufferView::ID)
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

                Result res =
                    stream.asyncFileSend.start(*server->eventLoop, stream.sourceFileDescriptor, connection.socket,
                                               static_cast<int64_t>(stream.fileSendOffset), stream.fileSendLength);
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
        StringSpan fallbackPath;
        SC_TRY(Internal::normalizeOptionFilePath(options.spaFallbackPath, fallbackPath));
        if (allowSpaFallback and not fallbackPath.isEmpty() and fallbackPath != filePath)
        {
            return getFile(stream, connection, fallbackPath, sendBody, false);
        }
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

    stream.putFileListener.connection = &asyncConnection;
    const bool addedFinishListener =
        stream.writableFileStream.eventFinish.addListener<HttpAsyncFileServer::Stream::PutFileListener,
                                                          &HttpAsyncFileServer::Stream::PutFileListener::onFinish>(
            stream.putFileListener);
    SC_ASSERT_RELEASE(addedFinishListener);

    connection.pipeline.source   = &connection.request.getReadableStream();
    connection.pipeline.sinks[0] = &stream.writableFileStream;
    SC_TRY(connection.pipeline.pipe());
    SC_TRY(connection.pipeline.start());
    return Result(true);
}

void HttpAsyncFileServer::Stream::PutFileListener::onFinish()
{
    SC_ASSERT_RELEASE(connection != nullptr);
    AsyncWritableStream& writable = *connection->pipeline.sinks[0];
    const bool           removedFinishListener =
        writable.eventFinish.removeListener<HttpAsyncFileServer::Stream::PutFileListener,
                                            &HttpAsyncFileServer::Stream::PutFileListener::onFinish>(*this);
    SC_ASSERT_RELEASE(removedFinishListener);
    SC_ASSERT_RELEASE(connection->response.sendEmpty(201));
}

StringSpan HttpAsyncFileServer::Internal::getContentType(const HttpAsyncFileServerOptions& options,
                                                         const StringSpan                  extension)
{
    if (options.mimeTypeLookup != nullptr)
    {
        const StringSpan custom = options.mimeTypeLookup(extension, options.mimeTypeUserData);
        if (not custom.isEmpty())
        {
            return custom;
        }
    }

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
    if (extension == "zip")
    {
        return "application/zip";
    }
    if (extension == "gz")
    {
        return "application/gzip";
    }
    if (extension == "tar")
    {
        return "application/x-tar";
    }
    if (extension == "dmg")
    {
        return "application/x-apple-diskimage";
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
    return "application/octet-stream";
}

Result HttpAsyncFileServer::Internal::extractSafeFilePath(StringSpan requestTarget, StringSpan& filePath)
{
    HttpRequestTargetView target;
    SC_TRY(target.parse(requestTarget));
    SC_TRY_MSG(HttpStringIterator::startsWith(target.path, "/"), "HttpAsyncFileServer request target must be path");

    filePath = HttpStringIterator::sliceStart(target.path, 1);
    if (filePath.isEmpty())
    {
        filePath = "index.html";
        return Result(true);
    }

    return validateSafeRelativePath(filePath);
}

Result HttpAsyncFileServer::Internal::normalizeOptionFilePath(StringSpan filePath, StringSpan& normalizedPath)
{
    normalizedPath = {};
    if (filePath.isEmpty())
    {
        return Result(true);
    }

    const char* data   = filePath.bytesWithoutTerminator();
    size_t      length = filePath.sizeInBytes();
    if (length > 0 and data[0] == '/')
    {
        data += 1;
        length -= 1;
    }
    normalizedPath = {{data, length}, false, filePath.getEncoding()};
    return validateSafeRelativePath(normalizedPath);
}

Result HttpAsyncFileServer::Internal::validateSafeRelativePath(StringSpan filePath)
{
    SC_TRY_MSG(not filePath.isEmpty(), "HttpAsyncFileServer empty file path rejected");

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

bool HttpAsyncFileServer::Internal::parseDecimalSize(StringSpan text, size_t& value)
{
    if (text.isEmpty())
    {
        return false;
    }

    const char*  data   = text.bytesWithoutTerminator();
    const size_t length = text.sizeInBytes();
    size_t       parsed = 0;
    for (size_t idx = 0; idx < length; ++idx)
    {
        if (data[idx] < '0' or data[idx] > '9')
        {
            return false;
        }
        const size_t digit = static_cast<size_t>(data[idx] - '0');
        if (parsed > (static_cast<size_t>(-1) - digit) / 10)
        {
            return false;
        }
        parsed = parsed * 10 + digit;
    }
    value = parsed;
    return true;
}

bool HttpAsyncFileServer::Internal::parseSingleByteRange(StringSpan header, size_t fileSize, ByteRange& range)
{
    static constexpr StringSpan Prefix = "bytes=";

    range.offset  = 0;
    range.length  = fileSize;
    range.partial = false;

    const char*  data   = header.bytesWithoutTerminator();
    const size_t length = header.sizeInBytes();
    if (length <= Prefix.sizeInBytes() or fileSize == 0)
    {
        return false;
    }
    for (size_t idx = 0; idx < Prefix.sizeInBytes(); ++idx)
    {
        if (data[idx] != Prefix.bytesWithoutTerminator()[idx])
        {
            return false;
        }
    }

    size_t dashIndex = static_cast<size_t>(-1);
    for (size_t idx = Prefix.sizeInBytes(); idx < length; ++idx)
    {
        if (data[idx] == ',')
        {
            return false;
        }
        if (data[idx] == '-')
        {
            if (dashIndex != static_cast<size_t>(-1))
            {
                return false;
            }
            dashIndex = idx;
        }
    }
    if (dashIndex == static_cast<size_t>(-1))
    {
        return false;
    }

    const size_t startBegin  = Prefix.sizeInBytes();
    const size_t startLength = dashIndex - startBegin;
    const size_t endBegin    = dashIndex + 1;
    const size_t endLength   = length - endBegin;

    if (startLength == 0 and endLength == 0)
    {
        return false;
    }

    size_t start = 0;
    size_t end   = fileSize - 1;
    if (startLength == 0)
    {
        size_t suffixLength = 0;
        if (not parseDecimalSize({{data + endBegin, endLength}, false, header.getEncoding()}, suffixLength) or
            suffixLength == 0)
        {
            return false;
        }
        if (suffixLength >= fileSize)
        {
            start = 0;
        }
        else
        {
            start = fileSize - suffixLength;
        }
    }
    else
    {
        if (not parseDecimalSize({{data + startBegin, startLength}, false, header.getEncoding()}, start))
        {
            return false;
        }
        if (start >= fileSize)
        {
            return false;
        }
        if (endLength > 0)
        {
            if (not parseDecimalSize({{data + endBegin, endLength}, false, header.getEncoding()}, end))
            {
                return false;
            }
            if (end < start)
            {
                return false;
            }
            if (end >= fileSize)
            {
                end = fileSize - 1;
            }
        }
    }

    range.offset  = start;
    range.length  = end - start + 1;
    range.partial = true;
    return true;
}

bool HttpAsyncFileServer::Internal::etagMatchesIfNoneMatch(StringSpan ifNoneMatch, StringSpan etag)
{
    const char*  data   = ifNoneMatch.bytesWithoutTerminator();
    const size_t length = ifNoneMatch.sizeInBytes();
    size_t       cursor = 0;

    while (cursor <= length)
    {
        size_t tokenEnd = cursor;
        while (tokenEnd < length and data[tokenEnd] != ',')
        {
            tokenEnd++;
        }

        size_t start = cursor;
        size_t end   = tokenEnd;
        while (start < end and (data[start] == ' ' or data[start] == '\t'))
        {
            start++;
        }
        while (end > start and (data[end - 1] == ' ' or data[end - 1] == '\t'))
        {
            end--;
        }

        const size_t tokenLength = end - start;
        if (tokenLength == 1 and data[start] == '*')
        {
            return true;
        }
        if (tokenLength > 0)
        {
            StringSpan token = {{data + start, tokenLength}, false, ifNoneMatch.getEncoding()};
            if (token == etag)
            {
                return true;
            }
        }

        if (tokenEnd == length)
        {
            break;
        }
        cursor = tokenEnd + 1;
    }

    return false;
}

bool HttpAsyncFileServer::Internal::ifRangeMatches(StringSpan ifRange, StringSpan etag, StringSpan lastModified)
{
    const char*  data   = ifRange.bytesWithoutTerminator();
    const size_t length = ifRange.sizeInBytes();

    size_t start = 0;
    size_t end   = length;
    while (start < end and (data[start] == ' ' or data[start] == '\t'))
    {
        start++;
    }
    while (end > start and (data[end - 1] == ' ' or data[end - 1] == '\t'))
    {
        end--;
    }

    if (end == start)
    {
        return false;
    }

    StringSpan value = {{data + start, end - start}, false, ifRange.getEncoding()};
    return value == etag or value == lastModified;
}

Result HttpAsyncFileServer::Internal::sendEmptyResponse(HttpResponse& response, int statusCode)
{
    SC_TRY(response.startResponse(statusCode));
    SC_TRY(response.addContentLength(0));
    SC_TRY(response.addHeader("Server", "SC"));
    SC_TRY(response.sendHeaders());
    return response.end();
}

Result HttpAsyncFileServer::Internal::sendNotModified(HttpResponse& response, StringSpan lastModified, StringSpan etag)
{
    SC_TRY(response.startResponse(304));
    SC_TRY(writeGMTHeaderTime("Date", response, getCurrentTimeMilliseconds()));
    SC_TRY(response.addHeader("Last-Modified", lastModified));
    if (not etag.isEmpty())
    {
        SC_TRY(response.addHeader("ETag", etag));
    }
    SC_TRY(response.addHeader("Server", "SC"));
    SC_TRY(response.sendHeaders());
    return response.end();
}

Result HttpAsyncFileServer::Internal::sendRangeNotSatisfiable(HttpResponse& response, size_t fileSize)
{
    char   contentRangeData[64];
    size_t contentRangeLength = 0;
    SC_TRY(formatUnsatisfiedContentRange(fileSize, contentRangeData, sizeof(contentRangeData), contentRangeLength));
    StringSpan contentRange = {{contentRangeData, contentRangeLength}, false, StringEncoding::Ascii};

    SC_TRY(response.startResponse(416));
    SC_TRY(writeGMTHeaderTime("Date", response, getCurrentTimeMilliseconds()));
    SC_TRY(response.addHeader("Content-Range", contentRange));
    SC_TRY(response.addHeader("Accept-Ranges", "bytes"));
    SC_TRY(response.addHeader("Server", "SC"));
    SC_TRY(response.addContentLength(0));
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

Result HttpAsyncFileServer::Internal::formatContentRange(const ByteRange& range, size_t fileSize, char* buffer,
                                                         size_t bufferSize, size_t& outLength)
{
    outLength = static_cast<size_t>(snprintf(
        buffer, bufferSize, "bytes %llu-%llu/%llu", static_cast<unsigned long long>(range.offset),
        static_cast<unsigned long long>(range.offset + range.length - 1), static_cast<unsigned long long>(fileSize)));
    if (outLength == 0 or outLength >= bufferSize)
    {
        return Result::Error("Failed to format Content-Range");
    }
    return Result(true);
}

Result HttpAsyncFileServer::Internal::formatUnsatisfiedContentRange(size_t fileSize, char* buffer, size_t bufferSize,
                                                                    size_t& outLength)
{
    outLength =
        static_cast<size_t>(snprintf(buffer, bufferSize, "bytes */%llu", static_cast<unsigned long long>(fileSize)));
    if (outLength == 0 or outLength >= bufferSize)
    {
        return Result::Error("Failed to format Content-Range");
    }
    return Result(true);
}

Result HttpAsyncFileServer::Internal::formatWeakETag(const FileSystem::FileStat& fileStat, char* buffer,
                                                     size_t bufferSize, size_t& outLength)
{
    outLength = static_cast<size_t>(snprintf(buffer, bufferSize, "W/\"%llu-%lld\"",
                                             static_cast<unsigned long long>(fileStat.fileSize),
                                             static_cast<long long>(fileStat.modifiedTime.milliseconds)));
    if (outLength == 0 or outLength >= bufferSize)
    {
        return Result::Error("Failed to format ETag");
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

    stream.multipartListener.server            = this;
    stream.multipartListener.stream            = &stream;
    stream.multipartListener.connection        = &connection;
    stream.multipartListener.currentHeaderName = {};
    stream.multipartListener.partHeaders.reset();
    stream.multipartListener.rejectedFileName = false;

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
                currentHeaderName = {};
                partHeaders.reset();
            }
            break;

            case HttpMultipartParser::Token::HeaderName: {
                currentHeaderName = {parsedData, false, StringEncoding::Ascii};
            }
            break;

            case HttpMultipartParser::Token::HeaderValue: {
                SC_ASSERT_RELEASE(partHeaders.addHeader(currentHeaderName, {parsedData, false, StringEncoding::Ascii}));
                if (partHeaders.hasFileName() and not partHeaders.hasSafeFileName())
                {
                    rejectedFileName = true;
                }
            }
            break;

            case HttpMultipartParser::Token::PartHeaderEnd: {
                if (partHeaders.hasSafeFileName())
                {
                    (void)currentFilePath.assign(server->directory.view());
                    (void)currentFilePath.append("/");
                    (void)currentFilePath.append(partHeaders.fileName());
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
