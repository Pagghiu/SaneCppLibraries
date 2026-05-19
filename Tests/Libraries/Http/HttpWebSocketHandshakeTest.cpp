// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpTestClient.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Http/HttpWebSocket.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct HttpWebSocketHandshakeTest;
void runHttpWebSocketHandshakeTest(TestReport& report);
} // namespace SC

struct SC::HttpWebSocketHandshakeTest : public SC::TestCase
{
    HttpWebSocketHandshakeTest(SC::TestReport& report) : TestCase(report, "HttpWebSocketHandshakeTest")
    {
        if (test_section("client key and accept generation"))
        {
            clientKeyAndAcceptGeneration();
        }
        if (test_section("server request validation"))
        {
            serverRequestValidation();
        }
        if (test_section("client response validation"))
        {
            clientResponseValidation();
        }
        if (test_section("async server accept integration"))
        {
            asyncServerAcceptIntegration();
        }
    }

    void clientKeyAndAcceptGeneration();
    void serverRequestValidation();
    void clientResponseValidation();
    void asyncServerAcceptIntegration();
};

void SC::HttpWebSocketHandshakeTest::clientKeyAndAcceptGeneration()
{
    const uint8_t nonce[HttpWebSocketHandshake::NonceLength] = {'t', 'h', 'e', ' ', 's', 'a', 'm', 'p',
                                                                'l', 'e', ' ', 'n', 'o', 'n', 'c', 'e'};

    char       keyStorage[HttpWebSocketHandshake::ClientKeyLength] = {0};
    StringSpan key;
    SC_TEST_EXPECT(HttpWebSocketHandshake::createClientKey(nonce, keyStorage, key));
    SC_TEST_EXPECT(key == "dGhlIHNhbXBsZSBub25jZQ==");
    SC_TEST_EXPECT(HttpWebSocketHandshake::validateClientKey(key));

    char       acceptStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
    StringSpan accept;
    SC_TEST_EXPECT(HttpWebSocketHandshake::computeAccept(key, acceptStorage, accept));
    SC_TEST_EXPECT(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

void SC::HttpWebSocketHandshakeTest::serverRequestValidation()
{
    HttpWebSocketServerHandshakeRequestView request;
    request.method              = HttpParser::Method::HttpGET;
    request.version             = "HTTP/1.1";
    request.upgrade             = "websocket";
    request.connection          = "keep-alive, Upgrade";
    request.secWebSocketKey     = "dGhlIHNhbXBsZSBub25jZQ==";
    request.secWebSocketVersion = "13";

    SC_TEST_EXPECT(HttpWebSocketHandshake::headerContainsToken("keep-alive, Upgrade", "upgrade"));
    SC_TEST_EXPECT(HttpWebSocketHandshake::validateServerRequest(request).accepted());

    request.connection = "keep-alive";
    SC_TEST_EXPECT(HttpWebSocketHandshake::validateServerRequest(request).status ==
                   HttpWebSocketHandshakeResult::Status::BadRequest);

    request.connection          = "Upgrade";
    request.secWebSocketVersion = "12";
    SC_TEST_EXPECT(HttpWebSocketHandshake::validateServerRequest(request).status ==
                   HttpWebSocketHandshakeResult::Status::UnsupportedVersion);

    request.secWebSocketVersion = "13";
    request.secWebSocketKey     = "not-a-valid-key";
    SC_TEST_EXPECT(HttpWebSocketHandshake::validateServerRequest(request).status ==
                   HttpWebSocketHandshakeResult::Status::BadRequest);
}

void SC::HttpWebSocketHandshakeTest::clientResponseValidation()
{
    HttpWebSocketClientHandshakeResponseView response;
    response.statusCode         = 101;
    response.upgrade            = "WebSocket";
    response.connection         = "Upgrade";
    response.secWebSocketAccept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

    SC_TEST_EXPECT(HttpWebSocketHandshake::validateClientResponse(response, "dGhlIHNhbXBsZSBub25jZQ=="));

    response.secWebSocketAccept = "aaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    SC_TEST_EXPECT(not HttpWebSocketHandshake::validateClientResponse(response, "dGhlIHNhbXBsZSBub25jZQ=="));
}

void SC::HttpWebSocketHandshakeTest::asyncServerAcceptIntegration()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<3, 3, 8 * 1024, 8 * 1024>;

    HttpConnectionType connections[1];
    HttpAsyncServer    httpServer;
    const uint16_t     serverPort = report.mapPort(6181);
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct ServerContext
    {
        bool accepted = false;
    } serverContext;

    struct ClientContext
    {
        HttpWebSocketHandshakeTest* test;
        HttpAsyncServer*            httpServer;
        ServerContext*              serverContext;

        void onClient(HttpTestClient& clientRef)
        {
            const StringView response(clientRef.getResponse());
            test->recordExpectation("server accepted websocket", serverContext->accepted);
            test->recordExpectation("response status", response.containsString("101 Switching Protocols"));
            test->recordExpectation("response accept key", response.containsString("Sec-WebSocket-Accept: "
                                                                                   "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
            test->recordExpectation("stop server", httpServer->stop());
        }
    } clientContext = {this, &httpServer, &serverContext};

    httpServer.onRequest = [this, &serverContext](HttpConnection& connection)
    {
        HttpWebSocketTransportView transport;
        char                       acceptStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
        const auto                 validation = HttpWebSocketHandshake::validateServerRequest(connection.request);
        if (validation.accepted())
        {
            serverContext.accepted = true;
            SC_TEST_EXPECT(HttpWebSocketHandshake::acceptServerConnection(connection, transport, acceptStorage));
            SC_TEST_EXPECT(transport.isValid());
        }
        else
        {
            SC_TEST_EXPECT(HttpWebSocketHandshake::rejectServerConnection(connection.response, validation));
        }
    };

    HttpTestClient client;
    String         endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/chat", serverPort));
    constexpr StringView request = "GET /chat HTTP/1.1\r\n"
                                   "Host: 127.0.0.1\r\n"
                                   "Upgrade: websocket\r\n"
                                   "Connection: keep-alive, Upgrade\r\n"
                                   "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                                   "Sec-WebSocket-Version: 13\r\n"
                                   "\r\n";

    client.callback.bind<ClientContext, &ClientContext::onClient>(clientContext);
    SC_TEST_EXPECT(client.sendRaw(eventLoop, endpoint.view(), request));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::runHttpWebSocketHandshakeTest(SC::TestReport& report) { HttpWebSocketHandshakeTest test(report); }
