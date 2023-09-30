// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Containers/ArenaMap.h"
#include "../Foundation/Language/Function.h"
#include "HttpParser.h"

#include "../Async/EventLoop.h"
#include "../Foundation/Containers/SmallVector.h"
#include "../Foundation/Strings/String.h"
#include "../Socket/SocketDescriptor.h"

namespace SC
{
struct HttpServer;
struct HttpServerAsync;
} // namespace SC

struct SC::HttpServer
{
    struct Header
    {
        HttpParser::Result result = HttpParser::Result::Method;

        uint32_t start  = 0;
        uint32_t length = 0;
    };
    struct ClientChannel;
    struct Request
    {
        bool headersEndReceived = false;
        bool parsedSuccessfully = true;

        HttpParser parser;
        StringView url;

        SmallVector<char, 255>  headerBuffer;
        SmallVector<Header, 16> headerOffsets;

        [[nodiscard]] bool find(HttpParser::Result result, StringView& res) const;
    };

    struct Response
    {
        bool                   ended = false;
        SmallVector<char, 255> outputBuffer;
        size_t                 highwaterMark = 255;

        [[nodiscard]] Result startResponse(int code);
        [[nodiscard]] Result addHeader(StringView headerName, StringView headerValue);
        [[nodiscard]] bool   mustBeFlushed() const { return ended or outputBuffer.size() > highwaterMark; }

        [[nodiscard]] Result end(StringView sv);
    };

    uint32_t maxHeaderSize = 8 * 1024;
    struct ClientChannel
    {
        Request  request;
        Response response;
    };
    ArenaMap<ClientChannel>        requests;
    Function<void(ClientChannel&)> onClient;

  protected:
    [[nodiscard]] Result parse(Span<const char> readData, ClientChannel& res);
};

struct SC::HttpServerAsync : public HttpServer
{
    HttpServerAsync() {}
    HttpServerAsync(const HttpServerAsync&)            = delete;
    HttpServerAsync& operator=(const HttpServerAsync&) = delete;

    [[nodiscard]] Result start(EventLoop& loop, uint32_t maxConnections, StringView address, uint16_t port);
    [[nodiscard]] Result stop();

  private:
    struct RequestClient
    {
        ArenaMap<RequestClient>::Key key;

        SocketDescriptor   socket;
        SmallString<50>    debugName;
        AsyncSocketReceive asyncReceive;
        AsyncSocketSend    asyncSend;
    };
    ArenaMap<RequestClient> requestClients;
    SocketDescriptor        serverSocket;

    AsyncSocketAccept asyncAccept;

    void onNewClient(AsyncSocketAccept::Result& result);
    void onReceive(AsyncSocketReceive::Result& result);
    void onAfterSend(AsyncSocketSend::Result& result);
};
