// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpConnection.h"
#include "Internal/HttpStringIterator.h"

#include "../Foundation/Assert.h"
#include "Internal/HttpFixedBufferWriter.inl"
#include "Internal/HttpParsedHeaders.inl"

namespace SC
{
//-------------------------------------------------------------------------------------------------------
// HttpMultipartWriter
//-------------------------------------------------------------------------------------------------------
void HttpMultipartWriter::reset()
{
    numParts = 0;
    boundary = {};
    ::memset(boundaryStorage, 0, sizeof(boundaryStorage));
}

Result HttpMultipartWriter::setBoundary(StringSpan boundaryValue)
{
    reset();
    SC_TRY_MSG(boundaryValue.sizeInBytes() > 0, "HttpMultipartWriter::setBoundary empty boundary");
    SC_TRY_MSG(boundaryValue.sizeInBytes() < sizeof(boundaryStorage), "HttpMultipartWriter::setBoundary too long");
    ::memcpy(boundaryStorage, boundaryValue.bytesWithoutTerminator(), boundaryValue.sizeInBytes());
    boundary = StringSpan::fromNullTerminated(boundaryStorage, StringEncoding::Ascii);
    return Result(true);
}

Result HttpMultipartWriter::addField(StringSpan fieldName, StringSpan value)
{
    SC_TRY_MSG(boundary.sizeInBytes() > 0, "HttpMultipartWriter::addField boundary not set");
    SC_TRY_MSG(numParts < MaxParts, "HttpMultipartWriter::addField too many parts");
    Part& part       = parts[numParts++];
    part.partName    = fieldName;
    part.fileName    = {};
    part.contentType = {};
    part.body        = value.toCharSpan();
    return Result(true);
}

Result HttpMultipartWriter::addFile(StringSpan fieldName, StringSpan fileName, Span<const char> body,
                                    StringSpan contentType)
{
    SC_TRY_MSG(boundary.sizeInBytes() > 0, "HttpMultipartWriter::addFile boundary not set");
    SC_TRY_MSG(numParts < MaxParts, "HttpMultipartWriter::addFile too many parts");
    Part& part       = parts[numParts++];
    part.partName    = fieldName;
    part.fileName    = fileName;
    part.contentType = contentType;
    part.body        = body;
    return Result(true);
}

size_t HttpMultipartWriter::getContentLength() const
{
    size_t total = 0;
    for (size_t idx = 0; idx < numParts; ++idx)
    {
        const Part& part = parts[idx];
        total += 2 + boundary.sizeInBytes() + 2; // --boundary\r\n
        total += sizeof("Content-Disposition: form-data; name=\"") - 1;
        total += part.partName.sizeInBytes();
        total += 1;
        if (part.fileName.sizeInBytes() > 0)
        {
            total += sizeof("; filename=\"") - 1;
            total += part.fileName.sizeInBytes();
            total += 1;
        }
        total += 2;

        if (part.contentType.sizeInBytes() > 0)
        {
            total += sizeof("Content-Type: ") - 1;
            total += part.contentType.sizeInBytes();
            total += 2;
        }
        total += 2;
        total += part.body.sizeInBytes();
        total += 2;
    }
    total += 2 + boundary.sizeInBytes() + 4; // --boundary--\r\n
    return total;
}
//-------------------------------------------------------------------------------------------------------
// HttpConnection
//-------------------------------------------------------------------------------------------------------
HttpConnection::HttpConnection()
{
    // This is needed on Linux so that ReadableSocketStream constructor don't need to be exported
    // in order to be used across plugin boundaries.
}

void HttpConnection::reset()
{
    request.reset();
    response.reset();
    state = State::Inactive;
}

void HttpConnectionBase::reset()
{
    (void)pipeline.unpipe();
    readableSocketStream.destroy();
    writableSocketStream.destroy();
    if (socket.isValid())
    {
        (void)socket.close();
    }
}

//-------------------------------------------------------------------------------------------------------
// HttpIncomingMessage
//-------------------------------------------------------------------------------------------------------
void HttpIncomingMessage::resetIncoming(HttpParser::Type type, Span<char> memory)
{
    headerMemory       = memory;
    readableStream     = nullptr;
    bodyBytesRemaining = 0;
    parsedHeaders.reset(type, memory);
}

bool HttpIncomingMessage::findParserToken(HttpParser::Token token, StringSpan& res) const
{
    return parsedHeaders.findParserToken(token, res);
}

bool HttpIncomingMessage::getHeader(StringSpan headerName, StringSpan& value) const
{
    return parsedHeaders.getHeader(headerName, value);
}

AsyncReadableStream& HttpIncomingMessage::getReadableStream()
{
    SC_ASSERT_RELEASE(readableStream != nullptr);
    return *readableStream;
}

const AsyncReadableStream& HttpIncomingMessage::getReadableStream() const
{
    SC_ASSERT_RELEASE(readableStream != nullptr);
    return *readableStream;
}

Result HttpIncomingMessage::consumeBodyBytes(size_t bytes)
{
    SC_TRY_MSG(bytes <= bodyBytesRemaining, "HttpIncomingMessage body exceeds Content-Length");
    bodyBytesRemaining -= bytes;
    return Result(true);
}

Result HttpIncomingMessage::writeHeaders(const uint32_t maxSize, Span<const char> readData, AsyncReadableStream& stream,
                                         AsyncBufferView::ID bufferID, const char* outOfSpaceError,
                                         const char* sizeExceededError, bool stopAtHeadersEnd)
{
    attachReadableStream(stream);
    const auto onParserResult = [&](const HttpParser&) -> Result { return Result(true); };
    SC_TRY(parsedHeaders.writeHeaders(maxSize, readData, stream, bufferID, outOfSpaceError, sizeExceededError,
                                      stopAtHeadersEnd, onParserResult));
    return Result(true);
}

size_t HttpIncomingMessage::getHeadersLength() const { return parsedHeaders.getHeadersLength(); }

bool HttpIncomingMessage::isMultipart() const
{
    StringSpan contentType;
    if (getHeader("Content-Type", contentType))
    {
        return HttpStringIterator::startsWithIgnoreCase(contentType, "multipart/form-data");
    }
    return false;
}

StringSpan HttpIncomingMessage::getBoundary() const
{
    StringSpan contentType;
    if (getHeader("Content-Type", contentType))
    {
        HttpStringIterator it(contentType);
        if (it.advanceUntilMatchesIgnoreCase("boundary="))
        {
            for (size_t i = 0; i < 9; ++i)
                (void)it.stepForward();

            if (it.advanceIfMatches('"'))
            {
                auto start = it;
                while (not it.isAtEnd() and not it.match('"'))
                    (void)it.stepForward();
                return HttpStringIterator::fromIterators(start, it, contentType.getEncoding());
            }

            auto start = it;
            char matched;
            (void)it.advanceUntilMatchesAny({';', ' ', '\r', '\0'}, matched);
            return HttpStringIterator::fromIterators(start, it, contentType.getEncoding());
        }
    }
    return {};
}

//-------------------------------------------------------------------------------------------------------
// HttpRequest
//-------------------------------------------------------------------------------------------------------
void HttpRequest::setHeaderMemory(Span<char> memory)
{
    resetIncoming(HttpParser::Type::Request, memory);
    url = {};
}

void HttpRequest::reset()
{
    resetIncoming(HttpParser::Type::Request, headerMemory);
    url = {};
}

Result HttpRequest::writeHeaders(const uint32_t maxSize, Span<const char> readData, AsyncReadableStream& stream,
                                 AsyncBufferView::ID bufferID)
{
    SC_TRY(HttpIncomingMessage::writeHeaders(maxSize, readData, stream, bufferID, "Header space is finished",
                                             "Header size exceeded limit", false));
    SC_TRY(findParserToken(HttpParser::Token::Url, url));
    setBodyBytesRemaining(getParser().contentLength);
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// HttpAsyncClientResponse
//-------------------------------------------------------------------------------------------------------
void HttpAsyncClientResponse::reset(Span<char> memory) { resetIncoming(HttpParser::Type::Response, memory); }

Result HttpAsyncClientResponse::writeHeaders(uint32_t maxHeaderSize, Span<const char> readData,
                                             AsyncReadableStream& stream, AsyncBufferView::ID bufferID)
{
    SC_TRY(HttpIncomingMessage::writeHeaders(maxHeaderSize, readData, stream, bufferID,
                                             "HttpAsyncClientResponse header space is finished",
                                             "HttpAsyncClientResponse header size exceeded", true));
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// HttpOutgoingMessage
//-------------------------------------------------------------------------------------------------------
void HttpOutgoingMessage::setHeaderMemory(Span<char> memory)
{
    headerMemory = memory;
    responseHeaders.reset(memory);
}

bool HttpOutgoingMessage::hasHeader(KnownHeader header) const
{
    switch (header)
    {
    case KnownHeader::Connection: return connectionHeaderAdded;
    case KnownHeader::Host: return hostHeaderAdded;
    case KnownHeader::UserAgent: return userAgentHeaderAdded;
    case KnownHeader::ContentLength: return contentLengthAdded;
    case KnownHeader::ContentType: return contentTypeAdded;
    case KnownHeader::TransferEncoding: return transferEncodingAdded;
    }
    return false;
}

Result HttpOutgoingMessage::addHeader(StringSpan headerName, StringSpan headerValue)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.writtenBytes() != 0, "startResponse or startRequest must be the first call");

    if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Connection")))
    {
        if (HttpStringIterator::equalsIgnoreCase(headerValue, StringSpan("keep-alive")))
        {
            if (forceDisableKeepAlive)
            {
                return Result::Error("HttpOutgoingMessage::addHeader - keep-alive forcefully disabled");
            }
            keepAlive = true;
        }
        else if (HttpStringIterator::equalsIgnoreCase(headerValue, StringSpan("close")) ||
                 HttpStringIterator::equalsIgnoreCase(headerValue, StringSpan("Closed")))
        {
            keepAlive = false;
        }
        connectionHeaderAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Host")))
    {
        hostHeaderAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("User-Agent")))
    {
        userAgentHeaderAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Content-Length")))
    {
        contentLengthAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Content-Type")))
    {
        contentTypeAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Transfer-Encoding")))
    {
        transferEncodingAdded = true;
    }

    SC_TRY(responseHeaders.appendHeader(headerName, headerValue,
                                        "HttpOutgoingMessage::appendAscii - header space is finished"));
    return Result(true);
}

Result HttpOutgoingMessage::sendHeaders(Function<void(AsyncBufferView::ID)> callback)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.writtenBytes() != 0, "startResponse or startRequest must be the first call");
    if (not connectionHeaderAdded)
    {
        if (forceDisableKeepAlive or not keepAlive)
        {
            SC_TRY(responseHeaders.appendLiteral("Connection: close\r\n",
                                                 "HttpOutgoingMessage::appendAscii - header space is finished"));
        }
        else
        {
            SC_TRY(responseHeaders.appendLiteral("Connection: keep-alive\r\n",
                                                 "HttpOutgoingMessage::appendAscii - header space is finished"));
        }
    }
    SC_TRY(responseHeaders.appendLiteral("\r\n", "HttpOutgoingMessage::appendAscii - header space is finished"));
    SC_TRY(writableStream->write(responseHeaders.written(), move(callback)));
    headersSent = true;
    return Result(true);
}

void HttpOutgoingMessage::reset()
{
    headersSent           = false;
    endCalled             = false;
    keepAlive             = true;
    connectionHeaderAdded = false;
    hostHeaderAdded       = false;
    userAgentHeaderAdded  = false;
    contentLengthAdded    = false;
    contentTypeAdded      = false;
    transferEncodingAdded = false;
    forceDisableKeepAlive = false;
    responseHeaders.reset(headerMemory);
}

Result HttpOutgoingMessage::end()
{
    SC_TRY_MSG(headersSent, "Forgot to send headers");
    writableStream->end();
    endCalled = true;
    return Result(true);
}

void HttpOutgoingMessage::setKeepAlive(bool value) { keepAlive = value; }

//-------------------------------------------------------------------------------------------------------
// HttpResponse
//-------------------------------------------------------------------------------------------------------
Result HttpResponse::startResponse(int code)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.writtenBytes() == 0, "startResponse must be the first call");
    SC_TRY(responseHeaders.appendLiteral("HTTP/1.1 ", "HttpResponse::appendAscii - header space is finished"));
    switch (code)
    {
    case 200:
        SC_TRY(responseHeaders.appendLiteral("200 OK\r\n", "HttpResponse::appendAscii - header space is finished"));
        break;
    case 201:
        SC_TRY(
            responseHeaders.appendLiteral("201 Created\r\n", "HttpResponse::appendAscii - header space is finished"));
        break;
    case 404:
        SC_TRY(
            responseHeaders.appendLiteral("404 Not Found\r\n", "HttpResponse::appendAscii - header space is finished"));
        break;
    case 405:
        SC_TRY(responseHeaders.appendLiteral("405 Method Not Allowed\r\n",
                                             "HttpResponse::appendAscii - header space is finished"));
        break;
    }
    return Result(true);
}

