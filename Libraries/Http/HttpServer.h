// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Containers/ArenaMap.h"
#include "../Foundation/Objects/Function.h"
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
    struct Response
    {
        bool                   ended = false;
        SmallVector<char, 255> outputBuffer;

        size_t     highwaterMark = 255;
        ReturnCode writeHead(int code)
        {
            ended = false;
            SC_UNUSED(code);
            return true;
        }

        bool mustBeFlushed() const { return ended or outputBuffer.size() > highwaterMark; }

        ReturnCode end(StringView sv);
    };
    struct Request
    {
        bool       headersEndReceived = false;
        bool       parsedSuccessfully = true;
        HttpParser parser;

        SmallVector<char, 255>  headerBuffer;
        SmallVector<Header, 16> headerOffsets;

        [[nodiscard]] ReturnCode parse(Span<const char> readData, Response& res, HttpServer& server);
    };

    HttpServer() {}

    uint32_t maxHeaderSize = 8 * 1024;
    struct RequestResponse
    {
        Request  request;
        Response response;
    };
    ArenaMap<RequestResponse>           requests;
    Function<void(Request&, Response&)> onClient;
};

struct SC::HttpServerAsync : public HttpServer
{
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

    uint32_t          maxHeaderSize = 8 * 1024;
    EventLoop*        eventLoop     = nullptr;
    AsyncSocketAccept asyncAccept;

    HttpServerAsync() {}

    HttpServerAsync(const HttpServerAsync&)            = delete;
    HttpServerAsync& operator=(const HttpServerAsync&) = delete;

    [[nodiscard]] ReturnCode start(EventLoop& loop, uint32_t maxConnections, StringView address, uint16_t port);
    [[nodiscard]] ReturnCode stop();

  private:
    [[nodiscard]] ReturnCode startSocketAccept();

    void onNewClient(AsyncSocketAccept::Result& result);
    void onReceive(AsyncSocketReceive::Result& result);
    void onAfterSend(AsyncSocketSend::Result& result);
};
