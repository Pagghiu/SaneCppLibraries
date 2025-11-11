// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpServer.h"
#include "Internal/HttpStringAppend.h"
#include <stdio.h>

// HttpRequest
bool SC::HttpRequest::find(HttpParser::Token token, StringSpan& res) const
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

void SC::HttpRequest::reset()
{
    headersEndReceived = false;
    parsedSuccessfully = true;
    numHeaders         = 0;
    parser             = {};
}

// HttpResponse
SC::Result SC::HttpResponse::startResponse(int code)
{
    GrowableBuffer<decltype(outputBuffer)> gb = {outputBuffer};

    HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));

    sb.clear();
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

SC::Result SC::HttpResponse::addHeader(StringSpan headerName, StringSpan headerValue)
{
    GrowableBuffer<decltype(outputBuffer)> gb = {outputBuffer};

    HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));

    SC_TRY(sb.append(headerName));
    SC_TRY(sb.append(": "));
    SC_TRY(sb.append(headerValue));
    SC_TRY(sb.append("\r\n"));
    return Result(true);
}

SC::Result SC::HttpResponse::end(Span<const char> span)
{
    {
        GrowableBuffer<decltype(outputBuffer)> gb = {outputBuffer};

        HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
        SC_TRY(sb.append("Content-Length:"));
        char bufferSize[32];
        snprintf(bufferSize, sizeof(bufferSize), "%zu", span.sizeInBytes());
        StringSpan ss = {{bufferSize, strlen(bufferSize)}, false, StringEncoding::Ascii};
        SC_TRY(sb.append(ss));
        SC_TRY(sb.append("\r\n\r\n"));
    }
    SC_TRY(not outputBuffer.isEmpty());
    SC_TRY(outputBuffer.append(span));
    return end();
}

SC::Result SC::HttpResponse::end()
{
    responseEnded = true;
    return Result(true);
}

// HttpServer

SC::Result SC::HttpServer::start(AsyncEventLoop& loop, StringSpan address, uint16_t port, Memory& memory)
{
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    eventLoop = &loop;
    SC_TRY(eventLoop->createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    SocketServer server(serverSocket);
    SC_TRY(server.bind(nativeAddress));
    SC_TRY(server.listen(511));

    asyncServerAccept.setDebugName("HttpServer");
    asyncServerAccept.callback.bind<HttpServer, &HttpServer::onNewClient>(*this);
    SC_TRY(asyncServerAccept.start(*eventLoop, serverSocket));
    started = true;

    clients       = memory.clients;
    headersMemory = &memory.headersMemory;
    return Result(true);
}

SC::Result SC::HttpServer::stopAsync()
{
    if (not asyncServerAccept.isFree())
    {
        SC_TRY(asyncServerAccept.stop(*eventLoop));
    }

    for (HttpServerClient& it : clients)
    {
        if (it.state != HttpServerClient::State::Free)
        {
            closeAsync(it);
        }
    }
    return Result(true);
}

SC::Result SC::HttpServer::stopSync()
{
    stopping = true;
    SC_TRY(stopAsync());
    while (numClients > 0)
    {
        SC_TRY(eventLoop->runNoWait());
    }
    while (not asyncServerAccept.isFree())
    {
        SC_TRY(eventLoop->runNoWait());
    }
    stopping = false;
    started  = false;
    return Result(true);
}

SC::Result SC::HttpServer::parse(HttpRequest& request, const uint32_t maxSize, Span<const char> readData)
{
    bool& parsedSuccessfully = request.parsedSuccessfully;
    if (request.readHeaders.sizeInBytes() > maxSize)
    {
        parsedSuccessfully = false;
        return Result::Error("Header size exceeded limit");
    }

    size_t readBytes;
    while (request.parsedSuccessfully and not readData.empty())
    {
        HttpParser&      parser = request.parser;
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
            if (request.numHeaders < HttpRequest::MaxNumHeaders)
            {
                request.headerOffsets[request.numHeaders] = header;
                request.numHeaders++;
            }
            else
            {
                parsedSuccessfully = false;
            }
            if (parser.token == HttpParser::Token::HeadersEnd)
            {
                request.headersEndReceived = true;
                SC_TRY(request.find(HttpParser::Token::Url, request.url));
                break;
            }
        }
    }
    return Result(parsedSuccessfully);
}