void HttpResponse::grabUnusedHeaderMemory(HttpRequest& request) { setHeaderMemory(request.getUnusedHeaderMemory()); }

//-------------------------------------------------------------------------------------------------------
// HttpAsyncClientRequest
//-------------------------------------------------------------------------------------------------------
void HttpAsyncClientRequest::reset()
{
    HttpOutgoingMessage::reset();
    method                      = HttpParser::Method::HttpGET;
    url                         = {};
    bodyStream                  = nullptr;
    bodyType                    = BodyType::None;
    bodySpan                    = {};
    contentLength               = 0;
    multipartWriter             = nullptr;
    defaultHost                 = {};
    userHeadersSentCallback     = {};
    internalHeadersSentCallback = {};
}

Result HttpAsyncClientRequest::startRequest(HttpParser::Method value, StringSpan valueURL)
{
    static constexpr const char* HeaderSpaceFinished = "HttpAsyncClientRequest header space is finished";

    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.writtenBytes() == 0, "startRequest must be the first call");

    method = value;
    url    = valueURL.sizeInBytes() > 0 ? valueURL : StringSpan("/");

    switch (method)
    {
    case HttpParser::Method::HttpGET: SC_TRY(responseHeaders.appendLiteral("GET ", HeaderSpaceFinished)); break;
    case HttpParser::Method::HttpPUT: SC_TRY(responseHeaders.appendLiteral("PUT ", HeaderSpaceFinished)); break;
    case HttpParser::Method::HttpPOST: SC_TRY(responseHeaders.appendLiteral("POST ", HeaderSpaceFinished)); break;
    }

    SC_TRY(responseHeaders.append(url, HeaderSpaceFinished));
    SC_TRY(responseHeaders.appendLiteral(" HTTP/1.1\r\n", HeaderSpaceFinished));
    return Result(true);
}

