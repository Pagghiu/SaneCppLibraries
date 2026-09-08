// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpTestClient.h"
#include "Libraries/Http/HttpAsyncClient.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Http/HttpWebSocket.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
#include <string.h>

namespace SC
{
struct HttpWebSocketHandshakeTest;
void   runHttpWebSocketHandshakeTest(TestReport& report);
void   httpWebSocketTestForceSha1ProvidersUnavailable(bool platformUnavailable, bool libCryptoUnavailable);
Result httpWebSocketTestSha1(HttpWebSocketSha1Mode mode, Span<const uint8_t> data, Span<uint8_t> digest,
                             size_t selfContainedChunkSize);
Result httpWebSocketTestSelfContainedSha1RepeatedByte(uint8_t value, size_t count, Span<uint8_t> digest);
} // namespace SC

static bool sha1DigestEquals(const SC::uint8_t actual[20], const SC::uint8_t expected[20])
{
    return ::memcmp(actual, expected, 20) == 0;
}

static bool resultMessageEquals(SC::Result result, SC::StringSpan expected)
{
    return not result and result.message != nullptr and
           SC::StringSpan::fromNullTerminated(result.message, SC::StringEncoding::Ascii) == expected;
}

struct SC::HttpWebSocketHandshakeTest : public SC::TestCase
{
    HttpWebSocketHandshakeTest(SC::TestReport& report) : TestCase(report, "HttpWebSocketHandshakeTest")
    {
        if (test_section("client key and accept generation"))
        {
            clientKeyAndAcceptGeneration();
        }
        if (test_section("SHA1 provider policies"))
        {
            sha1ProviderPolicies();
        }
        if (test_section("self-contained SHA1 standard vectors"))
        {
            selfContainedSha1StandardVectors();
        }
        if (test_section("SHA1 providers agree across block boundaries"))
        {
            sha1ProvidersAgreeAcrossBlockBoundaries();
        }
#if SC_PLATFORM_LINUX
        if (test_section("Linux SHA1 tries AF_ALG after libcrypto"))
        {
            linuxSha1Fallback();
        }
#endif
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
        if (test_section("async client connect integration"))
        {
            asyncClientConnectIntegration();
        }
        if (test_section("async server keeps upgraded connection alive"))
        {
            asyncServerKeepsUpgradedConnectionAlive();
        }
        if (test_section("async server releases upgraded connection on client close"))
        {
            asyncServerReleasesUpgradedConnectionOnClientClose();
        }
        if (test_section("async server broadcast reaches upgraded client"))
        {
            asyncServerBroadcastReachesUpgradedClient();
        }
    }

    void clientKeyAndAcceptGeneration();
    void sha1ProviderPolicies();
    void selfContainedSha1StandardVectors();
    void sha1ProvidersAgreeAcrossBlockBoundaries();
#if SC_PLATFORM_LINUX
    void linuxSha1Fallback();
#endif
    void serverRequestValidation();
    void clientResponseValidation();
    void asyncServerAcceptIntegration();
    void asyncClientConnectIntegration();
    void asyncServerKeepsUpgradedConnectionAlive();
    void asyncServerReleasesUpgradedConnectionOnClientClose();
    void asyncServerBroadcastReachesUpgradedClient();
};

