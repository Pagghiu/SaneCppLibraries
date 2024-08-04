// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpServer.h"
#include "../Async/Async.h"
#include "../Containers/ArenaMap.h"
#include "../Strings/SmallString.h"
#include "../Strings/StringBuilder.h"

// HttpServerBase::Request
bool SC::HttpRequest::find(HttpParser::Result result, StringView& res) const
{
    size_t found;
    if (headerOffsets.find([result](const auto& it) { return it.result == result; }, &found))
    {
        const HttpHeaderOffset& header = headerOffsets[found];
        res = StringView({headerBuffer.data() + header.start, header.length}, false, StringEncoding::Ascii);
        return true;
    }
    return false;
}

// HttpResponse
SC::Result SC::HttpResponse::startResponse(int code)
{
    StringBuilder sb(outputBuffer, StringEncoding::Ascii, StringBuilder::Clear);
    SC_TRY(sb.format("HTTP/1.1 "));
    switch (code)
    {
    case 200: SC_TRY(sb.append("{} OK\r\n", code)); break;
    case 404: SC_TRY(sb.append("{} Not Found\r\n", code)); break;
    case 405: SC_TRY(sb.append("{} Not Allowed\r\n", code)); break;
    }
    responseEnded = false;
    return Result(true);
}

SC::Result SC::HttpResponse::addHeader(StringView headerName, StringView headerValue)
{
    StringBuilder sb(outputBuffer, StringEncoding::Ascii);
    SC_TRY(sb.append(headerName));
    SC_TRY(sb.append(": "));
    SC_TRY(sb.append(headerValue));
    SC_TRY(sb.append("\r\n"));
    return Result(true);
}

SC::Result SC::HttpResponse::end(Span<const char> span)
{
    StringBuilder sb(outputBuffer, StringEncoding::Ascii);
    SC_TRY(sb.append("Content-Length: {}\r\n\r\n", span.sizeInBytes()));
    SC_TRY(outputBuffer.pop_back()); // pop null terminator
    SC_TRY(outputBuffer.append(span));
    responseEnded = true;
    return Result(true);
}

SC::Result SC::HttpClientChannel::parse(const uint32_t maxHeaderSize, Span<const char> readData)
{
    bool& parsedSuccessfully = request.parsedSuccessfully;
    if (request.headerBuffer.size() > maxHeaderSize)
    {
        parsedSuccessfully = false;
        return Result::Error("Header size exceeded limit");
    }
    // SC_TRY(request.headerBuffer.append(readData));
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
            HttpHeaderOffset header;
            header.result = parser.result;
            header.start  = static_cast<uint32_t>(parser.tokenStart);
            header.length = static_cast<uint32_t>(parser.tokenLength);
            parsedSuccessfully &= request.headerOffsets.push_back(header);
            if (parser.result == HttpParser::Result::HeadersEnd)
            {
                request.headersEndReceived = true;
                SC_TRY(request.find(HttpParser::Result::Url, request.url));
                break;
            }
        }
    }
    return Result(parsedSuccessfully);
}

// HttpServer

struct SC::HttpServer::ClientSocket
{
    ArenaMap<ClientSocket>::Key key;

    SocketDescriptor   socket;
    SmallString<50>    debugName;
    AsyncSocketReceive asyncReceive;
    AsyncSocketSend    asyncSend;
    AsyncSocketClose   asyncClose;
};

struct SC::HttpServer::Internal
{
    HttpServer& server;
    Internal(HttpServer& server) : server(server) {}
    ArenaMap<ClientSocket>      clientSockets;
    ArenaMap<HttpClientChannel> clientChannels;

    SocketDescriptor  serverSocket;
    AsyncSocketAccept asyncAccept;

    void onNewClient(AsyncSocketAccept::Result& result);
    void onReceive(AsyncSocketReceive::Result& result);
    void onAfterSend(AsyncSocketSend::Result& result);
    void onCloseSocket(AsyncSocketClose::Result& result);

    void closeAsync(ClientSocket& requestClient);
};

SC::HttpServer::HttpServer()
{
    eventLoop = nullptr;
    started   = false;
    stopping  = false;

    maxHeaderSize = 8 * 1024;
    placementNew(getInternal(), *this);
}

SC::HttpServer::~HttpServer() { getInternal().~Internal(); }

SC::HttpServer::Internal& SC::HttpServer::getInternal()
{
    static_assert(sizeof(Internal) <= sizeof(internalRaw), "size");
    return *reinterpret_cast<Internal*>(internalRaw);
}

