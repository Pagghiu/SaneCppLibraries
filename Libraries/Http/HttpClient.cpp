// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpClient.h"
#include "HttpURLParser.h"
#include "Internal/HttpStringAppend.h"

SC::Result SC::HttpClient::get(AsyncEventLoop& loop, StringSpan url)
{
    eventLoop = &loop;

    uint16_t      port;
    HttpURLParser parser;
    SC_TRY(parser.parse(url));
    SC_TRY_MSG(parser.protocol == "http", "Invalid protocol");
    // TODO: Make DNS Resolution asynchronous
    char       buffer[256];
    Span<char> ipAddress = {buffer};
    SC_TRY(SocketDNS::resolveDNS(parser.hostname, ipAddress))
    port = parser.port;
    SocketIPAddress localHost;
    SC_TRY(localHost.fromAddressPort({ipAddress, true, StringEncoding::Ascii}, port));
    SC_TRY(eventLoop->createAsyncTCPSocket(localHost.getAddressFamily(), clientSocket));

    {
        GrowableBuffer<decltype(content)> gb = {content};
        HttpStringAppend&                 sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));

        sb.clear();
        SC_TRY(sb.append("GET "));
        SC_TRY(sb.append(parser.path));
        SC_TRY(sb.append(" HTTP/1.1\r\n"));
        SC_TRY(sb.append("User-agent: SC\r\n"));
        SC_TRY(sb.append("Host: 127.0.0.1\r\n\r\n"));
    }

    const char* dbgName = customDebugName.isEmpty() ? "HttpClient" : customDebugName.bytesIncludingTerminator();
    connectAsync.setDebugName(dbgName);
    connectAsync.callback.bind<HttpClient, &HttpClient::onConnected>(*this);
    return connectAsync.start(*eventLoop, clientSocket, localHost);
}

SC::StringSpan SC::HttpClient::getResponse() const
{
    return StringSpan(content.toSpanConst(), false, StringEncoding::Ascii);
}

void SC::HttpClient::onConnected(AsyncSocketConnect::Result& result)
{
    SC_COMPILER_UNUSED(result);
    const char* dbgName =
        customDebugName.isEmpty() ? "HttpClient::clientSocket" : customDebugName.bytesIncludingTerminator();
    sendAsync.setDebugName(dbgName);

    sendAsync.callback.bind<HttpClient, &HttpClient::onAfterSend>(*this);
    auto res = sendAsync.start(*eventLoop, clientSocket, content.toSpanConst());
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::HttpClient::onAfterSend(AsyncSocketSend::Result& result)
{
    SC_COMPILER_UNUSED(result);
    SC_ASSERT_RELEASE(content.resizeWithoutInitializing(content.capacity()));

    const char* dbgName =
        customDebugName.isEmpty() ? "HttpClient::clientSocket" : customDebugName.bytesIncludingTerminator();
    receiveAsync.setDebugName(dbgName);

    receiveAsync.callback.bind<HttpClient, &HttpClient::onAfterRead>(*this);
    auto res = receiveAsync.start(*eventLoop, clientSocket, content.toSpan());
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::HttpClient::onAfterRead(AsyncSocketReceive::Result& result)
{
    // TODO: parse response and re-arm receive to read it entirely
    SC_ASSERT_RELEASE(clientSocket.close());
    if (not result.completionData.disconnected)
    {
        callback(*this);
    }
}
