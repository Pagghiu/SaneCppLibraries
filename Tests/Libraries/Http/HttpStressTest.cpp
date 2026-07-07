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
struct HttpStressTest;
void runHttpStressTest(TestReport& report);
} // namespace SC

namespace
{
struct StressReadableStream : public SC::AsyncReadableStream
{
    Request queue[1];

    StressReadableStream() { setReadQueue(queue); }

  private:
    virtual SC::Result asyncRead() override { return SC::Result(true); }
};

struct StressWritableStream : public SC::AsyncWritableStream
{
    Request queue[2];

    SC::size_t writes       = 0;
    SC::size_t payloadBytes = 0;

    StressWritableStream() { setWriteQueue(queue); }

  private:
    virtual SC::Result asyncWrite(SC::AsyncBufferView::ID                     bufferID,
                                  SC::Function<void(SC::AsyncBufferView::ID)> cb) override
    {
        SC::Span<const char> frame;
        SC_TRY(getBuffersPool().getReadableData(bufferID, frame));
        SC_TRY_MSG(frame.sizeInBytes() >= 2, "HttpStressTest websocket frame too small");
        SC_TRY_MSG((frame[0] & 0x80) != 0, "HttpStressTest websocket frame not final");
        SC_TRY_MSG((frame[0] & 0x0F) == static_cast<SC::uint8_t>(SC::HttpWebSocketOpcode::Text),
                   "HttpStressTest websocket frame opcode mismatch");
        SC_TRY_MSG((frame[1] & 0x80) == 0, "HttpStressTest server broadcast must be unmasked");

        const SC::size_t payloadLength = static_cast<SC::size_t>(frame[1] & 0x7F);
        SC_TRY_MSG(payloadLength < 126, "HttpStressTest payload unexpectedly uses extended length");
        SC_TRY_MSG(frame.sizeInBytes() == payloadLength + 2, "HttpStressTest websocket frame length mismatch");

        writes++;
        payloadBytes += payloadLength;

        finishedWriting(bufferID, SC::move(cb), SC::Result(true));
        return SC::Result(true);
    }
};
} // namespace

struct SC::HttpStressTest : public SC::TestCase
{
    HttpStressTest(SC::TestReport& report) : TestCase(report, "HttpStressTest")
    {
        if (test_section("websocket hub fanout smoke"))
        {
            websocketHubFanoutSmoke();
        }
        if (test_section("keep alive many requests smoke"))
        {
            keepAliveManyRequestsSmoke();
        }
        if (test_section("chunked request burst smoke"))
        {
            chunkedRequestBurstSmoke();
        }
    }

    void websocketHubFanoutSmoke();
    void keepAliveManyRequestsSmoke();
    void chunkedRequestBurstSmoke();
};

void SC::HttpStressTest::websocketHubFanoutSmoke()
{
    constexpr size_t NumClients       = 4;
    constexpr size_t PoolSlices       = 2;
    constexpr size_t PoolStorageBytes = 512;
    constexpr size_t NumMessages      = 512;
    constexpr size_t PayloadBytes     = 96;

    HttpWebSocketHubClient clients[NumClients];
    HttpWebSocketSmallHub  hub;
    SC_TEST_EXPECT(hub.init({clients, NumClients}));

    StressReadableStream readable[NumClients];
    StressWritableStream writable[NumClients];
    AsyncBuffersPool     pools[NumClients];
    AsyncBufferView      bufferViews[NumClients][PoolSlices];
    char                 bufferStorage[NumClients][PoolStorageBytes] = {};

    HttpWebSocketTransportView transports[NumClients];
    for (size_t idx = 0; idx < NumClients; ++idx)
    {
        pools[idx].setBuffers({bufferViews[idx], PoolSlices});
        SC_TEST_EXPECT(AsyncBuffersPool::sliceInEqualParts({bufferViews[idx], PoolSlices},
                                                           {bufferStorage[idx], PoolStorageBytes}, PoolSlices));
        SC_TEST_EXPECT(readable[idx].init(pools[idx]));
        SC_TEST_EXPECT(writable[idx].init(pools[idx]));

        transports[idx].readableStream = &readable[idx];
        transports[idx].writableStream = &writable[idx];
        transports[idx].buffersPool    = &pools[idx];

        size_t clientIndex = 0;
        SC_TEST_EXPECT(hub.join(transports[idx], clientIndex));
        SC_TEST_EXPECT(clientIndex == idx);
    }

    char payload[PayloadBytes] = {};
    for (size_t messageIdx = 0; messageIdx < NumMessages; ++messageIdx)
    {
        for (size_t payloadIdx = 0; payloadIdx < sizeof(payload); ++payloadIdx)
        {
            payload[payloadIdx] = static_cast<char>('a' + ((messageIdx + payloadIdx) % 26));
        }

        char frameStorage[128] = {};
        SC_TEST_EXPECT(hub.broadcastText({payload, sizeof(payload)}, frameStorage));
    }

    for (size_t idx = 0; idx < NumClients; ++idx)
    {
        SC_TEST_EXPECT(writable[idx].writes == NumMessages);
        SC_TEST_EXPECT(writable[idx].payloadBytes == NumMessages * PayloadBytes);
    }
    SC_TEST_EXPECT(hub.getNumClients() == NumClients);
}

