// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpConnection.h"
#include "Internal/HttpStringAppend.h"
#include <stdio.h>

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

void HttpRequest::reset()
{
    headersEndReceived = false;
    parsedSuccessfully = true;
    numHeaders         = 0;
    parser             = {};
}

Result HttpRequest::writeHeaders(const uint32_t maxSize, Span<const char> readData)
{
    // TODO: Handle error for available headers not big enough
    SC_TRY_MSG(readData.sizeInBytes() <= availableHeader.sizeInBytes(), "HttpRequest::parseHeaders - readData");
    ::memcpy(availableHeader.data(), readData.data(), readData.sizeInBytes());

    readHeaders = {readHeaders.data(), readHeaders.sizeInBytes() + readData.sizeInBytes()};

    const bool hasHeaderSpace = availableHeader.sliceStart(readData.sizeInBytes(), availableHeader);

    if (not hasHeaderSpace)
    {
        parsedSuccessfully = false;
        return Result::Error("Header space is finished");
    }

    if (readHeaders.sizeInBytes() > maxSize)
    {
        parsedSuccessfully = false;
        return Result::Error("Header size exceeded limit");
    }

    size_t readBytes;
    while (parsedSuccessfully and not readData.empty())
    {
        Span<const char> parsedData;
        parsedSuccessfully &= parser.parse(readData, readBytes, parsedData);
        parsedSuccessfully &= readData.sliceStart(readBytes, readData);
        if (parser.state == HttpParser::State::Finished)
            break;
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
            }
            if (parser.token == HttpParser::Token::HeadersEnd)
            {
                headersEndReceived = true;
                SC_TRY(findParserToken(HttpParser::Token::Url, url));
                break;
            }
        }
    }
    return Result(parsedSuccessfully);
}

//-------------------------------------------------------------------------------------------------------
// HttpResponse
//-------------------------------------------------------------------------------------------------------
Result HttpResponse::startResponse(int code)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.sizeInBytes() == 0, "startResponse must be the first call");
    GrowableBuffer<Span<char>> gb = {responseHeaders, responseHeadersCapacity};

    HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));

    SC_TRY(sb.append("HTTP/1.1 "));
    switch (code)
    {
    case 200: SC_TRY(sb.append("200 OK\r\n")); break;
    case 404: SC_TRY(sb.append("404 Not Found\r\n")); break;
    case 405: SC_TRY(sb.append("405 Not Allowed\r\n")); break;
    }
    return Result(true);
}

Result HttpResponse::addHeader(StringSpan headerName, StringSpan headerValue)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.sizeInBytes() != 0, "startResponse must be the first call");
    GrowableBuffer<Span<char>> gb = {responseHeaders, responseHeadersCapacity};

    HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));

    SC_TRY(sb.append(headerName));
    SC_TRY(sb.append(": "));
    SC_TRY(sb.append(headerValue));
    SC_TRY(sb.append("\r\n"));
    return Result(true);
}

Result HttpResponse::sendHeaders()
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.sizeInBytes() != 0, "startResponse must be the first call");
    {
        GrowableBuffer<Span<char>> gb = {responseHeaders, responseHeadersCapacity};

        HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
        SC_TRY(sb.append("\r\n"));
    }
    SC_TRY(writableStream->write(responseHeaders, {})); // headers first
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

void HttpResponse::reset() { headersSent = false; }

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

            connection.request.availableHeader = connection.headerMemory;
            (void)connection.request.availableHeader.sliceStartLength(0, 0, connection.request.readHeaders);
            numConnections++;
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
        return true;
    }
}

} // namespace SC