Result HttpAsyncClientRequest::setExpectedBodyLength(uint64_t value)
{
    bodyType        = value > 0 ? BodyType::Manual : BodyType::None;
    bodySpan        = {};
    bodyStream      = nullptr;
    multipartWriter = nullptr;
    contentLength   = value;
    return Result(true);
}

Result HttpAsyncClientRequest::setBody(Span<const char> value)
{
    bodyType        = BodyType::Span;
    bodySpan        = value;
    bodyStream      = nullptr;
    contentLength   = value.sizeInBytes();
    multipartWriter = nullptr;
    return Result(true);
}

void HttpAsyncClientRequest::setBody(AsyncReadableStream& stream, uint64_t contentLengthValue)
{
    bodyType        = BodyType::Stream;
    bodySpan        = {};
    bodyStream      = &stream;
    contentLength   = contentLengthValue;
    multipartWriter = nullptr;
}

void HttpAsyncClientRequest::setMultipart(HttpMultipartWriter& value)
{
    bodyType        = BodyType::Multipart;
    bodySpan        = {};
    bodyStream      = nullptr;
    multipartWriter = &value;
    contentLength   = value.getContentLength();
}

Result HttpAsyncClientRequest::sendHeaders(Function<void(AsyncBufferView::ID)> callback)
{
    static constexpr const char* HeaderSpaceFinished = "HttpAsyncClientRequest header space is finished";

    if (not hasHeader(KnownHeader::UserAgent))
    {
        SC_TRY(HttpOutgoingMessage::addHeader("User-Agent", "SC"));
    }
    if (defaultHost.sizeInBytes() > 0 and not hasHeader(KnownHeader::Host))
    {
        SC_TRY(HttpOutgoingMessage::addHeader("Host", defaultHost));
    }
    if (bodyType == BodyType::Multipart and not hasHeader(KnownHeader::ContentType))
    {
        SC_TRY_MSG(multipartWriter != nullptr, "HttpAsyncClientRequest multipart writer missing");
        SC_TRY_MSG(multipartWriter->getBoundary().sizeInBytes() > 0,
                   "HttpAsyncClientRequest multipart boundary missing");
        SC_TRY(responseHeaders.appendLiteral("Content-Type: multipart/form-data; boundary=", HeaderSpaceFinished));
        SC_TRY(responseHeaders.append(multipartWriter->getBoundary(), HeaderSpaceFinished));
        SC_TRY(responseHeaders.appendLiteral("\r\n", HeaderSpaceFinished));
        contentTypeAdded = true;
    }

    const bool hasBody = bodyType != BodyType::None;
    if (hasBody and not hasHeader(KnownHeader::ContentLength))
    {
        SC_TRY(responseHeaders.appendContentLength(contentLength, HeaderSpaceFinished,
                                                   "HttpAsyncClientRequest failed formatting Content-Length"));
        contentLengthAdded = true;
    }

    userHeadersSentCallback = move(callback);
    if (userHeadersSentCallback.isValid() or internalHeadersSentCallback.isValid())
    {
        return HttpOutgoingMessage::sendHeaders({[this](AsyncBufferView::ID bufferID)
                                                 {
                                                     if (userHeadersSentCallback.isValid())
                                                     {
                                                         userHeadersSentCallback(bufferID);
                                                     }
                                                     if (internalHeadersSentCallback.isValid())
                                                     {
                                                         internalHeadersSentCallback(bufferID);
                                                     }
                                                 }});
    }
    return HttpOutgoingMessage::sendHeaders();
}

