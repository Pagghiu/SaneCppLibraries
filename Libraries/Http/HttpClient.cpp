// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "HttpClient.h"

#include "../Foundation/Strings/StringBuilder.h"

SC::ReturnCode SC::HttpClient::start(EventLoop& loop, StringView ipAddress, uint16_t port, StringView requestContent)
{
    eventLoop = &loop;

    SocketIPAddress localHost;
    SC_TRY(localHost.fromAddressPort(ipAddress, port));
    SC_TRY(eventLoop->createAsyncTCPSocket(localHost.getAddressFamily(), clientSocket));

    StringBuilder sb(content, StringEncoding::Ascii, StringBuilder::Clear);
    SC_TRY(sb.append(requestContent));
    const char* dbgName = customDebugName.isEmpty() ? "HttpClient" : customDebugName.bytesIncludingTerminator();
    connectAsync.setDebugName(dbgName);
    connectAsync.callback.bind<HttpClient, &HttpClient::onConnected>(this);
    return connectAsync.start(*eventLoop, clientSocket, localHost);
}

SC::StringView SC::HttpClient::getResponse() const
{
    return StringView(content.data(), content.size(), false, StringEncoding::Ascii);
}

void SC::HttpClient::onConnected(AsyncSocketConnect::Result& result)
{
    SC_UNUSED(result);
    const char* dbgName =
        customDebugName.isEmpty() ? "HttpClient::clientSocket" : customDebugName.bytesIncludingTerminator();
    sendAsync.setDebugName(dbgName);

    sendAsync.callback.bind<HttpClient, &HttpClient::onAfterSend>(this);
    auto res = sendAsync.start(*eventLoop, clientSocket, content.toSpanConst());
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::HttpClient::onAfterSend(AsyncSocketSend::Result& result)
{
    SC_UNUSED(result);
    SC_RELEASE_ASSERT(content.resizeWithoutInitializing(content.capacity()));

    const char* dbgName =
        customDebugName.isEmpty() ? "HttpClient::clientSocket" : customDebugName.bytesIncludingTerminator();
    receiveAsync.setDebugName(dbgName);

    receiveAsync.callback.bind<HttpClient, &HttpClient::onAfterRead>(this);
    auto res = receiveAsync.start(*eventLoop, clientSocket, content.toSpan());
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::HttpClient::onAfterRead(AsyncSocketReceive::Result& result)
{
    SC_UNUSED(result);
    SC_RELEASE_ASSERT(SocketClient(clientSocket).close());
    callback(*this);
}
