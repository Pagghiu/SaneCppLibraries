// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpClient.h"
#include "Libraries/Http/HttpURLParser.h"
#include "Libraries/Http/Internal/HttpStringAppend.h"
#include <stdio.h> // snprintf

SC::Result SC::HttpClient::get(AsyncEventLoop& loop, StringSpan url, bool keepOpen)
{
    eventLoop          = &loop;
    keepConnectionOpen = keepOpen;

    // Parse URL
    HttpURLParser urlParser;
    SC_TRY(urlParser.parse(url));
    SC_TRY_MSG(urlParser.protocol == "http", "Invalid protocol");

    // Reset state for new request
    parser          = {};
    parser.type     = HttpParser::Type::Response;
    receivedBytes   = 0;
    parsedBytes     = 0;
    contentLen      = 0;
    headersReceived = false;

    {
        GrowableBuffer<decltype(content)> gb = {content};

        HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
        sb.clear();
        SC_TRY(sb.append("GET "));
        SC_TRY(sb.append(urlParser.path));
        SC_TRY(sb.append(" HTTP/1.1\r\n"));
        SC_TRY(sb.append("User-agent: SC\r\n"));
        SC_TRY(sb.append("Host: 127.0.0.1\r\n"));
        if (keepConnectionOpen)
        {
            SC_TRY(sb.append("Connection: keep-alive\r\n"));
        }
        SC_TRY(sb.append("\r\n"));
        headerBytes = content.size();
    }

    // If we don't have an active connection or we're not keeping connections open, create a new one
    if (not hasActiveConnection or not keepConnectionOpen)
    {
        uint16_t port;
        // TODO: Make DNS Resolution asynchronous
        char       buffer[256];
        Span<char> ipAddress = {buffer};
        SC_TRY(SocketDNS::resolveDNS(urlParser.hostname, ipAddress))
        port = urlParser.port;
        SocketIPAddress localHost;
        SC_TRY(localHost.fromAddressPort({ipAddress, true, StringEncoding::Ascii}, port));
        SC_TRY(eventLoop->createAsyncTCPSocket(localHost.getAddressFamily(), clientSocket));
        hasActiveConnection = true;

        connectAsync.callback.bind<HttpClient, &HttpClient::startSendingHeaders>(*this);
        return connectAsync.start(*eventLoop, clientSocket, localHost);
    }
    else
    {
        // Reuse existing connection - go directly to sending headers
        startSendingHeadersOnExistingConnection();
        return Result(true);
    }
}

SC::Result SC::HttpClient::put(AsyncEventLoop& loop, StringSpan url, StringSpan body, TimeMs delay)
{
    bodyDelay = delay;
    eventLoop = &loop;

    uint16_t      port;
    HttpURLParser urlParser;
    SC_TRY(urlParser.parse(url));
    SC_TRY_MSG(urlParser.protocol == "http", "Invalid protocol");
    // TODO: Make DNS Resolution asynchronous
    char       buffer[256];
    Span<char> ipAddress = {buffer};
    SC_TRY(SocketDNS::resolveDNS(urlParser.hostname, ipAddress))
    port = urlParser.port;
    SocketIPAddress localHost;
    SC_TRY(localHost.fromAddressPort({ipAddress, true, StringEncoding::Ascii}, port));
    SC_TRY(eventLoop->createAsyncTCPSocket(localHost.getAddressFamily(), clientSocket));

    {
        GrowableBuffer<decltype(content)> gb = {content};

        HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
        sb.clear();
        SC_TRY(sb.append("PUT "));
        SC_TRY(sb.append(urlParser.path));
        SC_TRY(sb.append(" HTTP/1.1\r\n"));
        SC_TRY(sb.append("User-agent: SC\r\n"));
        SC_TRY(sb.append("Host: 127.0.0.1\r\n"));
        char contentLengthBuffer[32];
        ::snprintf(contentLengthBuffer, sizeof(contentLengthBuffer), "%zu", body.sizeInBytes());
        StringSpan cl({contentLengthBuffer, ::strlen(contentLengthBuffer)}, false, StringEncoding::Ascii);
        SC_TRY(sb.append("Content-Length: "));
        SC_TRY(sb.append(cl));
        SC_TRY(sb.append("\r\n\r\n"));
        headerBytes = content.size();
        SC_TRY(sb.append(body));
    }

    connectAsync.callback.bind<HttpClient, &HttpClient::startSendingHeaders>(*this);
    parser      = {};
    parser.type = HttpParser::Type::Response;
    return connectAsync.start(*eventLoop, clientSocket, localHost);
}