void SC::HttpServer::onNewClient(AsyncSocketAccept::Result& result)
{
    SocketDescriptor acceptedClient;
    if (not result.moveTo(acceptedClient))
    {
        // TODO: Invoke an error
        return;
    }
    size_t idx;
    for (idx = 0; idx < clients.sizeInElements(); ++idx)
    {
        auto& it = clients[idx];
        if (it.state == HttpServerClient::State::Free)
        {
            break;
        }
    }
    numClients++;
    // Allocate always succeeds because we pause asyncAccept when the arena is full
    SC_ASSERT_RELEASE(idx != clients.sizeInElements());
    HttpServerClient& client = clients[idx];
    client.state             = HttpServerClient::State::Used;
    client.response.server   = this;

    client.response.key = idx;
    client.socket       = move(acceptedClient);
    client.asyncSend.setDebugName(client.debugName);
    client.asyncReceive.setDebugName(client.debugName);
    client.asyncReceive.callback.bind<HttpServer, &HttpServer::onReceive>(*this);

    const size_t headerBufferSize = headersMemory->size() / clients.sizeInElements();

    client.request.availableHeader = {headersMemory->data(), headerBufferSize};
    client.request.readHeaders     = {headersMemory->data(), 0};
    // This cannot fail because start reports only incorrect API usage (AsyncRequest already in use etc.)
    SC_TRUST_RESULT(client.asyncReceive.start(*eventLoop, client.socket, client.request.availableHeader));

    // Only reactivate asyncAccept if arena is not full (otherwise it's being reactivated in closeAsync)
    result.reactivateRequest(numClients < clients.sizeInElements());
}

void SC::HttpServer::onReceive(AsyncSocketReceive::Result& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF
    HttpServerClient& client = SC_COMPILER_FIELD_OFFSET(HttpServerClient, asyncReceive, result.getAsync());
    SC_COMPILER_WARNING_POP
    SC_ASSERT_RELEASE(&client.asyncReceive == &result.getAsync());
    Span<char> readData;
    if (not result.get(readData))
    {
        // TODO: Invoke on error
        return;
    }
    client.request.readHeaders = {client.request.readHeaders.data(),
                                  client.request.readHeaders.sizeInBytes() + readData.sizeInBytes()};
    const bool hasHeaderSpace =
        client.request.availableHeader.sliceStart(readData.sizeInBytes(), client.request.availableHeader);

    if (not parse(client.request, maxHeaderSize, readData))
    {
        // TODO: Invoke on error
        return;
    }
    else if (not hasHeaderSpace)
    {
        // TODO: Invoke on error (no more header space)
        return;
    }
    if (client.request.headersEndReceived)
    {
        onRequest(client.request, client.response);
    }
    if (client.response.mustBeFlushed())
    {
        client.asyncSend.setDebugName(client.debugName);

        auto outspan = client.response.outputBuffer.toSpan();
        client.asyncSend.callback.bind<HttpServer, &HttpServer::onAfterSend>(*this);
        auto res = client.asyncSend.start(*eventLoop, client.socket, outspan);
        if (not res)
        {
            // TODO: Invoke on error
            return;
        }
    }
    else if (not result.completionData.disconnected)
    {
        result.reactivateRequest(true);
    }
}

void SC::HttpServer::onAfterSend(AsyncSocketSend::Result& result)
{
    if (result.isValid())
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF
        HttpServerClient& requestClient = SC_COMPILER_FIELD_OFFSET(HttpServerClient, asyncSend, result.getAsync());
        SC_COMPILER_WARNING_POP
        closeAsync(requestClient);
    }
}

void SC::HttpServer::closeAsync(HttpServerClient& requestClient)
{
    if (not requestClient.asyncSend.isFree())
    {
        (void)requestClient.asyncSend.stop(*eventLoop);
    }
    if (not requestClient.asyncReceive.isFree())
    {
        (void)requestClient.asyncReceive.stop(*eventLoop);
    }
    SC_TRUST_RESULT(requestClient.socket.close());
    const bool wasFull = numClients == clients.sizeInElements();

    clients[requestClient.response.key].setFree();
    numClients--;
    if (wasFull and not stopping)
    {
        // Arena was full and in onNewClient asyncAccept has been paused (by avoiding reactivation).
        // Now a slot has been freed so it's possible to start accepting clients again.
        SC_TRUST_RESULT(asyncServerAccept.start(*eventLoop, serverSocket));
    }
}
