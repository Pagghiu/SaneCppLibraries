// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpConnection.h"
#include "Internal/HttpStringIterator.h"

namespace SC
{
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

//-------------------------------------------------------------------------------------------------------
// HttpRequest
//-------------------------------------------------------------------------------------------------------
bool HttpRequest::findParserToken(HttpParser::Token token, StringSpan& res) const
{
    for (size_t idx = 0; idx < numHeaders; ++idx)
    {
        const HttpHeaderOffset& header = headerOffsets[idx];
        if (header.token == token)
        {
            res = StringSpan({readHeaders.data() + header.start, header.length}, false, StringEncoding::Ascii);
            return true;
        }
    }
    return false;
}

bool HttpRequest::getHeader(StringSpan headerName, StringSpan& value) const
{
    for (size_t idx = 0; idx < numHeaders; ++idx)
    {
        const HttpHeaderOffset& header = headerOffsets[idx];
        if (header.token == HttpParser::Token::HeaderName)
        {
            StringSpan name({readHeaders.data() + header.start, header.length}, false, StringEncoding::Ascii);
            if (idx + 1 < numHeaders && headerOffsets[idx + 1].token == HttpParser::Token::HeaderValue)
            {
                if (HttpStringIterator::equalsIgnoreCase(name, headerName))
                {
                    const HttpHeaderOffset& valueHeader = headerOffsets[idx + 1];
                    value = StringSpan({readHeaders.data() + valueHeader.start, valueHeader.length}, false,
                                       StringEncoding::Ascii);
                    return true;
                }
            }
        }
    }
    return false;
}

void HttpRequest::reset()
{
    headersEndReceived = false;
    parsedSuccessfully = true;
    headersEndMatch    = 0;
    numHeaders         = 0;
    parser             = {};
}

Result HttpRequest::writeHeaders(const uint32_t maxSize, Span<const char> readData, AsyncReadableStream& stream,
                                 AsyncBufferView::ID bufferID)
{
    if (headersEndReceived)
    {
        return Result(true);
    }

    size_t bytesToCopy     = readData.sizeInBytes();
    bool   foundHeadersEnd = false;

    for (size_t idx = 0; idx < readData.sizeInBytes(); ++idx)
    {
        const char current = readData.data()[idx];
        switch (headersEndMatch)
        {
        case 0: headersEndMatch = static_cast<uint8_t>(current == '\r' ? 1 : 0); break;
        case 1:
            if (current == '\n')
                headersEndMatch = 2;
            else if (current == '\r')
                headersEndMatch = 1;
            else
                headersEndMatch = 0;
            break;
        case 2: headersEndMatch = static_cast<uint8_t>(current == '\r' ? 3 : 0); break;
        case 3:
            if (current == '\n')
            {
                headersEndMatch = 4;
            }
            else if (current == '\r')
            {
                headersEndMatch = 1;
            }
            else
            {
                headersEndMatch = 0;
            }
            break;
        default: headersEndMatch = 0; break;
        }

        if (headersEndMatch == 4)
        {
            foundHeadersEnd = true;
            bytesToCopy     = idx + 1;
            headersEndMatch = 0;
            break;
        }
    }

    if (bytesToCopy > 0)
    {
        SC_TRY_MSG(bytesToCopy <= availableHeader.sizeInBytes(), "Header space is finished");
        SC_TRY_MSG(readHeaders.sizeInBytes() + bytesToCopy <= maxSize, "Header size exceeded limit");

        const size_t previousHeaderSize = readHeaders.sizeInBytes();
        ::memcpy(availableHeader.data(), readData.data(), bytesToCopy);
        readHeaders = {readHeaders.data(), previousHeaderSize + bytesToCopy};

        if (not availableHeader.sliceStart(bytesToCopy, availableHeader))
        {
            parsedSuccessfully = false;
            return Result::Error("Header space is finished");
        }
    }

    if (not foundHeadersEnd)
    {
        return Result(true);
    }

    Span<const char> headerData = {readHeaders.data(), readHeaders.sizeInBytes()};
    size_t           readBytes;
    while (parsedSuccessfully and parser.state != HttpParser::State::Finished)
    {
        Span<const char> parsedData;
        parsedSuccessfully &= parser.parse(headerData, readBytes, parsedData);
        if (not parsedSuccessfully)
        {
            break;
        }

        if (parser.state == HttpParser::State::Result)
        {
            HttpHeaderOffset header;
            header.token  = parser.token;
            header.start  = static_cast<uint32_t>(parser.tokenStart);
            header.length = static_cast<uint32_t>(parser.tokenLength);
            if (numHeaders < HttpRequest::MaxNumHeaders)
            {
                headerOffsets[numHeaders] = header;
                numHeaders++;
            }
            else
            {
                parsedSuccessfully = false;
                break;
            }
        }

        if (readBytes > 0)
        {
            parsedSuccessfully &= headerData.sliceStart(readBytes, headerData);
        }
        else if (parser.state != HttpParser::State::Finished)
        {
            parsedSuccessfully = false;
            break;
        }
    }

    SC_TRY(parsedSuccessfully);
    headersEndReceived = true;
    SC_TRY(findParserToken(HttpParser::Token::Url, url));

    if (bytesToCopy < readData.sizeInBytes())
    {
        const size_t        bodyOffset = bytesToCopy;
        const size_t        bodyLength = readData.sizeInBytes() - bodyOffset;
        AsyncBufferView::ID childID;
        SC_TRY(stream.getBuffersPool().createChildView(bufferID, bodyOffset, bodyLength, childID));
        SC_TRY(stream.unshift(childID));
        stream.getBuffersPool().unrefBuffer(childID);
    }

    return Result(true);
}

size_t HttpRequest::getHeadersLength() const
{
    if (numHeaders > 0)
    {
        const auto& last = headerOffsets[numHeaders - 1];
        if (last.token == HttpParser::Token::HeadersEnd)
            return static_cast<size_t>(last.start + last.length);
    }
    return 0;
}

bool HttpRequest::isMultipart() const
{
    StringSpan contentType;
    if (getHeader("Content-Type", contentType))
    {
        return HttpStringIterator::startsWithIgnoreCase(contentType, "multipart/form-data");
    }
    return false;
}

StringSpan HttpRequest::getBoundary() const
{
    StringSpan contentType;
    if (getHeader("Content-Type", contentType))
    {
        HttpStringIterator it(contentType);
        if (it.advanceUntilMatchesIgnoreCase("boundary="))
        {
            // Skip "boundary="
            for (size_t i = 0; i < 9; ++i)
                (void)it.stepForward();

            // Check for opening quote
            if (it.advanceIfMatches('"'))
            {
                // Quoted boundary: find closing quote
                auto start = it;
                while (!it.isAtEnd() && !it.match('"'))
                    (void)it.stepForward();
                return HttpStringIterator::fromIterators(start, it, contentType.getEncoding());
            }
            else
            {
                // Unquoted boundary: advance until delimiter
                auto start = it;
                char matched;
                (void)it.advanceUntilMatchesAny({';', ' ', '\r', '\0'}, matched);
                return HttpStringIterator::fromIterators(start, it, contentType.getEncoding());
            }
        }
    }
    return {};
}

//-------------------------------------------------------------------------------------------------------
// HttpResponse
//-------------------------------------------------------------------------------------------------------
Result HttpResponse::startResponse(int code)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.sizeInBytes() == 0, "startResponse must be the first call");
    SC_TRY(appendAsciiLiteral("HTTP/1.1 "));
    switch (code)
    {
    case 200: SC_TRY(appendAsciiLiteral("200 OK\r\n")); break;
    case 201: SC_TRY(appendAsciiLiteral("201 Created\r\n")); break;
    case 404: SC_TRY(appendAsciiLiteral("404 Not Found\r\n")); break;
    case 405: SC_TRY(appendAsciiLiteral("405 Method Not Allowed\r\n")); break;
    }
    return Result(true);
}

