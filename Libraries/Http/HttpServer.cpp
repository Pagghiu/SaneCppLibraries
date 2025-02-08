// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpServer.h"
#include "../Async/Async.h"
#include "../Containers/ArenaMap.h"
#include "../Socket/Socket.h"
#include "../Strings/String.h"
#include "../Strings/StringBuilder.h"

// HttpRequest
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
    SC_TRY(not outputBuffer.isEmpty());
    SC_TRY(outputBuffer.resizeWithoutInitializing(outputBuffer.size() - 1)); // pop null terminator
    SC_TRY(outputBuffer.append(span));
    responseEnded = true;
    return Result(true);
}

// HttpServer

struct SC::HttpServer::Internal
{
    HttpServer& server;
    Internal(HttpServer& server) : server(server)
    {
        eventLoop = nullptr;
        started   = false;
        stopping  = false;

        maxHeaderSize = 8 * 1024;
    }

    ArenaMap<HttpServerClient> clients;
    SocketDescriptor           serverSocket;
    AsyncSocketAccept          asyncServerAccept;

    uint32_t maxHeaderSize;

    bool started;
    bool stopping;

    AsyncEventLoop* eventLoop;

    void onNewClient(AsyncSocketAccept::Result& result);
    void onReceive(AsyncSocketReceive::Result& result);
    void onAfterSend(AsyncSocketSend::Result& result);
    void onCloseSocket(AsyncSocketClose::Result& result);

    void closeAsync(HttpServerClient& requestClient);

    Result parse(HttpRequest& request, const uint32_t maxHeaderSize, Span<const char> readData);
};

struct SC::HttpServerClient
{
    HttpRequest  request;
    HttpResponse response;

    SocketDescriptor   socket;
    SmallString<16>    debugName;
    AsyncSocketReceive asyncReceive;
    AsyncSocketSend    asyncSend;
    AsyncSocketClose   asyncClose;
};

SC::HttpServer::HttpServer() : internal(*reinterpret_cast<Internal*>(internalRaw))
{
    static_assert(sizeof(Internal) <= sizeof(internalRaw), "size");
    placementNew(internal, *this);
}

SC::HttpServer::~HttpServer() { internal.~Internal(); }

SC::Result SC::HttpServer::start(AsyncEventLoop& loop, uint32_t maxConcurrentRequests, StringView address,
                                 uint16_t port)
{
    SC_TRY(internal.clients.resize(maxConcurrentRequests));
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    internal.eventLoop = &loop;
    SC_TRY(internal.eventLoop->createAsyncTCPSocket(nativeAddress.getAddressFamily(), internal.serverSocket));
    SocketServer server(internal.serverSocket);
    SC_TRY(server.bind(nativeAddress));
    SC_TRY(server.listen(511));

    internal.asyncServerAccept.setDebugName("HttpServer");
    internal.asyncServerAccept.callback.bind<Internal, &Internal::onNewClient>(internal);
    SC_TRY(internal.asyncServerAccept.start(*internal.eventLoop, internal.serverSocket));
    internal.started = true;
    return Result(true);
}

SC::Result SC::HttpServer::stopAsync()
{
    if (not internal.asyncServerAccept.isFree())
    {
        SC_TRY(internal.asyncServerAccept.stop());
    }

    for (HttpServerClient& it : internal.clients)
    {
        internal.closeAsync(it);
    }
    return Result(true);
}

SC::Result SC::HttpServer::stopSync()
{
    SC_TRY(stopAsync());
    while (internal.clients.size() > 0)
    {
        SC_TRY(internal.eventLoop->runNoWait());
    }
    while (not internal.asyncServerAccept.isFree())
    {
        SC_TRY(internal.eventLoop->runNoWait());
    }
    internal.stopping = false;
    internal.started  = false;
    return Result(true);
}

bool SC::HttpServer::isStarted() const { return internal.started; }

SC::HttpRequest* SC::HttpServer::getRequest(ArenaMapKey<HttpServerClient> key) const
{
    HttpServerClient* client = internal.clients.get(key);
    return client ? &client->request : nullptr;
}

