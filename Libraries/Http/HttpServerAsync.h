// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/ArenaMap.h"
#include "../Foundation/Function.h"
#include "HttpParser.h"

#include "../Async/Async.h"
#include "../Containers/SmallVector.h"
#include "../Socket/SocketDescriptor.h"
#include "../Strings/SmallString.h"

namespace SC
{
namespace Http
{
struct ServerBase;
struct ServerAsync;
} // namespace Http
} // namespace SC

//! @addtogroup group_http
//! @{

/// @brief Http server common logic
struct SC::Http::ServerBase
{
    /// @brief Http header
    struct Header
    {
        Http::Parser::Result result = Http::Parser::Result::Method;

        uint32_t start  = 0;
        uint32_t length = 0;
    };
    struct ClientChannel;

    /// @brief Http request
    struct Request
    {
        bool headersEndReceived = false; ///< All headers have been received
        bool parsedSuccessfully = true;  ///< Request headers have been parsed successfully

        Http::Parser parser; ///< The parser used to parse headers
        StringView   url;    ///< The url extracted from parsed headers

        SmallVector<char, 255>  headerBuffer;  ///< Buffer containing all headers
        SmallVector<Header, 16> headerOffsets; ///< Headers, defined as offsets in headerBuffer

        /// @brief Finds a specific Http::Parser::Result in the list of parsed header
        /// @param result The result to look for (Method, Url etc.)
        /// @param res A StringView, pointing at headerBuffer containing the found result
        /// @return `true` if the result has been found
        [[nodiscard]] bool find(Http::Parser::Result result, StringView& res) const;
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

/// @brief Http server using Async library
struct SC::Http::ServerAsync : public Http::ServerBase
{
    ServerAsync() {}
    ServerAsync(const ServerAsync&)            = delete;
    ServerAsync& operator=(const ServerAsync&) = delete;

    /// @brief Starts the http server on the given Async::EventLoop, address and port
    /// @param loop The event loop to be used, where to add the listening socket
    /// @param maxConnections Maximum number of concurrent listnening connections
    /// @param address The address of local interface where to listen to
    /// @param port The local port where to start listening to
    /// @return Valid Result if http listening has been started successfully
    [[nodiscard]] Result start(Async::EventLoop& loop, uint32_t maxConnections, StringView address, uint16_t port);

    /// @brief Stops the http server
    /// @return Valid Result if server has been stopped successfully
    [[nodiscard]] Result stop();

  private:
    struct RequestClient
    {
        ArenaMap<RequestClient>::Key key;

        SocketDescriptor     socket;
        SmallString<50>      debugName;
        Async::SocketReceive asyncReceive;
        Async::SocketSend    asyncSend;
    };
    ArenaMap<RequestClient> requestClients;
    SocketDescriptor        serverSocket;

    Async::SocketAccept asyncAccept;

    void onNewClient(Async::SocketAccept::Result& result);
    void onReceive(Async::SocketReceive::Result& result);
    void onAfterSend(Async::SocketSend::Result& result);
};

//! @}