SC::Result SC::HttpClient::postMultipart(AsyncEventLoop& loop, StringSpan url, StringSpan fieldName,
                                         StringSpan fileName, StringSpan fileContent, TimeMs delay)
{
    bodyDelay = delay;
    eventLoop = &loop;

    uint16_t      port;
    HttpURLParser urlParser;
    SC_TRY(urlParser.parse(url));
    SC_TRY_MSG(urlParser.protocol == "http", "Invalid protocol");
    // TODO: Make DNS Resolution asynchronous
    char       buffer[256];
    Span<char> ipAddress = {buffer};
    SC_TRY(SocketDNS::resolveDNS(urlParser.hostname, ipAddress))
    port = urlParser.port;
    SocketIPAddress localHost;
    SC_TRY(localHost.fromAddressPort({ipAddress, true, StringEncoding::Ascii}, port));
    SC_TRY(eventLoop->createAsyncTCPSocket(localHost.getAddressFamily(), clientSocket));

    // Generate a unique boundary
    StringSpan boundary    = "----SCFormBoundary7MA4YWxkTrZu0gW";
    size_t     boundaryLen = boundary.sizeInBytes();

    {
        GrowableBuffer<decltype(content)> gb = {content};

        HttpStringAppend& sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
        sb.clear();

        // Build the multipart body first to know content length
        // Body format:
        // --boundary\r\n
        // Content-Disposition: form-data; name="fieldName"; filename="fileName"\r\n
        // Content-Type: application/octet-stream\r\n
        // \r\n
        // fileContent
        // \r\n--boundary--\r\n

        // Calculate body size
        size_t bodySize = 0;
        bodySize += 2 + boundaryLen + 2;                                            // --boundary\r\n
        bodySize += 38 + fieldName.sizeInBytes() + 13 + fileName.sizeInBytes() + 3; // Content-Disposition: ...\r\n
        bodySize += 40;                        // Content-Type: application/octet-stream\r\n
        bodySize += 2;                         // \r\n
        bodySize += fileContent.sizeInBytes(); // file content
        bodySize += 4 + boundaryLen + 4;       // \r\n--boundary--\r\n

        // Build HTTP headers
        SC_TRY(sb.append("POST "));
        SC_TRY(sb.append(urlParser.path));
        SC_TRY(sb.append(" HTTP/1.1\r\n"));
        SC_TRY(sb.append("User-agent: SC\r\n"));
        SC_TRY(sb.append("Host: 127.0.0.1\r\n"));
        SC_TRY(sb.append("Content-Type: multipart/form-data; boundary="));
        SC_TRY(sb.append(boundary));
        SC_TRY(sb.append("\r\n"));

        char contentLengthBuffer[32];
        ::snprintf(contentLengthBuffer, sizeof(contentLengthBuffer), "%zu", bodySize);
        StringSpan cl({contentLengthBuffer, ::strlen(contentLengthBuffer)}, false, StringEncoding::Ascii);
        SC_TRY(sb.append("Content-Length: "));
        SC_TRY(sb.append(cl));
        SC_TRY(sb.append("\r\n\r\n"));

        // Build multipart body
        SC_TRY(sb.append("--"));
        SC_TRY(sb.append(boundary));
        SC_TRY(sb.append("\r\n"));
        SC_TRY(sb.append("Content-Disposition: form-data; name=\""));
        SC_TRY(sb.append(fieldName));
        headerBytes = content.size(); // break it in the middle
        SC_TRY(sb.append("\"; filename=\""));
        SC_TRY(sb.append(fileName));
        SC_TRY(sb.append("\"\r\n"));
        SC_TRY(sb.append("Content-Type: application/octet-stream\r\n"));
        SC_TRY(sb.append("\r\n"));
        SC_TRY(sb.append(fileContent));
        SC_TRY(sb.append("\r\n--"));
        SC_TRY(sb.append(boundary));
        SC_TRY(sb.append("--\r\n"));
    }

    connectAsync.callback.bind<HttpClient, &HttpClient::startSendingHeaders>(*this);
    parser      = {};
    parser.type = HttpParser::Type::Response;
    return connectAsync.start(*eventLoop, clientSocket, localHost);
}

SC::StringSpan SC::HttpClient::getResponse() const
{
    return StringSpan(content.toSpanConst(), false, StringEncoding::Ascii);
}