Result HttpResponse::addHeader(StringSpan headerName, StringSpan headerValue)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.sizeInBytes() != 0, "startResponse must be the first call");

    // Check if this is a Connection header and update keep-alive flags accordingly
    if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Connection")))
    {
        // HTTP/1.1 defines "keep-alive" and "close" as valid Connection values
        if (HttpStringIterator::equalsIgnoreCase(headerValue, StringSpan("keep-alive")))
        {
            if (forceDisableKeepAlive)
            {
                return Result::Error("HttpResponse::addHeader - keep-alive forcefully disabled");
            }
            keepAlive = true;
        }
        else if (HttpStringIterator::equalsIgnoreCase(headerValue, StringSpan("close")) ||
                 HttpStringIterator::equalsIgnoreCase(
                     headerValue, StringSpan("Closed"))) // Handle non-standard "Closed" used in tests
        {
            keepAlive = false;
        }
        // For any other value, we don't change the flags
        connectionHeaderAdded = true;
    }

    SC_TRY(appendAscii(headerName.toCharSpan()));
    SC_TRY(appendAsciiLiteral(": "));
    SC_TRY(appendAscii(headerValue.toCharSpan()));
    SC_TRY(appendAsciiLiteral("\r\n"));
    return Result(true);
}

Result HttpResponse::sendHeaders(Function<void(AsyncBufferView::ID)> callback)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.sizeInBytes() != 0, "startResponse must be the first call");
    // Auto-add Connection header only if not already added manually
    if (not connectionHeaderAdded)
    {
        if (forceDisableKeepAlive or not keepAlive)
        {
            SC_TRY(appendAsciiLiteral("Connection: close\r\n"));
        }
        else
        {
            SC_TRY(appendAsciiLiteral("Connection: keep-alive\r\n"));
        }
    }
    SC_TRY(appendAsciiLiteral("\r\n"));
    SC_TRY(writableStream->write(responseHeaders, move(callback))); // headers first
    headersSent = true;
    return Result(true);
}