void SC::HttpWebSocketHandshakeTest::sha1ProviderPolicies()
{
    char       platformStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
    StringSpan platformAccept;
    SC_TEST_EXPECT(HttpWebSocketHandshake::computeAccept("dGhlIHNhbXBsZSBub25jZQ==", platformStorage, platformAccept,
                                                         HttpWebSocketSha1Mode::Platform));
    SC_TEST_EXPECT(platformAccept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

    char       selfContainedStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
    StringSpan selfContainedAccept;
    SC_TEST_EXPECT(HttpWebSocketHandshake::computeAccept("dGhlIHNhbXBsZSBub25jZQ==", selfContainedStorage,
                                                         selfContainedAccept, HttpWebSocketSha1Mode::SelfContained));
    SC_TEST_EXPECT(selfContainedAccept == platformAccept);

    httpWebSocketTestForceSha1ProvidersUnavailable(true, true);
    const Result unavailable   = HttpWebSocketHandshake::computeAccept("dGhlIHNhbXBsZSBub25jZQ==", platformStorage,
                                                                       platformAccept, HttpWebSocketSha1Mode::Platform);
    const Result selfContained = HttpWebSocketHandshake::computeAccept(
        "dGhlIHNhbXBsZSBub25jZQ==", selfContainedStorage, selfContainedAccept, HttpWebSocketSha1Mode::SelfContained);
    httpWebSocketTestForceSha1ProvidersUnavailable(false, false);

    SC_TEST_EXPECT(resultMessageEquals(unavailable, "HttpWebSocketHandshake platform SHA1 provider unavailable"));
    SC_TEST_EXPECT(selfContained);
    SC_TEST_EXPECT(selfContainedAccept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");

    const Result invalidMode = HttpWebSocketHandshake::computeAccept(
        "dGhlIHNhbXBsZSBub25jZQ==", selfContainedStorage, selfContainedAccept, static_cast<HttpWebSocketSha1Mode>(255));
    SC_TEST_EXPECT(resultMessageEquals(invalidMode, "HttpWebSocketHandshake SHA1 mode invalid"));
}

void SC::HttpWebSocketHandshakeTest::selfContainedSha1StandardVectors()
{
    static constexpr uint8_t EmptyDigest[20]    = {0xDA, 0x39, 0xA3, 0xEE, 0x5E, 0x6B, 0x4B, 0x0D, 0x32, 0x55,
                                                   0xBF, 0xEF, 0x95, 0x60, 0x18, 0x90, 0xAF, 0xD8, 0x07, 0x09};
    static constexpr uint8_t AbcDigest[20]      = {0xA9, 0x99, 0x3E, 0x36, 0x47, 0x06, 0x81, 0x6A, 0xBA, 0x3E,
                                                   0x25, 0x71, 0x78, 0x50, 0xC2, 0x6C, 0x9C, 0xD0, 0xD8, 0x9D};
    static constexpr uint8_t LongDigest[20]     = {0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E, 0xBA, 0xAE,
                                                   0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5, 0xE5, 0x46, 0x70, 0xF1};
    static constexpr uint8_t MillionADigest[20] = {0x34, 0xAA, 0x97, 0x3C, 0xD4, 0xC4, 0xDA, 0xA4, 0xF6, 0x1E,
                                                   0xEB, 0x2B, 0xDB, 0xAD, 0x27, 0x31, 0x65, 0x34, 0x01, 0x6F};
    static constexpr char    LongMessage[]      = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    uint8_t digest[20] = {};
    SC_TEST_EXPECT(httpWebSocketTestSha1(HttpWebSocketSha1Mode::SelfContained, {}, digest, 1));
    SC_TEST_EXPECT(sha1DigestEquals(digest, EmptyDigest));

    SC_TEST_EXPECT(httpWebSocketTestSha1(HttpWebSocketSha1Mode::SelfContained,
                                         Span<const uint8_t>::reinterpret_bytes("abc", 3), digest, 1));
    SC_TEST_EXPECT(sha1DigestEquals(digest, AbcDigest));

    const auto   longMessage  = Span<const uint8_t>::reinterpret_bytes(LongMessage, sizeof(LongMessage) - 1);
    const size_t chunkSizes[] = {1, 7, 55, 56, 64};
    for (size_t chunkSize : chunkSizes)
    {
        SC_TEST_EXPECT(httpWebSocketTestSha1(HttpWebSocketSha1Mode::SelfContained, longMessage, digest, chunkSize));
        SC_TEST_EXPECT(sha1DigestEquals(digest, LongDigest));
    }

    SC_TEST_EXPECT(httpWebSocketTestSelfContainedSha1RepeatedByte('a', 1000000, digest));
    SC_TEST_EXPECT(sha1DigestEquals(digest, MillionADigest));
}

void SC::HttpWebSocketHandshakeTest::sha1ProvidersAgreeAcrossBlockBoundaries()
{
    uint8_t input[129];
    for (size_t idx = 0; idx < sizeof(input); ++idx)
    {
        input[idx] = static_cast<uint8_t>(idx * 37 + 11);
    }

    for (size_t length = 0; length <= sizeof(input); ++length)
    {
        uint8_t platformDigest[20]      = {};
        uint8_t selfContainedDigest[20] = {};
        SC_TEST_EXPECT(httpWebSocketTestSha1(HttpWebSocketSha1Mode::Platform, {input, length}, platformDigest, 0));
        SC_TEST_EXPECT(httpWebSocketTestSha1(HttpWebSocketSha1Mode::SelfContained, {input, length}, selfContainedDigest,
                                             length % 17 + 1));
        SC_TEST_EXPECT(sha1DigestEquals(platformDigest, selfContainedDigest));
    }
}

#if SC_PLATFORM_LINUX
void SC::HttpWebSocketHandshakeTest::linuxSha1Fallback()
{
    char       acceptStorage[HttpWebSocketHandshake::AcceptKeyLength] = {};
    StringSpan accept;

    httpWebSocketTestForceSha1ProvidersUnavailable(false, true);
    const Result result = HttpWebSocketHandshake::computeAccept("dGhlIHNhbXBsZSBub25jZQ==", acceptStorage, accept,
                                                                HttpWebSocketSha1Mode::Platform);
    httpWebSocketTestForceSha1ProvidersUnavailable(false, false);

    if (result)
    {
        SC_TEST_EXPECT(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }
    else
    {
        SC_TEST_EXPECT(resultMessageEquals(result, "HttpWebSocketHandshake platform SHA1 provider unavailable"));
    }
}
#endif

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

void SC::HttpWebSocketHandshakeTest::asyncClientConnectIntegration()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using ServerConnection = HttpAsyncConnection<3, 3, 8 * 1024, 8 * 1024>;
    using ClientConnection = HttpAsyncClientConnection<4, 4, 8 * 1024, 8 * 1024>;

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   serverPort = report.mapPort(6182);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct ServerContext
    {
        HttpWebSocketTransportView transport;
        HttpConnection*            connection = nullptr;
        bool                       accepted   = false;
    } serverContext;

    httpServer.onRequest = [this, &serverContext](HttpConnection& connection)
    {
        serverContext.connection                                    = &connection;
        char acceptStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
        SC_TEST_EXPECT(
            HttpWebSocketHandshake::acceptServerConnection(connection, serverContext.transport, acceptStorage));
        serverContext.accepted = serverContext.transport.isValid();
    };

    ClientConnection             clientStorage;
    HttpAsyncClient              client;
    HttpWebSocketClientHandshake handshake;
    HttpWebSocketTransportView   clientTransport;
    SC_TEST_EXPECT(client.init(clientStorage));

    struct ClientContext
    {
        HttpWebSocketHandshakeTest* test;
        HttpAsyncServer*            httpServer;
        HttpAsyncClient*            client;
        ServerContext*              serverContext;
        bool                        connected = false;

        void onConnected(HttpWebSocketTransportView& transport)
        {
            connected = true;
            test->recordExpectation("server accepted", serverContext->accepted);
            test->recordExpectation("client transport valid", transport.isValid());
            test->recordExpectation("server transport valid", serverContext->transport.isValid());

            transport.readableStream->destroy();
            transport.writableStream->destroy();
            if (serverContext->connection != nullptr)
            {
                serverContext->connection->readableSocketStream.destroy();
                serverContext->connection->writableSocketStream.destroy();
                if (serverContext->connection->socket.isValid())
                {
                    (void)serverContext->connection->socket.close();
                }
            }
            test->recordExpectation("stop server", httpServer->stop());
        }

        void onError(Result result) { test->recordExpectation("client websocket connect", result); }
    } clientContext = {this, &httpServer, &client, &serverContext, false};

    handshake.onConnected.bind<ClientContext, &ClientContext::onConnected>(clientContext);
    handshake.onError.bind<ClientContext, &ClientContext::onError>(clientContext);

    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/chat", serverPort));
    SC_TEST_EXPECT(handshake.connect(client, eventLoop, endpoint.view(), "dGhlIHNhbXBsZSBub25jZQ==", clientTransport));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(clientContext.connected);
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpWebSocketHandshakeTest::asyncServerKeepsUpgradedConnectionAlive()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using ServerConnection = HttpAsyncConnection<3, 3, 8 * 1024, 8 * 1024>;
    using ClientConnection = HttpAsyncClientConnection<4, 4, 8 * 1024, 8 * 1024>;

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   serverPort = report.mapPort(6183);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct ClientContext
    {
        HttpWebSocketHandshakeTest* test      = nullptr;
        AsyncEventLoop*             eventLoop = nullptr;
        AsyncLoopTimeout*           sendDelay = nullptr;
        HttpWebSocketTransportView* transport = nullptr;
        HttpWebSocketEndpoint       endpoint;
        bool                        connected = false;
        bool                        sent      = false;

        Result sendAfterUpgrade()
        {
            SC_TRY_MSG(transport != nullptr and transport->isValid(), "client websocket transport invalid");

            constexpr uint8_t maskKey[4]       = {1, 2, 3, 4};
            char              frameStorage[64] = {0};
            Span<const char>  encodedFrame;
            SC_TRY(endpoint.sendData(HttpWebSocketOpcode::Text, "after-upgrade"_a8.toCharSpan(), true, maskKey,
                                     frameStorage, encodedFrame));

            AsyncBufferView::ID bufferID;
            Span<char>          writableData;
            SC_TRY(transport->buffersPool->requestNewBuffer(encodedFrame.sizeInBytes(), bufferID, writableData));
            ::memcpy(writableData.data(), encodedFrame.data(), encodedFrame.sizeInBytes());
            transport->buffersPool->setNewBufferSize(bufferID, encodedFrame.sizeInBytes());
            const Result writeResult = transport->writableStream->write(bufferID);
            transport->buffersPool->unrefBuffer(bufferID);
            SC_TRY(writeResult);

            sent = true;
            return Result(true);
        }

        void onConnected(HttpWebSocketTransportView& connectedTransport)
        {
            connected = true;
            test->recordExpectation("client websocket transport", connectedTransport.isValid());
            if (sendDelay != nullptr)
            {
                test->recordExpectation("schedule websocket frame", sendDelay->start(*eventLoop, TimeMs{10}));
            }
        }

        void onError(Result result) { test->recordExpectation("client websocket connect", result); }

        void cleanup()
        {
            if (transport != nullptr and transport->isValid())
            {
                transport->readableStream->destroy();
                transport->writableStream->destroy();
            }
        }
    };

    struct ServerContext
    {
        HttpWebSocketHandshakeTest* test          = nullptr;
        HttpAsyncServer*            httpServer    = nullptr;
        ClientContext*              clientContext = nullptr;
        HttpWebSocketTransportView  transport;
        HttpConnection*             connection = nullptr;
        HttpWebSocketEndpoint       endpoint;
        bool                        accepted = false;
        bool                        received = false;
        bool                        cleaned  = false;

        Result onPayload(HttpWebSocketOpcode opcode, Span<char> payload, bool frameFinished)
        {
            if (not frameFinished)
            {
                return Result(true);
            }

            const bool payloadMatches =
                payload.sizeInBytes() == 13 and ::memcmp(payload.data(), "after-upgrade", 13) == 0;
            received = opcode == HttpWebSocketOpcode::Text and payloadMatches;
            test->recordExpectation("server websocket frame", received);
            cleanup();
            return Result(true);
        }

        void onData(AsyncBufferView::ID bufferID)
        {
            Span<char> data;
            if (connection == nullptr)
            {
                test->recordExpectation("server websocket connection", false);
                cleanup();
                return;
            }
            Result writableData = connection->buffersPool.getWritableData(bufferID, data);
            test->recordExpectation("server websocket data", writableData);
            if (not writableData)
            {
                cleanup();
                return;
            }

            size_t consumed = 0;
            Result frame    = endpoint.receive(data, consumed);
            test->recordExpectation("server websocket receive", frame);
            if (frame)
            {
                test->recordExpectation("server websocket consumed", consumed == data.sizeInBytes());
            }
            else
            {
                cleanup();
            }
        }

        void cleanup()
        {
            if (cleaned)
            {
                return;
            }

            cleaned = true;
            if (clientContext != nullptr)
            {
                clientContext->cleanup();
            }
            if (connection != nullptr)
            {
                connection->readableSocketStream.destroy();
                connection->writableSocketStream.destroy();
            }
            test->recordExpectation("stop server", httpServer->stop());
        }
    };

    HttpWebSocketTransportView clientTransport;
    AsyncLoopTimeout           sendDelay;
    ClientContext              clientContext;
    clientContext.test      = this;
    clientContext.eventLoop = &eventLoop;
    clientContext.sendDelay = &sendDelay;
    clientContext.transport = &clientTransport;
    clientContext.endpoint.reset(HttpWebSocketEndpointRole::Client);

    ServerContext serverContext;
    serverContext.test          = this;
    serverContext.httpServer    = &httpServer;
    serverContext.clientContext = &clientContext;
    serverContext.endpoint.reset(HttpWebSocketEndpointRole::Server);
    serverContext.endpoint.onDataFramePayload.bind<ServerContext, &ServerContext::onPayload>(serverContext);

    httpServer.onRequest = [this, &serverContext](HttpConnection& connection)
    {
        serverContext.connection                                    = &connection;
        char acceptStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
        SC_TEST_EXPECT(
            HttpWebSocketHandshake::acceptServerConnection(connection, serverContext.transport, acceptStorage));
        serverContext.accepted = serverContext.transport.isValid();
        SC_TEST_EXPECT(serverContext.accepted);
        SC_TEST_EXPECT((connection.readableSocketStream.eventData.addListener<ServerContext, &ServerContext::onData>(
            serverContext)));
    };

    sendDelay.callback = [&clientContext](AsyncLoopTimeout::Result&)
    { clientContext.test->recordExpectation("send websocket frame after upgrade", clientContext.sendAfterUpgrade()); };

    ClientConnection             clientStorage;
    HttpAsyncClient              client;
    HttpWebSocketClientHandshake handshake;
    SC_TEST_EXPECT(client.init(clientStorage));

    handshake.onConnected.bind<ClientContext, &ClientContext::onConnected>(clientContext);
    handshake.onError.bind<ClientContext, &ClientContext::onError>(clientContext);

    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/chat", serverPort));
    SC_TEST_EXPECT(handshake.connect(client, eventLoop, endpoint.view(), "dGhlIHNhbXBsZSBub25jZQ==", clientTransport));

    AsyncLoopTimeout timeout;
    timeout.callback = [this, &serverContext](AsyncLoopTimeout::Result&)
    {
        SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false);
        serverContext.cleanup();
        SC_TEST_EXPECT(serverContext.received);
    };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(clientContext.connected);
    SC_TEST_EXPECT(clientContext.sent);
    SC_TEST_EXPECT(serverContext.received);
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpWebSocketHandshakeTest::asyncServerReleasesUpgradedConnectionOnClientClose()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using ServerConnection = HttpAsyncConnection<3, 3, 8 * 1024, 8 * 1024>;
    using ClientConnection = HttpAsyncClientConnection<4, 4, 8 * 1024, 8 * 1024>;

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   serverPort = report.mapPort(6184);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct ServerContext
    {
        HttpConnection*            connection = nullptr;
        HttpWebSocketTransportView transport;
        bool                       accepted = false;
    } serverContext;

    httpServer.onRequest = [this, &serverContext](HttpConnection& connection)
    {
        serverContext.connection                                    = &connection;
        char acceptStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
        SC_TEST_EXPECT(
            HttpWebSocketHandshake::acceptServerConnection(connection, serverContext.transport, acceptStorage));
        serverContext.accepted = serverContext.transport.isValid();
        SC_TEST_EXPECT(serverContext.accepted);
    };

    ClientConnection             clientStorage;
    HttpAsyncClient              client;
    HttpWebSocketClientHandshake handshake;
    HttpWebSocketTransportView   clientTransport;
    SC_TEST_EXPECT(client.init(clientStorage));

    struct ClientContext
    {
        HttpWebSocketHandshakeTest* test;
        HttpAsyncClient*            client;
        bool                        connected = false;

        void onConnected(HttpWebSocketTransportView& transport)
        {
            connected = true;
            test->recordExpectation("client transport valid", transport.isValid());
            test->recordExpectation("client close", client->close());
        }

        void onError(Result result) { test->recordExpectation("client websocket connect", result); }
    } clientContext = {this, &client, false};

    handshake.onConnected.bind<ClientContext, &ClientContext::onConnected>(clientContext);
    handshake.onError.bind<ClientContext, &ClientContext::onError>(clientContext);

    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/chat", serverPort));
    SC_TEST_EXPECT(handshake.connect(client, eventLoop, endpoint.view(), "dGhlIHNhbXBsZSBub25jZQ==", clientTransport));

    struct ReleaseCheck
    {
        HttpWebSocketHandshakeTest* test;
        HttpAsyncServer*            httpServer;
        bool                        released = false;
        int                         attempts = 0;

        void onTimeout(AsyncLoopTimeout::Result& result)
        {
            if (httpServer->getConnections().getNumActiveConnections() == 0)
            {
                released = true;
                test->recordExpectation("stop server", httpServer->stop());
                return;
            }

            attempts++;
            if (attempts >= 100)
            {
                test->recordExpectation("upgraded connection released", false);
                test->recordExpectation("stop server", httpServer->stop());
                return;
            }

            result.getAsync().relativeTimeout = TimeMs{10};
            result.reactivateRequest(true);
        }
    } releaseCheck = {this, &httpServer, false, 0};

    AsyncLoopTimeout releaseTimeout;
    releaseTimeout.callback.bind<ReleaseCheck, &ReleaseCheck::onTimeout>(releaseCheck);
    SC_TEST_EXPECT(releaseTimeout.start(eventLoop, TimeMs{10}));

    AsyncLoopTimeout timeout;
    timeout.callback = [this, &httpServer](AsyncLoopTimeout::Result&)
    {
        SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false);
        (void)httpServer.stop();
    };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(serverContext.accepted);
    SC_TEST_EXPECT(clientContext.connected);
    SC_TEST_EXPECT(releaseCheck.released);
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpWebSocketHandshakeTest::asyncServerBroadcastReachesUpgradedClient()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using ServerConnection = HttpAsyncConnection<3, 3, 8 * 1024, 8 * 1024>;
    using ClientConnection = HttpAsyncClientConnection<4, 4, 8 * 1024, 8 * 1024>;

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   serverPort = report.mapPort(6185);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct ServerContext
    {
        HttpWebSocketHubClient     hubClients[1];
        HttpWebSocketSmallHub      hub;
        HttpWebSocketTransportView transport;
        HttpConnection*            connection = nullptr;
        bool                       accepted   = false;

        Result broadcastClear()
        {
            char frameStorage[128] = {0};
            return hub.broadcastText("{\"type\":\"clear\"}"_a8.toCharSpan(), frameStorage);
        }
    } serverContext;
    SC_TEST_EXPECT(serverContext.hub.init(Span<HttpWebSocketHubClient>(serverContext.hubClients)));

    httpServer.onRequest = [this, &serverContext](HttpConnection& connection)
    {
        serverContext.connection                                    = &connection;
        char acceptStorage[HttpWebSocketHandshake::AcceptKeyLength] = {0};
        SC_TEST_EXPECT(
            HttpWebSocketHandshake::acceptServerConnection(connection, serverContext.transport, acceptStorage));
        serverContext.accepted = serverContext.transport.isValid();
        SC_TEST_EXPECT(serverContext.accepted);
        size_t hubIndex = 0;
        SC_TEST_EXPECT(serverContext.hub.join(serverContext.transport, hubIndex));
    };

    ClientConnection             clientStorage;
    HttpAsyncClient              client;
    HttpWebSocketClientHandshake handshake;
    HttpWebSocketTransportView   clientTransport;
    SC_TEST_EXPECT(client.init(clientStorage));

    struct ClientContext
    {
        HttpWebSocketHandshakeTest* test       = nullptr;
        AsyncEventLoop*             eventLoop  = nullptr;
        HttpAsyncServer*            httpServer = nullptr;
        ServerContext*              server     = nullptr;
        AsyncLoopTimeout*           sendDelay  = nullptr;
        HttpWebSocketTransportView* transport  = nullptr;
        HttpWebSocketEndpoint       endpoint;
        bool                        connected = false;
        bool                        received  = false;
        bool                        cleaned   = false;

        Result onPayload(HttpWebSocketOpcode opcode, Span<char> payload, bool frameFinished)
        {
            if (not frameFinished)
            {
                return Result(true);
            }

            const bool payloadMatches =
                payload.sizeInBytes() == 16 and ::memcmp(payload.data(), "{\"type\":\"clear\"}", 16) == 0;
            received = opcode == HttpWebSocketOpcode::Text and payloadMatches;
            test->recordExpectation("client received clear broadcast", received);
            cleanup();
            return Result(true);
        }

        void onData(AsyncBufferView::ID bufferID)
        {
            Span<char> data;
            Result     writableData = transport->buffersPool->getWritableData(bufferID, data);
            test->recordExpectation("client websocket data", writableData);
            if (not writableData)
            {
                cleanup();
                return;
            }

            size_t consumed = 0;
            Result frame    = endpoint.receive(data, consumed);
            test->recordExpectation("client websocket receive", frame);
            if (frame)
            {
                test->recordExpectation("client websocket consumed", consumed == data.sizeInBytes());
            }
            else
            {
                cleanup();
            }
        }

        void onConnected(HttpWebSocketTransportView& connectedTransport)
        {
            connected = true;
            test->recordExpectation("client transport valid", connectedTransport.isValid());
            const bool listenerAdded =
                connectedTransport.readableStream->eventData.addListener<ClientContext, &ClientContext::onData>(*this);
            test->recordExpectation("client data listener", listenerAdded);
            if (sendDelay != nullptr)
            {
                test->recordExpectation("schedule server broadcast", sendDelay->start(*eventLoop, TimeMs{10}));
            }
        }

        void onError(Result result) { test->recordExpectation("client websocket connect", result); }

        void cleanup()
        {
            if (cleaned)
            {
                return;
            }

            cleaned = true;
            if (transport != nullptr and transport->isValid())
            {
                transport->readableStream->destroy();
                transport->writableStream->destroy();
            }
            if (server != nullptr and server->connection != nullptr)
            {
                server->connection->readableSocketStream.destroy();
                server->connection->writableSocketStream.destroy();
            }
            test->recordExpectation("stop server", httpServer->stop());
        }
    } clientContext;

    clientContext.test       = this;
    clientContext.eventLoop  = &eventLoop;
    clientContext.httpServer = &httpServer;
    clientContext.server     = &serverContext;
    clientContext.transport  = &clientTransport;
    clientContext.endpoint.reset(HttpWebSocketEndpointRole::Client);
    clientContext.endpoint.onDataFramePayload.bind<ClientContext, &ClientContext::onPayload>(clientContext);

    AsyncLoopTimeout sendDelay;
    clientContext.sendDelay = &sendDelay;
    sendDelay.callback      = [&serverContext, &clientContext](AsyncLoopTimeout::Result&)
    { clientContext.test->recordExpectation("server clear broadcast", serverContext.broadcastClear()); };

    handshake.onConnected.bind<ClientContext, &ClientContext::onConnected>(clientContext);
    handshake.onError.bind<ClientContext, &ClientContext::onError>(clientContext);

    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/chat", serverPort));
    SC_TEST_EXPECT(handshake.connect(client, eventLoop, endpoint.view(), "dGhlIHNhbXBsZSBub25jZQ==", clientTransport));

    AsyncLoopTimeout timeout;
    timeout.callback = [this, &clientContext](AsyncLoopTimeout::Result&)
    {
        SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false);
        clientContext.cleanup();
    };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(serverContext.accepted);
    SC_TEST_EXPECT(clientContext.connected);
    SC_TEST_EXPECT(clientContext.received);
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::runHttpWebSocketHandshakeTest(SC::TestReport& report) { HttpWebSocketHandshakeTest test(report); }