//-------------------------------------------------------------------------------------------------------
// HttpConnectionsPool
//-------------------------------------------------------------------------------------------------------
Result HttpConnectionsPool::init(SpanWithStride<HttpConnection> connectionsStorage)
{
    SC_TRY_MSG(numConnections == 0, "HttpConnectionsPool::init - numConnections != 0");
    for (size_t idx = 0; idx < connectionsStorage.sizeInElements(); ++idx)
    {
        HttpConnection& connection = connectionsStorage[idx];
        if (connection.getHeaderMemory().sizeInBytes() == 0)
        {
            return Result::Error("HttpConnection::headerMemory is empty");
        }
    }

    connections = connectionsStorage;
    return Result(true);
}

Result HttpConnectionsPool::close()
{
    SC_TRY_MSG(numConnections == 0, "HttpConnectionsPool::close - numConnections != 0");
    connections = {};
    return Result(true);
}

bool HttpConnectionsPool::activateNew(HttpConnection::ID& connectionID)
{
    for (size_t idx = 0; idx < connections.sizeInElements(); ++idx)
    {
        HttpConnection& connection = connections[idx];
        if (connection.state == HttpConnection::State::Inactive)
        {
            connection.state        = HttpConnection::State::Active;
            connectionID.index      = idx;
            connection.connectionID = connectionID;

            if (idx > highestActiveConnection)
            {
                highestActiveConnection = idx;
            }
            connection.request.setHeaderMemory(connection.getHeaderMemory());
            numConnections++;
            if (numConnections == connections.sizeInElements())
            {
                // avoid deadlock by force disabling keep-alive if this is the last connection
                // TODO: Consider some criteria that will disable keep alive after a threshold of active connections
                connection.response.forceDisableKeepAlive = true;
            }
            return true;
        }
    }
    return false;
}

