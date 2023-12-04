// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "HttpClientAsync.h"
#include "../Socket/SocketDescriptor.h" // DNSResolver
#include "HttpURLParser.h"

#include "../Strings/SmallString.h"
#include "../Strings/StringBuilder.h"

SC::Result SC::Http::ClientAsync::get(Async::EventLoop& loop, StringView url)
{
    eventLoop = &loop;

    SmallString<256> ipAddress;
    uint16_t         port;
    Http::URLParser  parser;
    SC_TRY(parser.parse(url));
    SC_TRY_MSG(parser.protocol == "http", "Invalid protocol");
    // TODO: Make DNS Resolution asynchronous
    SC_TRY(DNSResolver::resolve(parser.hostname, ipAddress))
    port = parser.port;
    SocketIPAddress localHost;
    SC_TRY(localHost.fromAddressPort(ipAddress.view(), port));
    SC_TRY(eventLoop->createAsyncTCPSocket(localHost.getAddressFamily(), clientSocket));

    StringBuilder sb(content, StringEncoding::Ascii, StringBuilder::Clear);

    SC_TRY(sb.append("GET {} HTTP/1.1\r\n"
                     "User-agent: {}\r\n"
                     "Host: {}\r\n\r\n",
                     parser.path, "SC", "127.0.0.1"));
    const char* dbgName = customDebugName.isEmpty() ? "Http::ClientAsync" : customDebugName.bytesIncludingTerminator();
    connectAsync.setDebugName(dbgName);
    connectAsync.callback.bind<Http::ClientAsync, &Http::ClientAsync::onConnected>(*this);
    return connectAsync.start(*eventLoop, clientSocket, localHost);
}

SC::StringView SC::Http::ClientAsync::getResponse() const
{
    return StringView(content.data(), content.size(), false, StringEncoding::Ascii);
}

void SC::Http::ClientAsync::onConnected(Async::SocketConnect::Result& result)
{
    SC_COMPILER_UNUSED(result);
    const char* dbgName =
        customDebugName.isEmpty() ? "Http::ClientAsync::clientSocket" : customDebugName.bytesIncludingTerminator();
    sendAsync.setDebugName(dbgName);

    sendAsync.callback.bind<Http::ClientAsync, &Http::ClientAsync::onAfterSend>(*this);
    auto res = sendAsync.start(*eventLoop, clientSocket, content.toSpanConst());
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::Http::ClientAsync::onAfterSend(Async::SocketSend::Result& result)
{
    SC_COMPILER_UNUSED(result);
    SC_ASSERT_RELEASE(content.resizeWithoutInitializing(content.capacity()));

    const char* dbgName =
        customDebugName.isEmpty() ? "Http::ClientAsync::clientSocket" : customDebugName.bytesIncludingTerminator();
    receiveAsync.setDebugName(dbgName);

    receiveAsync.callback.bind<Http::ClientAsync, &Http::ClientAsync::onAfterRead>(*this);
    auto res = receiveAsync.start(*eventLoop, clientSocket, content.toSpan());
    if (not res)
    {
        // TODO: raise error
    }
}

void SC::Http::ClientAsync::onAfterRead(Async::SocketReceive::Result& result)
{
    SC_COMPILER_UNUSED(result);
    SC_ASSERT_RELEASE(SocketClient(clientSocket).close());
    callback(*this);
}
