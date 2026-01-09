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

Result HttpAsyncFileServer::handleRequest(HttpAsyncFileServer::Stream& stream, HttpConnection& connection)
{
    auto url = connection.request.getURL();
    if (not HttpStringIterator::startsWith(url, "/"))
    {
        return Result::Error("Wrong url");
    }
    StringSpan filePath = HttpStringIterator::sliceStart(url, 1);
    if (filePath.isEmpty())
    {
        filePath = "index.html";
    }
    // TODO: Resolve and validate final path to check that is inside allowed folders for GET/PUT

    FileSystem fileSystem;
    SC_TRY(fileSystem.init(directory.view()));
    switch (connection.request.getParser().method)
    {
    case HttpParser::Method::HttpPOST:
    case HttpParser::Method::HttpPUT: return putFile(stream, connection, filePath);
    case HttpParser::Method::HttpGET: return getFile(stream, connection, filePath);
    default: {
        SC_TRY(connection.response.startResponse(405));
        SC_TRY(connection.response.addHeader("Allow", "GET, PUT, POST"));
        SC_TRY(connection.response.addHeader("Server", "SC"));
        SC_TRY(connection.response.addHeader("Connection", "close"));
        SC_TRY(connection.response.sendHeaders());
        SC_TRY(connection.response.end());
    }
    break;
    }
    return Result(true);
}

Result HttpAsyncFileServer::getFile(HttpAsyncFileServer::Stream& stream, HttpConnection& connection,
                                    StringSpan filePath)
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
        FileDescriptor fd;
        SC_TRY(fd.open(path.view(), FileOpen::Read));
        SC_TRY(stream.readableFileStream.init(connection.buffersPool, *eventLoop, fd));
        SC_TRY(stream.readableFileStream.request.executeOn(stream.readableFileStreamTask, *threadPool));
        fd.detach();
        stream.readableFileStream.setAutoCloseDescriptor(true);
        connection.pipeline.source   = &stream.readableFileStream;
        connection.pipeline.sinks[0] = &connection.response.getWritableStream();

        SC_TRY(connection.response.startResponse(200));
        char buffer[20];
        ::snprintf(buffer, sizeof(buffer), "%zu", fileStat.fileSize);
        StringSpan contentLength = {{buffer, ::strlen(buffer)}, true, StringEncoding::Ascii};
        SC_TRY(connection.response.addHeader("Content-Length", contentLength));
        SC_TRY(connection.response.addHeader("Content-Type", Internal::getContentType(extension)));
        SC_TRY(Internal::writeGMTHeaderTime("Date", connection.response, Internal::getCurrentTimeMilliseconds()));
        SC_TRY(Internal::writeGMTHeaderTime("Last-Modified", connection.response, fileStat.modifiedTime.milliseconds));
        SC_TRY(connection.response.addHeader("Server", "SC"));
        SC_TRY(connection.response.addHeader("Connection", "close"));

        SC_TRY(connection.response.sendHeaders());

        SC_TRY(connection.pipeline.pipe());
        SC_TRY(connection.pipeline.start());
    }
    else
    {
        SC_TRY(connection.response.startResponse(404));
        SC_TRY(connection.response.addHeader("Server", "SC"));
        SC_TRY(connection.response.addHeader("Connection", "close"));
        SC_TRY(connection.response.sendHeaders());
        SC_TRY(connection.response.end());
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
    FileDescriptor fd;
    SC_TRY(fd.open(path.view(), FileOpen::Write));
    SC_TRY(stream.writableFileStream.init(connection.buffersPool, *eventLoop, fd));
    fd.detach();
    stream.writableFileStream.setAutoCloseDescriptor(true);

    StringSpan partialBody = connection.request.getFirstBodySlice();
    if (not partialBody.isEmpty())
    {
        AsyncBufferView buffer = Span<const char>(partialBody.bytesWithoutTerminator(), partialBody.sizeInBytes());
        SC_TRY(stream.writableFileStream.AsyncWritableStream::write(move(buffer)));
    }

    struct OnFileWritten
    {
        HttpConnection&              client;
        HttpAsyncFileServer::Stream& stream;

        void operator()()
        {
            SC_ASSERT_RELEASE(stream.writableFileStream.eventFinish.removeListener(*this));
            (void)client.response.startResponse(201);
            (void)client.response.addHeader("Content-Length", "0");
            (void)client.response.sendHeaders();
            (void)client.response.end();
        }
    };
    SC_TRY(stream.writableFileStream.eventFinish.addListener(OnFileWritten{connection, stream}));

    HttpAsyncConnectionBase& asyncConnection = static_cast<HttpAsyncConnectionBase&>(connection);

    const size_t totalFileUploadBytes = static_cast<size_t>(connection.request.getParser().contentLength);
    if (partialBody.sizeInBytes() == totalFileUploadBytes)
    {
        // Body fully received
        stream.writableFileStream.end();
    }
    else
    {
        // Body partially received. Piping rest.
        connection.pipeline.source   = &asyncConnection.readableSocketStream;
        connection.pipeline.sinks[0] = &stream.writableFileStream;
        SC_TRY(connection.pipeline.pipe());
        SC_TRY(connection.pipeline.start());
        // This listener could have been a transform stream but it would be less efficient as transform
        // streams will request a new output buffer for the transformation.
        // Once transforms streams will be revisited to allow declaring if and how many output buffers
        // they need, it could make sense to make the following into a transform stream instead of a
        // raw eventData listener.
        struct EndStreamWhenAllBytesReceived
        {
            AsyncReadableStream& stream;
            size_t               remainingBytes;

            void operator()(AsyncBufferView::ID bufferID)
            {
                Span<const char> data;
                SC_ASSERT_RELEASE(stream.getBuffersPool().getReadableData(bufferID, data));
                if (remainingBytes == data.sizeInBytes())
                {
                    // Last chunk, we can remove this listener and terminate the readable stream
                    SC_ASSERT_RELEASE(stream.eventData.removeListener(*this));
                    stream.destroy();
                }
                else if (remainingBytes > data.sizeInBytes())
                {
                    // Intermediate chunk, we need to continue streaming data
                    remainingBytes -= data.sizeInBytes();
                }
                else if (remainingBytes < data.sizeInBytes())
                {
                    // This is currently an unhandled case where request streams has more data than it's expected.
                    // TODO: Logic will need to be expanded here for keep-alive and for bad clients
                    SC_ASSERT_RELEASE(false);
                }
            }
        };

        // It's important for this data event to be added last, so that it will be invoked after data has already
        // been dispatched through the pipeline, to avoid missing the last chunk.
        EndStreamWhenAllBytesReceived listener{asyncConnection.readableSocketStream, totalFileUploadBytes};
        SC_ASSERT_RELEASE(asyncConnection.readableSocketStream.eventData.addListener(listener));
    }
    return Result(true);
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
    if (extension == "jpeg" or extension == "jpg")
    {
        return "image/jpg";
    }
    if (extension == "svg")
    {
        return "image/svg+xml";
    }
    if (extension == "js")
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
    return "text/html";
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
} // namespace SC