bool HttpConnectionsPool::deactivate(HttpConnection::ID connectionID)
{
    HttpConnection& connection = connections[connectionID.index];
    if (connection.state == HttpConnection::State::Inactive)
    {
        return false;
    }

    connection.reset();
    numConnections--;

    if (numConnections == 0)
    {
        highestActiveConnection = 0;
    }
    else if (connectionID.index == highestActiveConnection)
    {
        while (highestActiveConnection > 0 and
               connections[highestActiveConnection].state == HttpConnection::State::Inactive)
        {
            highestActiveConnection--;
        }
    }
    return true;
}

Result HttpConnectionsPool::Memory::assignTo(HttpConnectionsPool::Configuration conf,
                                             SpanWithStride<HttpConnection>     connectionsSpan)
{
    const size_t numClients = connectionsSpan.sizeInElements();
    SC_TRY_MSG(allReadQueue.sizeInElements() >= numClients * conf.readQueueSize, "Insufficient read queue");
    SC_TRY_MSG(allWriteQueue.sizeInElements() >= numClients * conf.writeQueueSize, "Insufficient write queue");
    SC_TRY_MSG(allBuffers.sizeInElements() >= numClients * conf.buffersQueueSize, "Insufficient buffers queue");
    SC_TRY_MSG(allHeaders.sizeInElements() >= numClients * conf.headerBytesLength, "Insufficient headers storage");
    SC_TRY_MSG(allStreams.sizeInElements() >= numClients * conf.streamBytesLength, "Insufficient streams storage");
    for (size_t idx = 0; idx < numClients; ++idx)
    {
        HttpConnection& connection = connectionsSpan[idx];

        const size_t NumSlices   = conf.readQueueSize;
        const size_t SliceLength = conf.streamBytesLength / NumSlices;

        Span<AsyncBufferView> buffers;
        SC_TRY(allBuffers.sliceStartLength(idx * conf.buffersQueueSize, conf.buffersQueueSize, buffers));
        Span<char> streamStorage;
        SC_TRY(allStreams.sliceStartLength(idx * conf.streamBytesLength, conf.streamBytesLength, streamStorage));
        for (size_t sliceIdx = 0; sliceIdx < NumSlices; ++sliceIdx)
        {
            Span<char> slice;
            SC_TRY(streamStorage.sliceStartLength(sliceIdx * SliceLength, SliceLength, slice));
            buffers[sliceIdx] = slice;
            buffers[sliceIdx].setReusable(true);
        }
        connection.buffersPool.setBuffers(buffers);
        Span<char> headerStorage;
        SC_TRY(allHeaders.sliceStartLength(idx * conf.headerBytesLength, conf.headerBytesLength, headerStorage));
        connection.setHeaderMemory(headerStorage);
        Span<AsyncReadableStream::Request> readQueue;
        SC_TRY(allReadQueue.sliceStartLength(idx * conf.readQueueSize, conf.readQueueSize, readQueue));
        Span<AsyncWritableStream::Request> writeQueue;
        SC_TRY(allWriteQueue.sliceStartLength(idx * conf.writeQueueSize, conf.writeQueueSize, writeQueue));
        connection.readableSocketStream.setReadQueue(readQueue);
        connection.writableSocketStream.setWriteQueue(writeQueue);
    }
    return Result(true);
}
} // namespace SC