SC::HttpResponse* SC::HttpServer::getResponse(ArenaMapKey<HttpServerClient> key) const
{
    HttpServerClient* client = internal.clients.get(key);
    return client ? &client->response : nullptr;
}

SC::SocketDescriptor* SC::HttpServer::getSocket(ArenaMapKey<HttpServerClient> key) const
{
    HttpServerClient* client = internal.clients.get(key);
    return client ? &client->socket : nullptr;
}

SC::uint32_t SC::HttpServer::getMaxConcurrentRequests() const { return internal.clients.getNumAllocated(); }

SC::Result SC::HttpServer::Internal::parse(HttpRequest& request, const uint32_t maxSize, Span<const char> readData)
{
    bool& parsedSuccessfully = request.parsedSuccessfully;
    if (request.headerBuffer.size() > maxSize)
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
            detail::HttpHeaderOffset header;
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

void SC::HttpServer::Internal::onNewClient(AsyncSocketAccept::Result& result)
{
    SocketDescriptor acceptedClient;
    if (not result.moveTo(acceptedClient))
    {
        // TODO: Invoke an error
        return;
    }

    ArenaMapKey<HttpServerClient> clientKey = clients.allocate();

    // Allocate always succeds because we pause asyncAccept when the arena is full
    SC_TRUST_RESULT(clientKey.isValid());

    HttpServerClient& client = *clients.get(clientKey);

    client.response.server = &server;

    client.response.key = clientKey;
    client.socket       = move(acceptedClient);
    client.asyncSend.setDebugName(client.debugName.bytesIncludingTerminator());
    client.asyncReceive.setDebugName(client.debugName.bytesIncludingTerminator());
    client.asyncReceive.callback.bind<Internal, &Internal::onReceive>(*this);

    auto& buffer = client.request.headerBuffer;

    // TODO: This can potentially fail
    SC_TRUST_RESULT(buffer.resizeWithoutInitializing(1024));

    // This cannot fail because start reports only incorrect API usage (AsyncRequest already in use etc.)
    SC_TRUST_RESULT(client.asyncReceive.start(*eventLoop, client.socket, buffer.toSpan()));

    // Only reactivate asyncAccept if arena is not full (otherwise it's being reactivated in onCloseSocket)
    result.reactivateRequest(not clients.isFull());
}

void SC::HttpServer::Internal::onReceive(AsyncSocketReceive::Result& result)
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
    if (not parse(client.request, maxHeaderSize, readData))
    {
        // TODO: Invoke on error
        return;
    }
    if (client.request.headersEndReceived)
    {
        server.onRequest(client.request, client.response);
    }
    if (client.response.mustBeFlushed())
    {
        client.asyncSend.setDebugName(client.debugName.bytesIncludingTerminator());

        auto outspan = client.response.outputBuffer.toSpan();
        client.asyncSend.callback.bind<Internal, &Internal::onAfterSend>(*this);
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

void SC::HttpServer::Internal::onAfterSend(AsyncSocketSend::Result& result)
{
    if (result.isValid())
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF
        HttpServerClient& requestClient = SC_COMPILER_FIELD_OFFSET(HttpServerClient, asyncSend, result.getAsync());
        SC_COMPILER_WARNING_POP
        closeAsync(requestClient);
    }
}

void SC::HttpServer::Internal::closeAsync(HttpServerClient& requestClient)
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
        SC_TRUST_RESULT(requestClient.asyncClose.start(*eventLoop, requestClient.socket));
    }
}

void SC::HttpServer::Internal::onCloseSocket(AsyncSocketClose::Result& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF
    HttpServerClient& client = SC_COMPILER_FIELD_OFFSET(HttpServerClient, asyncClose, result.getAsync());
    SC_COMPILER_WARNING_POP

    const bool wasFull = clients.isFull();
    SC_TRUST_RESULT(clients.remove(client.response.key));
    if (wasFull and not stopping)
    {
        // Arena was full and in onNewClient asyncAccept has been paused (by avoiding reactivation).
        // Now a slot has been freed so it's possible to start accepting clients again.
        SC_TRUST_RESULT(asyncServerAccept.start(*eventLoop, serverSocket));
    }
}