Result HttpResponse::end()
{
    SC_TRY_MSG(headersSent, "Forgot to send headers");
    writableStream->end();
    return Result(true);
}

void HttpResponse::grabUnusedHeaderMemory(HttpRequest& request)
{
    responseHeaders         = {request.availableHeader.data(), 0};
    responseHeadersCapacity = request.availableHeader.sizeInBytes();
}

void HttpResponse::reset()
{
    headersSent             = false;
    keepAlive               = true;
    connectionHeaderAdded   = false;
    forceDisableKeepAlive   = false;
    responseHeaders         = {};
    responseHeadersCapacity = 0;
}

void HttpResponse::setKeepAlive(bool value) { keepAlive = value; }

Result HttpResponse::appendAscii(Span<const char> ascii)
{
    SC_TRY_MSG(responseHeaders.sizeInBytes() + ascii.sizeInBytes() <= responseHeadersCapacity,
               "HttpResponse::appendAscii - header space is finished");
    ::memcpy(responseHeaders.data() + responseHeaders.sizeInBytes(), ascii.data(), ascii.sizeInBytes());
    responseHeaders = {responseHeaders.data(), responseHeaders.sizeInBytes() + ascii.sizeInBytes()};
    return Result(true);
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
        if (connection.headerMemory.sizeInBytes() == 0)
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
            connection.request.availableHeader = connection.headerMemory;
            (void)connection.request.availableHeader.sliceStartLength(0, 0, connection.request.readHeaders);
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
    if (numConnections == 0 or connectionID.index >= connections.sizeInElements())
    {
        return false;
    }
    else
    {
        connections[connectionID.index].reset();
        numConnections--;
        if (connectionID.index == highestActiveConnection)
        {
            while (highestActiveConnection > 0)
            {
                if (connections[highestActiveConnection].state == HttpConnection::State::Active)
                {
                    break;
                }
                highestActiveConnection--;
            }
        }
        return true;
    }
}

} // namespace SC
