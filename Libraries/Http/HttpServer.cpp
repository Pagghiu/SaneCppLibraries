// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpServer.h"
#include "Internal/HttpStringAppend.h"
#include <stdio.h>

namespace SC
{
//-------------------------------------------------------------------------------------------------------
// HttpServerClient
//-------------------------------------------------------------------------------------------------------
HttpServerClient::HttpServerClient()
{
    // This is needed on Linux so that ReadableSocketStream constructor don't need to be exported
    // in order to be used across plugin boundaries.
}

//-------------------------------------------------------------------------------------------------------
// HttpRequest
//-------------------------------------------------------------------------------------------------------
bool HttpRequest::find(HttpParser::Token token, StringSpan& res) const
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

Result HttpRequest::parse(const uint32_t maxSize, Span<const char> readData)
{
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
            detail::HttpHeaderOffset header;
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
                SC_TRY(find(HttpParser::Token::Url, url));
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
    responseEnded = false;
    return Result(true);
}

Result HttpResponse::addHeader(StringSpan headerName, StringSpan headerValue)
{
    SC_TRY_MSG(responseHeaders.sizeInBytes() != 0, "startResponse must be the first call");
    GrowableBuffer<Span<char>> gb = {responseHeaders, responseHeadersCapacity};

    HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));

    SC_TRY(sb.append(headerName));
    SC_TRY(sb.append(": "));
    SC_TRY(sb.append(headerValue));
    SC_TRY(sb.append("\r\n"));
    return Result(true);
}

Result HttpResponse::end(Span<const char> span)
{
    SC_TRY_MSG(responseHeaders.sizeInBytes() != 0, "startResponse must be the first call");
    {
        GrowableBuffer<Span<char>> gb = {responseHeaders, responseHeadersCapacity};

        HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
        SC_TRY(sb.append("Content-Length:"));
        char bufferSize[32];
        snprintf(bufferSize, sizeof(bufferSize), "%zu", span.sizeInBytes());
        StringSpan ss = {{bufferSize, strlen(bufferSize)}, false, StringEncoding::Ascii};
        SC_TRY(sb.append(ss));
        SC_TRY(sb.append("\r\n\r\n"));
    }
    SC_TRY(outputBuffer.assign(span));
    return end();
}

Result HttpResponse::end()
{
    responseEnded = true;
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// HttpServer
//-------------------------------------------------------------------------------------------------------
Result HttpServer::start(Memory& memory)
{
    clients       = memory.clients;
    headersMemory = &memory.headersMemory;
    return Result(true);
}

bool HttpServer::allocateClient(size_t& idx)
{
    for (idx = 0; idx < clients.sizeInElements(); ++idx)
    {
        HttpServerClient& client = clients[idx];
        if (client.state == HttpServerClient::State::Free)
        {
            client.state = HttpServerClient::State::Used;
            client.index = idx;

            const size_t headerBufferSize = headersMemory->size() / clients.sizeInElements();

            client.request.availableHeader = {headersMemory->data(), headerBufferSize};
            client.request.readHeaders     = {headersMemory->data(), 0};
            numClients++;
            return true;
        }
    }
    return false;
}

bool HttpServer::deallocateClient(HttpServerClient& client)
{
    if (numClients == 0 or client.index >= clients.sizeInElements())
    {
        return false;
    }
    else
    {
        clients[client.index].reset();
        numClients--;
        return true;
    }
}

} // namespace SC