SC::Result SC::HttpServer::start(AsyncEventLoop& loop, uint32_t maxConcurrentRequests, StringView address,
                                 uint16_t port)
{
    auto& asyncAccept    = getInternal().asyncAccept;
    auto& serverSocket   = getInternal().serverSocket;
    auto& clientSockets  = getInternal().clientSockets;
    auto& clientChannels = getInternal().clientChannels;
    SC_TRY(clientSockets.resize(maxConcurrentRequests));
    SC_TRY(clientChannels.resize(maxConcurrentRequests));
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    eventLoop = &loop;
    SC_TRY(eventLoop->createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    SocketServer server(serverSocket);
    SC_TRY(server.bind(nativeAddress));
    SC_TRY(server.listen(511));

    asyncAccept.setDebugName("HttpServer");
    asyncAccept.callback.bind<Internal, &Internal::onNewClient>(getInternal());
    SC_TRY(asyncAccept.start(*eventLoop, serverSocket));
    started = true;
    return Result(true);
}

SC::Result SC::HttpServer::stopAsync()
{
    auto& asyncAccept = getInternal().asyncAccept;
    if (not asyncAccept.isFree())
    {
        SC_TRY(asyncAccept.stop());
    }

    for (ClientSocket& it : getInternal().clientSockets)
    {
        getInternal().closeAsync(it);
    }
    return Result(true);
}

SC::Result SC::HttpServer::stopSync()
{
    SC_TRY(stopAsync());
    Internal& internal = getInternal();
    while (internal.clientSockets.size() > 0)
    {
        SC_TRY(eventLoop->runNoWait());
    }
    while(not internal.asyncAccept.isFree())
    {
        SC_TRY(eventLoop->runNoWait());
    }
    stopping = false;
    started  = false;
    return Result(true);
}

void SC::HttpServer::Internal::onNewClient(AsyncSocketAccept::Result& result)
{
    SocketDescriptor acceptedClient;
    if (not result.moveTo(acceptedClient))
    {
        // TODO: Invoke an error
        return;
    }

    ArenaMapKey<HttpClientChannel> channelKey = clientChannels.allocate();
    ArenaMapKey<ClientSocket>      socketKey  = clientSockets.allocate();

    // Allocate always succeds because we pause asyncAccept when the two arenas are full
    SC_TRUST_RESULT(channelKey.isValid() and socketKey.isValid());

    ClientSocket& socket = *clientSockets.get(socketKey);

    socket.key    = socketKey;
    socket.socket = move(acceptedClient);
    socket.asyncSend.setDebugName(socket.debugName.bytesIncludingTerminator());
    socket.asyncReceive.setDebugName(socket.debugName.bytesIncludingTerminator());
    socket.asyncReceive.callback.bind<Internal, &Internal::onReceive>(*this);

    HttpClientChannel& channel = *clientChannels.get(channelKey);
    Vector<char>&      buffer  = channel.request.headerBuffer;

    // TODO: This can potentially fail
    SC_TRUST_RESULT(buffer.resizeWithoutInitializing(1024));

    // This cannot fail because start reports only incorrect API usage (AsyncRequest already in use etc.)
    SC_TRUST_RESULT(socket.asyncReceive.start(*server.eventLoop, socket.socket, buffer.toSpan()));

    // Only reactivate asyncAccept if arena is not full (otherwise it's being reactivated in onCloseSocket)
    result.reactivateRequest(not clientChannels.isFull());
}

void SC::HttpServer::Internal::onReceive(AsyncSocketReceive::Result& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF
    ClientSocket& requestClient = SC_COMPILER_FIELD_OFFSET(ClientSocket, asyncReceive, result.getAsync());
    SC_COMPILER_WARNING_POP
    SC_ASSERT_RELEASE(&requestClient.asyncReceive == &result.getAsync());
    HttpClientChannel& client = *clientChannels.get(requestClient.key.cast_to<HttpClientChannel>());
    Span<char>         readData;
    if (not result.get(readData))
    {
        // TODO: Invoke on error
        return;
    }
    if (not client.parse(server.maxHeaderSize, readData))
    {
        // TODO: Invoke on error
        return;
    }
    if (client.request.headersEndReceived)
    {
        server.onClient(client);
    }
    if (client.response.mustBeFlushed())
    {
        requestClient.asyncSend.setDebugName(requestClient.debugName.bytesIncludingTerminator());

        auto outspan = client.response.outputBuffer.toSpan();
        requestClient.asyncSend.callback.bind<Internal, &Internal::onAfterSend>(*this);
        auto res = requestClient.asyncSend.start(*server.eventLoop, requestClient.socket, outspan);
        if (not res)
        {
            // TODO: Invoke on error
            return;
        }
    }
    else
    {
        result.reactivateRequest(true);
    }
}

void SC::HttpServer::Internal::onAfterSend(AsyncSocketSend::Result& result)
{
    if (result.isValid())
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF
        ClientSocket& requestClient = SC_COMPILER_FIELD_OFFSET(ClientSocket, asyncSend, result.getAsync());
        SC_COMPILER_WARNING_POP
        closeAsync(requestClient);
    }
}

void SC::HttpServer::Internal::closeAsync(ClientSocket& requestClient)
{
    if (not requestClient.asyncSend.isFree())
    {
        (void)requestClient.asyncSend.stop();
    }
    if (not requestClient.asyncReceive.isFree())
    {
        (void)requestClient.asyncReceive.stop();
    }
    requestClient.asyncClose.callback.bind<Internal, &Internal::onCloseSocket>(*this);

    if (requestClient.asyncClose.isFree())
    {
        SC_TRUST_RESULT(requestClient.asyncClose.start(*server.eventLoop, requestClient.socket));
    }
}

void SC::HttpServer::Internal::onCloseSocket(AsyncSocketClose::Result& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF
    ClientSocket& requestClient = SC_COMPILER_FIELD_OFFSET(ClientSocket, asyncClose, result.getAsync());
    SC_COMPILER_WARNING_POP
    ArenaMapKey<HttpClientChannel> clientKey = requestClient.key.cast_to<HttpClientChannel>();

    const bool wasFull = clientChannels.isFull();
    SC_TRUST_RESULT(clientChannels.remove(clientKey));
    SC_TRUST_RESULT(clientSockets.remove(requestClient.key));
    if (wasFull and not server.stopping)
    {
        // Arena was full and in onNewClient asyncAccept has been paused (by avoiding reactivation).
        // Now a slot has been freed so it's possible to start accepting clients again.
        SC_TRUST_RESULT(asyncAccept.start(*server.eventLoop, serverSocket));
    }
}
