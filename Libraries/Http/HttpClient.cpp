// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "HttpClient.h"

#include "../Foundation/StringBuilder.h"

SC::ReturnCode SC::HttpClient::start(EventLoop& loop, StringView ipAddress, uint16_t port, StringView requestContent,
                                     Function<void(HttpClient&)>&& cb)
{
    eventLoop = &loop;
    callback  = move(cb);

    SocketIPAddress localHost;
    SC_TRY_IF(localHost.fromAddressPort(ipAddress, port));
    SC_TRY_IF(eventLoop->createAsyncTCPSocket(localHost.getAddressFamily(), clientSocket));

    StringBuilder sb(content, StringEncoding::Ascii, StringBuilder::Clear);
    SC_TRY_IF(sb.append(requestContent));
    connectAsync.debugName = customDebugName.isEmpty() ? "HttpClient" : customDebugName.bytesIncludingTerminator();
    SC_TRY_IF((eventLoop->startSocketConnect(connectAsync, clientSocket, localHost,
                                             SC_FUNCTION_MEMBER(&HttpClient::onConnected, this))));
    return true;
}

SC::StringView SC::HttpClient::getResponse() const
{
    return StringView(content.data(), content.size(), false, StringEncoding::Ascii);
}

void SC::HttpClient::onConnected(AsyncSocketConnectResult& result)
{
    SC_UNUSED(result);
    sendAsync.debugName =
        customDebugName.isEmpty() ? "HttpClient::clientSocket" : customDebugName.bytesIncludingTerminator();

    auto res = eventLoop->startSocketSend(sendAsync, clientSocket, content.toSpanConst(),
                                          SC_FUNCTION_MEMBER(&HttpClient::onAfterSend, this));
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::HttpClient::onAfterSend(AsyncSocketSendResult& result)
{
    SC_UNUSED(result);
    SC_RELEASE_ASSERT(content.resizeWithoutInitializing(content.capacity()));

    receiveAsync.debugName =
        customDebugName.isEmpty() ? "HttpClient::clientSocket" : customDebugName.bytesIncludingTerminator();

    auto res = eventLoop->startSocketReceive(receiveAsync, clientSocket, content.toSpan(),
                                             SC_FUNCTION_MEMBER(&HttpClient::onAfterRead, this));
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::HttpClient::onAfterRead(AsyncSocketReceiveResult& result)
{
    SC_UNUSED(result);
    SC_RELEASE_ASSERT(SocketClient(clientSocket).close());
    callback(*this);
}