void SC::HttpStressTest::keepAliveManyRequestsSmoke()
{
    constexpr int NumRequests = SC_COMPILER_FILC ? 8 : 64;
    constexpr int TimeoutMs   = SC_COMPILER_FILC ? 30000 : 3000;

    constexpr int  MaxConnections = 4;
    constexpr int  RequestSlices  = 2;
    constexpr int  HeaderBytes    = 8 * 1024;
    constexpr int  StreamBytes    = 1024;
    const uint16_t serverPort     = report.mapPort(6210);

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<RequestSlices, RequestSlices, HeaderBytes, StreamBytes>;

    HttpConnectionType connections[MaxConnections];
    HttpAsyncServer    httpServer;
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    httpServer.setMaxRequestsPerConnection(NumRequests + 1);
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct Context
    {
        HttpStressTest* test = nullptr;

        AsyncEventLoop*  loop       = nullptr;
        HttpAsyncServer* httpServer = nullptr;

        String requestURL = StringEncoding::Ascii;

        int serverRequests  = 0;
        int clientResponses = 0;
    } context;

    context.test       = this;
    context.loop       = &eventLoop;
    context.httpServer = &httpServer;
    SC_TEST_EXPECT(StringBuilder::format(context.requestURL, "http://127.0.0.1:{}/stress", serverPort));

    httpServer.onRequest = [&context](HttpConnection& connection)
    {
        context.serverRequests++;
        HttpResponse& response = connection.response;
        SC_ASSERT_RELEASE(response.sendText(200, "OK"));
    };

    HttpTestClient client;
    client.callback = [&context](HttpTestClient& completedClient)
    {
        context.clientResponses++;
        const StringView response(completedClient.getResponse());
        context.test->recordExpectation("stress keep-alive response body", response.containsString("OK"));

        if (context.clientResponses < NumRequests)
        {
            context.test->recordExpectation("stress keep-alive next request",
                                            completedClient.get(*context.loop, context.requestURL.view(), true));
            return;
        }

        context.test->recordExpectation("stress keep-alive stop server", context.httpServer->stop());
    };

    SC_TEST_EXPECT(client.get(eventLoop, context.requestURL.view(), true));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("HttpStressTest keep-alive smoke timed out" && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{TimeoutMs}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());

    SC_TEST_EXPECT(context.serverRequests == NumRequests);
    SC_TEST_EXPECT(context.clientResponses == NumRequests);
    SC_TEST_EXPECT(httpServer.getConnections().getNumActiveConnections() <= 1);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpStressTest::chunkedRequestBurstSmoke()
{
    constexpr int    NumRequests       = 32;
    constexpr int    NumConnections    = 4;
    constexpr int    RequestSlices     = 2;
    constexpr int    HeaderBytes       = 8 * 1024;
    constexpr int    StreamBytes       = 1024;
    constexpr size_t ExpectedBodyBytes = 32;
    const uint16_t   serverPort        = report.mapPort(6211);

    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<RequestSlices, RequestSlices, HeaderBytes, StreamBytes>;

    HttpConnectionType connections[NumConnections];
    HttpAsyncServer    httpServer;
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct RequestSlot
    {
        void onData(AsyncBufferView::ID bufferID)
        {
            SC_ASSERT_RELEASE(connection != nullptr);

            Span<const char> data;
            SC_ASSERT_RELEASE(connection->request.getReadableStream().getBuffersPool().getReadableData(bufferID, data));

            const size_t index = connection->getConnectionID().getIndex();
            SC_ASSERT_RELEASE(index < NumConnections);
            bodyBytes[index] += data.sizeInBytes();
            SC_ASSERT_RELEASE(connection->request.consumeBodyBytes(data.sizeInBytes()));
        }

        void onEnd()
        {
            SC_ASSERT_RELEASE(connection != nullptr);
            if (responded)
            {
                return;
            }
            responded = true;
            (void)connection->request.getReadableStream().eventData.removeListener<RequestSlot, &RequestSlot::onData>(
                *this);
            (void)connection->request.getReadableStream().eventEnd.removeListener<RequestSlot, &RequestSlot::onEnd>(
                *this);

            const size_t index = connection->getConnectionID().getIndex();
            test->recordExpectation("chunked stress connection index", index < NumConnections);
            test->recordExpectation("chunked stress decoded body size", bodyBytes[index] == expectedBodyBytes);
            test->recordExpectation("chunked stress response", connection->response.sendEmpty(200));
            (*ended)++;
        }

        HttpStressTest* test              = nullptr;
        size_t*         bodyBytes         = nullptr;
        size_t          expectedBodyBytes = 0;
        int*            ended             = nullptr;
        HttpConnection* connection        = nullptr;
        bool            responded         = false;
    };

    RequestSlot requestSlots[NumConnections];

    struct ServerContext
    {
        HttpStressTest* test         = nullptr;
        RequestSlot*    requestSlots = nullptr;

        size_t bodyBytes[NumConnections] = {};
        size_t expectedBodyBytes         = ExpectedBodyBytes;
        int    requests                  = 0;
        int    ended                     = 0;

        void onRequest(HttpConnection& connection)
        {
            requests++;
            const size_t index = connection.getConnectionID().getIndex();
            test->recordExpectation("chunked stress request index", index < NumConnections);
            bodyBytes[index]       = 0;
            RequestSlot& slot      = requestSlots[index];
            slot.test              = test;
            slot.bodyBytes         = bodyBytes;
            slot.expectedBodyBytes = expectedBodyBytes;
            slot.ended             = &ended;
            slot.connection        = &connection;
            slot.responded         = false;

            const bool addedData =
                connection.request.getReadableStream().eventData.addListener<RequestSlot, &RequestSlot::onData>(slot);
            test->recordExpectation("chunked stress data listener", addedData);
            const bool addedEnd =
                connection.request.getReadableStream().eventEnd.addListener<RequestSlot, &RequestSlot::onEnd>(slot);
            test->recordExpectation("chunked stress end listener", addedEnd);
        }
    } serverContext;

    serverContext.test         = this;
    serverContext.requestSlots = requestSlots;
    httpServer.onRequest.bind<ServerContext, &ServerContext::onRequest>(serverContext);

    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/chunked-stress", serverPort));

    constexpr StringView request = "PUT /chunked-stress HTTP/1.1\r\n"
                                   "Host: 127.0.0.1\r\n"
                                   "Transfer-Encoding: chunked\r\n"
                                   "\r\n"
                                   "4\r\n0123\r\n"
                                   "4\r\n4567\r\n"
                                   "4\r\n89ab\r\n"
                                   "4\r\ncdef\r\n"
                                   "4\r\n0123\r\n"
                                   "4\r\n4567\r\n"
                                   "4\r\n89ab\r\n"
                                   "4\r\ncdef\r\n"
                                   "0\r\n\r\n";

    HttpTestClient clients[NumRequests];
    struct ClientContext
    {
        HttpStressTest*  test       = nullptr;
        HttpAsyncServer* httpServer = nullptr;
        AsyncEventLoop*  loop       = nullptr;
        HttpTestClient*  clients    = nullptr;
        String*          endpoint   = nullptr;
        StringSpan       request;
        int              responses = 0;
    } clientContext;
    clientContext.test       = this;
    clientContext.httpServer = &httpServer;
    clientContext.loop       = &eventLoop;
    clientContext.clients    = clients;
    clientContext.endpoint   = &endpoint;
    clientContext.request    = request;

    for (HttpTestClient& client : clients)
    {
        client.callback = [&clientContext](HttpTestClient& completedClient)
        {
            clientContext.responses++;
            const StringView response(completedClient.getResponse());
            clientContext.test->recordExpectation("chunked stress response status", response.containsString("200 OK"));
            if (clientContext.responses == NumRequests)
            {
                clientContext.test->recordExpectation("chunked stress stop server", clientContext.httpServer->stop());
                return;
            }
            clientContext.test->recordExpectation(
                "chunked stress next request",
                clientContext.clients[clientContext.responses].sendRaw(
                    *clientContext.loop, clientContext.endpoint->view(), clientContext.request));
        };
    }
    SC_TEST_EXPECT(clients[0].sendRaw(eventLoop, endpoint.view(), request));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("HttpStressTest chunked burst smoke timed out" && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{5000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());

    SC_TEST_EXPECT(serverContext.requests == NumRequests);
    SC_TEST_EXPECT(serverContext.ended == NumRequests);
    SC_TEST_EXPECT(clientContext.responses == NumRequests);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::runHttpStressTest(SC::TestReport& report) { HttpStressTest test(report); }