void SC::HttpClient::startSendingHeaders(AsyncSocketConnect::Result& result)
{
    SC_COMPILER_UNUSED(result);

    Span<const char> toSend;
    if (headerBytes < content.size() and bodyDelay.milliseconds > 0)
    {
        // Sending out the body in a separate async send after delay just for testing purposes
        SC_ASSERT_RELEASE(content.toSpanConst().sliceStartLength(0, headerBytes, toSend));
        sendAsync.callback.bind<HttpClient, &HttpClient::startWaiting>(*this);
    }
    else
    {
        // Send it all
        toSend = content.toSpanConst();
        sendAsync.callback.bind<HttpClient, &HttpClient::startReceiveResponse>(*this);
    }
    auto res = sendAsync.start(*eventLoop, clientSocket, toSend);
    SC_ASSERT_RELEASE(res);
}

void SC::HttpClient::startSendingHeadersOnExistingConnection()
{
    Span<const char> toSend;
    if (headerBytes < content.size() and bodyDelay.milliseconds > 0)
    {
        // Sending out the body in a separate async send after delay just for testing purposes
        SC_ASSERT_RELEASE(content.toSpanConst().sliceStartLength(0, headerBytes, toSend));
        sendAsync.callback.bind<HttpClient, &HttpClient::startWaiting>(*this);
    }
    else
    {
        // Send it all
        toSend = content.toSpanConst();
        sendAsync.callback.bind<HttpClient, &HttpClient::startReceiveResponse>(*this);
    }
    auto res = sendAsync.start(*eventLoop, clientSocket, toSend);
    SC_ASSERT_RELEASE(res);
}

void SC::HttpClient::startWaiting(AsyncSocketSend::Result&)
{
    timeoutAsync.callback.bind<HttpClient, &HttpClient::startSendingBody>(*this);
    auto res = timeoutAsync.start(*eventLoop, bodyDelay); // 10 ms delay
    SC_ASSERT_RELEASE(res);
}

void SC::HttpClient::startSendingBody(AsyncLoopTimeout::Result&)
{
    sendAsync.callback.bind<HttpClient, &HttpClient::startReceiveResponse>(*this);
    Span<const char> bodySpan;
    SC_ASSERT_RELEASE(content.toSpanConst().sliceStart(headerBytes, bodySpan));
    auto res = sendAsync.start(*eventLoop, clientSocket, bodySpan);
    SC_ASSERT_RELEASE(res);
}

void SC::HttpClient::startReceiveResponse(AsyncSocketSend::Result& result)
{
    SC_COMPILER_UNUSED(result);
    SC_ASSERT_RELEASE(content.resizeWithoutInitializing(1024));

    receivedBytes   = 0;
    headersReceived = false;
    receiveAsync.callback.bind<HttpClient, &HttpClient::tryParseResponse>(*this);
    auto res = receiveAsync.start(*eventLoop, clientSocket, content.toSpan());
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::HttpClient::tryParseResponse(AsyncSocketReceive::Result& result)
{
    receivedBytes += result.completionData.numBytes;
    SC_ASSERT_RELEASE(content.resize(receivedBytes));
    Span<const char> readData;
    SC_ASSERT_RELEASE(content.toSpanConst().sliceStart(parsedBytes, readData));
    bool parsedSuccessfully = true;
    if (not headersReceived)
    {
        size_t readBytes;
        while (parsedSuccessfully and not readData.empty())
        {
            Span<const char> parsedData;
            parsedSuccessfully &= parser.parse(readData, readBytes, parsedData);
            parsedSuccessfully &= readData.sliceStart(readBytes, readData);
            parsedBytes += readBytes;
            if (parser.state == HttpParser::State::Result and parser.token == HttpParser::Token::HeaderValue)
            {
                if (parser.matchesHeader(HttpParser::HeaderType::ContentLength))
                {
                    contentLen = static_cast<size_t>(parser.contentLength);
                }
            }
            if (parser.token == HttpParser::Token::HeadersEnd)
            {
                headersReceived = true;
                break;
            }
        }
    }
    if (content.size() == parsedBytes + contentLen)
    {
        // If we're not keeping the connection open, or if the server closed the connection, close it
        if (not keepConnectionOpen or result.completionData.disconnected)
        {
            SC_ASSERT_RELEASE(clientSocket.close());
            hasActiveConnection = false;
        }
        // Call the callback regardless
        callback(*this);
    }
    else
    {
        SC_ASSERT_RELEASE(content.reserve(receivedBytes + 1024));
        SC_ASSERT_RELEASE(content.resize(content.capacity()));
        SC_ASSERT_RELEASE(content.toSpan().sliceStart(receivedBytes, receiveAsync.buffer));
        if (parsedSuccessfully)
        {
            result.reactivateRequest(true);
        }
        else
        {
            // TODO: raise error
        }
    }
}
