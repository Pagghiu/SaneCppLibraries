// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "HttpServer.h"
#include "../Foundation/StringBuilder.h"
namespace SC
{
template <int offset, typename T, typename R>
inline T& fieldOffset(R& object)
{
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(&object) - offset);
}
} // namespace SC

#define SC_FIELD_OFFSET(Class, Field, Value)                                                                           \
    fieldOffset<SC_OFFSETOF(Class, Field), Class, decltype(Class::Field)>(Value);

SC::ReturnCode SC::HttpServer::Response::end(StringView sv)
{
    const StringView headers = "HTTP/1.1 200 OK\r\n"
                               "Date: Mon, 27 Aug 2023 16:37:00 GMT\r\n"
                               "Server: SC\r\n"
                               "Last-Modified: Wed, 27 Aug 2023 16:37:00 GMT\r\n"
                               "Content-Length: {}\r\n"
                               "Content-Type: text/html\r\n"
                               "Connection: Closed\r\n"
                               "\r\n"_a8;

    StringBuilder sb(outputBuffer, StringEncoding::Ascii, StringBuilder::Clear);
    SC_TRY_IF(sb.format(headers, sv.sizeInBytes()));
    SC_TRY_IF(sb.append(sv));
    ended = true;
    return outputBuffer.pop_back(); // pop null terminator
}

SC::ReturnCode SC::HttpServer::Request::parse(Span<const char> readData, Response& res, HttpServer& server)
{
    if (headerBuffer.size() > server.maxHeaderSize)
    {
        parsedSuccessfully = false;
        return "Header size exceeded limit"_a8;
    }
    SC_TRY_IF(headerBuffer.push_back(readData));
    size_t readBytes;
    while (parsedSuccessfully and not readData.empty())
    {
        Span<const char> parsedData;
        parsedSuccessfully &= parser.parse(readData.asConst(), readBytes, parsedData);
        parsedSuccessfully &= readData.sliceStart(readBytes, readData);
        if (parser.state == HttpParser::State::Finished)
            break;
        if (parser.state == HttpParser::State::Result)
        {
            Header header;
            header.result = parser.result;
            header.start  = static_cast<uint32_t>(parser.tokenStart);
            header.length = static_cast<uint32_t>(parser.tokenLength);
            parsedSuccessfully &= headerOffsets.push_back(header);
            if (parser.result == HttpParser::Result::HeadersEnd)
            {
                headersEndReceived = true;
                server.onClient(*this, res);
                break;
            }
        }
    }
    return parsedSuccessfully;
}

SC::ReturnCode SC::HttpServerAsync::start(EventLoop& loop, uint32_t maxConnections, StringView address, uint16_t port)
{
    eventLoop = &loop;
    SC_TRY_IF(requestClients.resize(maxConnections));
    SC_TRY_IF(requests.resize(maxConnections));
    SocketIPAddress nativeAddress;
    SC_TRY_IF(nativeAddress.fromAddressPort(address, port));
    SC_TRY_IF(eventLoop->createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
    SC_TRY_IF(SocketServer(serverSocket).listen(nativeAddress));
    SC_TRY_IF(startAccept());
    return true;
}

SC::ReturnCode SC::HttpServerAsync::stop() { return eventLoop->stopAsync(asyncAccept); }

SC::ReturnCode SC::HttpServerAsync::startAccept()
{
    asyncAccept.debugName = "HttpServerAsync";
    return eventLoop->startAccept(asyncAccept, serverSocket, SC_FUNCTION_MEMBER(&HttpServerAsync::onNewClient, this));
}

void SC::HttpServerAsync::onNewClient(AsyncAcceptResult& result)
{
    bool succeeded = true;

    auto key1 = requests.allocate();
    succeeded &= key1.isValid();
    auto key2 = requestClients.allocate();
    succeeded &= key2.isValid() and key1 == key2;
    if (succeeded)
    {
        RequestClient&   client = *requestClients.get(key2);
        RequestResponse& req    = *requests.get(key1);
        client.key              = key2;
        client.socket           = move(result.acceptedClient);

        auto& buffer = req.request.headerBuffer;
        succeeded &= buffer.resizeWithoutInitializing(buffer.capacity());
        (void)StringBuilder(client.debugName)
            .format("HttpServerAsync::client [{}:{}]", (int)key1.generation.generation, (int)key1.index);
        client.asyncReceive.debugName = client.debugName.bytesIncludingTerminator();
        succeeded &= eventLoop->startReceive(client.asyncReceive, client.socket, buffer.toSpan(),
                                             SC_FUNCTION_MEMBER(&HttpServerAsync::onReceive, this));
        SC_RELEASE_ASSERT(succeeded);
    }
    result.rearm(true);
}

void SC::HttpServerAsync::onReceive(AsyncReceiveResult& result)
{
    SC_DISABLE_OFFSETOF_WARNING
    RequestClient& client = SC_FIELD_OFFSET(RequestClient, asyncReceive, result.async);
    SC_ENABLE_OFFSETOF_WARNING
    SC_RELEASE_ASSERT(&client.asyncReceive == &result.async);
    RequestResponse& rr = *requests.get(client.key.cast_to<RequestResponse>());
    if (not rr.request.parse(result.readData.asConst(), rr.response, *this))
    {
        // TODO: Invoke on error
    }
    else
    {
        if (rr.response.mustBeFlushed())
        {
            client.asyncSend.debugName = client.debugName.bytesIncludingTerminator();

            auto outspan = rr.response.outputBuffer.toSpan().asConst();
            bool res     = eventLoop->startSend(client.asyncSend, client.socket, outspan,
                                                SC_FUNCTION_MEMBER(&HttpServerAsync::onAfterSend, this));
            if (not res)
            {
                // TODO: Invoke on error
                res = res;
            }
        }
        else
        {
            result.rearm(true);
        }
    }
}

void SC::HttpServerAsync::onAfterSend(AsyncSendResult& result)
{
    SC_DISABLE_OFFSETOF_WARNING
    RequestClient& requestClient = SC_FIELD_OFFSET(RequestClient, asyncSend, result.async);
    SC_ENABLE_OFFSETOF_WARNING
    SC_UNUSED(requestClient);
    auto* rr = requests.get(requestClient.key.cast_to<RequestResponse>());
    rr->response.outputBuffer.clear();
    // TODO: Close socket and dispose resources
}
