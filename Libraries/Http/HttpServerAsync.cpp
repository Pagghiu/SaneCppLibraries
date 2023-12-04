// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "HttpServerAsync.h"
#include "../Strings/SmallString.h"
#include "../Strings/StringBuilder.h"

// Http::ServerBase::Request
bool SC::Http::ServerBase::Request::find(Http::Parser::Result result, StringView& res) const
{
    size_t found;
    if (headerOffsets.find([result](const auto& it) { return it.result == result; }, &found))
    {
        const Header& header = headerOffsets[found];
        res = StringView(headerBuffer.data() + header.start, header.length, false, StringEncoding::Ascii);
        return true;
    }
    return false;
}

// Http::ServerBase::Response
SC::Result SC::Http::ServerBase::Response::startResponse(int code)
{
    StringBuilder sb(outputBuffer, StringEncoding::Ascii, StringBuilder::Clear);
    SC_TRY(sb.format("HTTP/1.1 "));
    switch (code)
    {
    case 200: SC_TRY(sb.append("{} OK\r\n", code)); break;
    case 404: SC_TRY(sb.append("{} Not Found\r\n", code)); break;
    case 405: SC_TRY(sb.append("{} Not Allowed\r\n", code)); break;
    }
    ended = false;
    return Result(true);
}

SC::Result SC::Http::ServerBase::Response::addHeader(StringView headerName, StringView headerValue)
{
    StringBuilder sb(outputBuffer, StringEncoding::Ascii);
    SC_TRY(sb.append(headerName));
    SC_TRY(sb.append(": "));
    SC_TRY(sb.append(headerValue));
    SC_TRY(sb.append("\r\n"));
    return Result(true);
}

SC::Result SC::Http::ServerBase::Response::end(StringView sv)
{
    StringBuilder sb(outputBuffer, StringEncoding::Ascii);
    SC_TRY(sb.append("Content-Length: {}\r\n\r\n", sv.sizeInBytes()));
    SC_TRY(sb.append(sv));
    ended = true;
    return Result(outputBuffer.pop_back()); // pop null terminator
}

// Http::Server

SC::Result SC::Http::ServerBase::parse(Span<const char> readData, ClientChannel& client)
{
    bool& parsedSuccessfully = client.request.parsedSuccessfully;
    if (client.request.headerBuffer.size() > maxHeaderSize)
    {
        parsedSuccessfully = false;
        return Result::Error("Header size exceeded limit");
    }
    SC_TRY(client.request.headerBuffer.append(readData));
    size_t readBytes;
    while (client.request.parsedSuccessfully and not readData.empty())
    {
        Http::Parser&    parser = client.request.parser;
        Span<const char> parsedData;
        parsedSuccessfully &= parser.parse(readData, readBytes, parsedData);
        parsedSuccessfully &= readData.sliceStart(readBytes, readData);
        if (parser.state == Http::Parser::State::Finished)
            break;
        if (parser.state == Http::Parser::State::Result)
        {
            Header header;
            header.result = parser.result;
            header.start  = static_cast<uint32_t>(parser.tokenStart);
            header.length = static_cast<uint32_t>(parser.tokenLength);
            parsedSuccessfully &= client.request.headerOffsets.push_back(header);
            if (parser.result == Http::Parser::Result::HeadersEnd)
            {
                client.request.headersEndReceived = true;
                SC_TRY(client.request.find(Http::Parser::Result::Url, client.request.url));
                onClient(client);
                break;
            }
        }
    }
    return Result(parsedSuccessfully);
}

// Http::ServerAsync
SC::Result SC::Http::ServerAsync::start(Async::EventLoop& eventLoop, uint32_t maxConnections, StringView address,
                                        uint16_t port)
{
    SC_TRY(requestClients.resize(maxConnections));
    SC_TRY(requests.resize(maxConnections));
    SocketIPAddress nativeAddress;
    SC_TRY(nativeAddress.fromAddressPort(address, port));
    SC_TRY(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    SC_TRY(SocketServer(serverSocket).listen(nativeAddress));
    asyncAccept.setDebugName("Http::ServerAsync");
    asyncAccept.callback.bind<Http::ServerAsync, &Http::ServerAsync::onNewClient>(*this);
    return asyncAccept.start(eventLoop, serverSocket);
}

SC::Result SC::Http::ServerAsync::stop() { return asyncAccept.stop(); }

void SC::Http::ServerAsync::onNewClient(Async::SocketAccept::Result& result)
{
    SocketDescriptor acceptedClient;
    if (not result.moveTo(acceptedClient))
    {
        // TODO: Invoke an error
        return;
    }
    bool succeeded = true;

    // TODO: do proper error handling
    auto key1 = requests.allocate();
    succeeded &= key1.isValid();
    auto key2 = requestClients.allocate();
    succeeded &= key2.isValid() and key1 == key2;
    if (succeeded)
    {
        RequestClient& client = *requestClients.get(key2);
        ClientChannel& req    = *requests.get(key1);
        client.key            = key2;
        client.socket         = move(acceptedClient);

        auto& buffer = req.request.headerBuffer;
        succeeded &= buffer.resizeWithoutInitializing(buffer.capacity());
        client.asyncReceive.setDebugName(client.debugName.bytesIncludingTerminator());
        client.asyncReceive.callback.bind<Http::ServerAsync, &Http::ServerAsync::onReceive>(*this);
        succeeded &= client.asyncReceive.start(*asyncAccept.getEventLoop(), client.socket, buffer.toSpan());
        SC_ASSERT_RELEASE(succeeded);
    }
    result.reactivateRequest(true);
}

void SC::Http::ServerAsync::onReceive(Async::SocketReceive::Result& result)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF
    RequestClient& requestClient = SC_COMPILER_FIELD_OFFSET(RequestClient, asyncReceive, result.async);
    SC_COMPILER_WARNING_POP
    SC_ASSERT_RELEASE(&requestClient.asyncReceive == &result.async);
    ClientChannel& client = *requests.get(requestClient.key.cast_to<ClientChannel>());
    Span<char>     readData;
    if (not result.moveTo(readData))
    {
        // TODO: Invoke on error
        return;
    }
    if (not Http::ServerBase::parse(readData, client))
    {
        // TODO: Invoke on error
        return;
    }
    if (client.response.mustBeFlushed())
    {
        requestClient.asyncSend.setDebugName(requestClient.debugName.bytesIncludingTerminator());

        auto outspan = client.response.outputBuffer.toSpan();
        requestClient.asyncSend.callback.bind<Http::ServerAsync, &Http::ServerAsync::onAfterSend>(*this);
        auto res = requestClient.asyncSend.start(*asyncAccept.getEventLoop(), requestClient.socket, outspan);
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

void SC::Http::ServerAsync::onAfterSend(Async::SocketSend::Result& result)
{
    if (result.isValid())
    {
        SC_COMPILER_WARNING_PUSH_OFFSETOF
        RequestClient& requestClient = SC_COMPILER_FIELD_OFFSET(RequestClient, asyncSend, result.async);
        SC_COMPILER_WARNING_POP
        auto clientKey = requestClient.key.cast_to<ClientChannel>();
        requests.get(clientKey)->response.outputBuffer.clear();
    }
    // TODO: Close socket and dispose resources
}
