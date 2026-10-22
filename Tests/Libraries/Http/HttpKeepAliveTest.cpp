// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpStreamTestHelpers.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct HttpKeepAliveTest;
} // namespace SC

struct SC::HttpKeepAliveTest : public SC::TestCase
{
    HttpKeepAliveTest(SC::TestReport& report) : TestCase(report, "HttpKeepAliveTest")
    {
        if (test_section("keep-alive multiple requests"))
        {
            runScenario(6160, 3, true, -1, 0, false);
        }
        if (test_section("keep-alive disabled by response"))
        {
            runScenario(6161, 1, true, 0, 0, true);
        }
        if (test_section("keep-alive max requests"))
        {
            runScenario(6162, 2, true, -1, 2, true);
        }
        if (test_section("keep-alive server default disabled"))
        {
            runScenario(6163, 1, false, -1, 0, true);
            // An explicit response override must win, including after each response reset.
            runScenario(6164, 3, false, 1, 0, false);
        }
        if (test_section("request close cannot be overridden"))
        {
            runRequestClose(6165, false, false);
            runRequestClose(6166, true, false);
            runRequestClose(6167, true, true);
        }
    }

    // responseOverride: -1 leaves the server default intact, 0 disables, 1 enables persistence.
    void runScenario(uint16_t port, int requests, bool defaultKeepAlive, int responseOverride, uint32_t maxRequests,
                     bool terminal);

    void runRequestClose(uint16_t port, bool overrideResponse, bool pipeline);
};

void SC::HttpKeepAliveTest::runRequestClose(uint16_t port, bool overrideResponse, bool pipeline)
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    using ServerConnection = HttpAsyncConnection<2, 2, 8 * 1024, 1024>;
    ServerConnection connections[2];
    HttpAsyncServer  server;
    SC_TEST_EXPECT(server.init(Span<ServerConnection>(connections)));

    SocketIPAddress address;
    SC_TEST_EXPECT(address.fromAddressPort("127.0.0.1", report.mapPort(port)));
    SocketDescriptor peer;
    SC_TEST_EXPECT(loop.createAsyncTCPSocket(address.getAddressFamily(), peer));

    AsyncSocketConnect connect;
    AsyncSocketSend    send;
    AsyncSocketReceive receive;

    AsyncLoopTimeout deadline;
    AsyncLoopTimeout checkReclaimed;

    struct Context
    {
        AsyncEventLoop&     loop;
        HttpAsyncServer&    server;
        SocketDescriptor&   peer;
        AsyncSocketSend&    send;
        AsyncSocketReceive& receive;
        AsyncLoopTimeout&   checkReclaimed;

        bool       overrideResponse;
        StringView request;

        int requests  = 0;
        int shutdowns = 0;

        bool peerEOF   = false;
        bool reclaimed = false;
        bool timedOut  = false;

        char   received[4096]      = {};
        size_t receivedSize        = 0;
        char   receiveBuffer[1024] = {};
    } ctx = {loop,
             server,
             peer,
             send,
             receive,
             checkReclaimed,
             overrideResponse,
             pipeline ? StringView("GET /first HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
                                   "GET /second HTTP/1.1\r\nHost: localhost\r\n\r\n")
                      : StringView("GET /first HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")};

    Function<Result(HttpConnection&, Function<void(Result)>)> shutdown = {
        [this, &ctx](HttpConnection& connection, Function<void(Result)> complete) -> Result
        {
            ctx.shutdowns++;
            SC_TEST_EXPECT(not connection.request.getKeepAlive());
            SC_TEST_EXPECT(connection.requestCount == 0);
            complete(Result(true));
            return Result(true);
        }};
    // The callback count below also verifies installation with the legacy void-returning setter.
    (void)server.setTransportShutdown(move(shutdown));
    server.onRequest = [this, &ctx](HttpConnection& connection)
    {
        ctx.requests++;
        SC_TEST_EXPECT(not connection.request.getKeepAlive());
        HttpResponse& response = connection.response;
        SC_TEST_EXPECT(not response.getKeepAlive());
        if (ctx.overrideResponse)
        {
            response.setKeepAlive(true);
            SC_TEST_EXPECT(not response.getKeepAlive());
        }
        SC_TEST_EXPECT(response.startResponse(200));
        if (ctx.overrideResponse)
        {
            SC_TEST_EXPECT(response.addHeader("Connection", "keep-alive")
                               .isError(HttpResultCategory, HttpError::KeepAliveDisabled));
        }
        SC_TEST_EXPECT(response.addHeader("Content-Length", "2"));
        SC_TEST_EXPECT(response.sendHeaders());
        SC_TEST_EXPECT(response.getWritableStream().write("OK"));
        SC_TEST_EXPECT(response.end());
    };
    server.onError = [this, &loop](Result result)
    {
        SC_TEST_EXPECT(result);
        loop.interrupt();
    };
    checkReclaimed.callback = [&ctx](AsyncLoopTimeout::Result& result)
    {
        if (ctx.server.getConnections().getNumActiveConnections() == 0)
        {
            ctx.reclaimed = true;
            ctx.loop.interrupt();
        }
        else
        {
            result.reactivateRequest(true);
        }
    };
    receive.callback = [this, &ctx](AsyncSocketReceive::Result& result)
    {
        Span<char> data;
        SC_TEST_EXPECT(result.get(data));
        if (result.completionData.disconnected)
        {
            ctx.peerEOF = true;
            SC_TEST_EXPECT(ctx.checkReclaimed.start(ctx.loop, TimeMs{1}));
        }
        else
        {
            SC_TEST_EXPECT(ctx.receivedSize + data.sizeInBytes() <= sizeof(ctx.received));
            if (ctx.receivedSize + data.sizeInBytes() <= sizeof(ctx.received))
            {
                ::memcpy(ctx.received + ctx.receivedSize, data.data(), data.sizeInBytes());
                ctx.receivedSize += data.sizeInBytes();
                result.reactivateRequest(true);
            }
            else
            {
                ctx.loop.interrupt();
            }
        }
    };
    connect.callback = [this, &ctx](AsyncSocketConnect::Result& result)
    {
        SC_TEST_EXPECT(result.isValid());
        SC_TEST_EXPECT(ctx.receive.start(ctx.loop, ctx.peer, ctx.receiveBuffer));
        SC_TEST_EXPECT(ctx.send.start(ctx.loop, ctx.peer, ctx.request.toCharSpan()));
    };
    send.callback = [this, &ctx](AsyncSocketSend::Result& result)
    {
        SC_TEST_EXPECT(result.isValid());
        SC_TEST_EXPECT(result.completionData.numBytes == ctx.request.sizeInBytes());
    };
    deadline.callback = [&ctx, &loop](AsyncLoopTimeout::Result&)
    {
        ctx.timedOut = true;
        loop.interrupt();
    };

    SC_TEST_EXPECT(server.start(loop, "127.0.0.1", report.mapPort(port)));
    SC_TEST_EXPECT(deadline.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(connect.start(loop, peer, address));
    SC_TEST_EXPECT(loop.run());

    SC_TEST_EXPECT(not ctx.timedOut);
    SC_TEST_EXPECT(ctx.peerEOF);
    SC_TEST_EXPECT(ctx.reclaimed);
    SC_TEST_EXPECT(ctx.requests == 1);
    SC_TEST_EXPECT(ctx.shutdowns == 1);
    SC_TEST_EXPECT(server.getConnections().getNumActiveConnections() == 0);
    SC_TEST_EXPECT(StringView({ctx.received, ctx.receivedSize}, false, StringEncoding::Ascii) ==
                   "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nOK");

    // The peer never closes or half-closes until server EOF and reclamation were observed.
    if (not receive.isFree())
    {
        SC_TEST_EXPECT(receive.stop(loop));
    }
    if (not checkReclaimed.isFree())
    {
        SC_TEST_EXPECT(checkReclaimed.stop(loop));
    }
    if (not deadline.isFree())
    {
        SC_TEST_EXPECT(deadline.stop(loop));
    }
    SC_TEST_EXPECT(server.stop());
    SC_TEST_EXPECT(server.close());
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(peer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpKeepAliveTest::runScenario(uint16_t port, int requests, bool defaultKeepAlive, int responseOverride,
                                        uint32_t maxRequests, bool terminal)
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    // A single pool slot would itself force keep-alive off.
    using ServerConnection = HttpAsyncConnection<2, 2, 8 * 1024, 1024>;
    ServerConnection connections[2];
    HttpAsyncServer  server;
    SC_TEST_EXPECT(server.init(Span<ServerConnection>(connections)));
    server.setDefaultKeepAlive(defaultKeepAlive);
    server.setMaxRequestsPerConnection(maxRequests);

    HttpAsyncClientConnection<2, 2, 8 * 1024, 1024> clientStorage;

    HttpAsyncClient client;
    SC_TEST_EXPECT(client.init(clientStorage));

    HttpTestHelpers::ResponseCollector collector;

    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/test", report.mapPort(port)));

    AsyncLoopTimeout deferredStep;
    AsyncLoopTimeout deadline;
    struct Context
    {
        AsyncEventLoop&  loop;
        HttpAsyncServer& server;
        HttpAsyncClient& client;

        HttpTestHelpers::ResponseCollector& collector;

        String& url;

        AsyncLoopTimeout& deferredStep;

        int requests;
        int responseOverride;

        bool defaultKeepAlive;
        bool terminal;

        int accepted        = 0;
        int serverRequests  = 0;
        int clientResponses = 0;
        int shutdowns       = 0;
        int shutdownOrdinal = 0;

        HttpConnection* acceptedConnection = nullptr;

        bool finished = false;
        bool timedOut = false;
    } ctx = {loop,         server,   client,           collector,        url,
             deferredStep, requests, responseOverride, defaultKeepAlive, terminal};

    SC_TEST_EXPECT(server.setTransportSetup({[&ctx](HttpAsyncServerTransportSetup& setup) -> Result
                                             {
                                                 ctx.accepted++;
                                                 ctx.acceptedConnection = setup.connection;
                                                 setup.complete(Result(true));
                                                 return Result(true);
                                             }}));
    Function<Result(HttpConnection&, Function<void(Result)>)> shutdown = {
        [&ctx](HttpConnection& connection, Function<void(Result)> complete) -> Result
        {
            ctx.shutdowns++;
            ctx.shutdownOrdinal = static_cast<int>(connection.requestCount + 1);
            complete(Result(true));
            return Result(true);
        }};
    SC_TEST_EXPECT(server.setTransportShutdown(move(shutdown)));
    server.onRequest = [this, &ctx](HttpConnection& connection)
    {
        SC_TEST_EXPECT(ctx.accepted == 1);
        SC_TEST_EXPECT(&connection == ctx.acceptedConnection);
        SC_TEST_EXPECT(connection.requestCount == static_cast<uint32_t>(ctx.serverRequests));
        ctx.serverRequests++;

        HttpResponse& response = connection.response;
        SC_TEST_EXPECT(response.getKeepAlive() == ctx.defaultKeepAlive);
        if (ctx.responseOverride >= 0)
        {
            response.setKeepAlive(ctx.responseOverride != 0);
        }
        SC_TEST_EXPECT(response.startResponse(200));
        SC_TEST_EXPECT(response.addHeader("Content-Length", "2"));
        SC_TEST_EXPECT(response.sendHeaders());
        SC_TEST_EXPECT(response.getWritableStream().write("OK"));
        SC_TEST_EXPECT(response.end());
    };

    // Never start another request from inside the response body's end notification.
    ctx.deferredStep.callback = [this, &ctx](AsyncLoopTimeout::Result&)
    {
        ctx.collector.detach();
        if (ctx.clientResponses < ctx.requests)
        {
            SC_TEST_EXPECT(ctx.client.get(ctx.loop, ctx.url.view(), true));
        }
        else if (not ctx.terminal or ctx.server.getConnections().getNumActiveConnections() == 0)
        {
            ctx.finished = true;
            ctx.loop.interrupt();
        }
        else
        {
            SC_TEST_EXPECT(ctx.deferredStep.start(ctx.loop, TimeMs{1}));
        }
    };
    ctx.client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completed)
                             {
                                 SC_TEST_EXPECT(completed.getParser().statusCode == 200);
                                 const bool expectedKeepAlive =
                                     ctx.responseOverride < 0 ? ctx.defaultKeepAlive : ctx.responseOverride != 0;
                                 SC_TEST_EXPECT(completed.getKeepAlive() == expectedKeepAlive);
                                 SC_TEST_EXPECT(StringView(ctx.collector.view()) == "OK");
                                 ctx.clientResponses++;
                                 SC_TEST_EXPECT(ctx.serverRequests == ctx.clientResponses);
                                 SC_TEST_EXPECT(ctx.deferredStep.start(ctx.loop, TimeMs{0}));
                             });
    };
    auto onError = [this, &loop](Result result)
    {
        SC_TEST_EXPECT(result);
        loop.interrupt();
    };
    server.onError    = onError;
    client.onError    = onError;
    deadline.callback = [&ctx, &loop](AsyncLoopTimeout::Result&)
    {
        ctx.timedOut = true;
        loop.interrupt();
    };

    SC_TEST_EXPECT(server.start(loop, "127.0.0.1", report.mapPort(port)));
    // Keep the deadline counted so a stalled listener is interrupted deterministically.
    SC_TEST_EXPECT(deadline.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view(), true));
    SC_TEST_EXPECT(loop.run());

    SC_TEST_EXPECT(not ctx.timedOut);
    SC_TEST_EXPECT(ctx.finished);
    SC_TEST_EXPECT(ctx.accepted == 1);
    SC_TEST_EXPECT(ctx.serverRequests == requests);
    SC_TEST_EXPECT(ctx.clientResponses == requests);
    SC_TEST_EXPECT(ctx.shutdowns == (terminal ? 1 : 0));
    if (terminal)
    {
        // This observes the server shutdown path and slot deactivation, not peer EOF.
        SC_TEST_EXPECT(ctx.shutdownOrdinal == requests);
        SC_TEST_EXPECT(server.getConnections().getNumActiveConnections() == 0);
    }
    else
    {
        SC_TEST_EXPECT(server.getConnections().getNumActiveConnections() == 1);
    }

    collector.detach();
    if (not deadline.isFree())
    {
        SC_TEST_EXPECT(deadline.stop(loop));
    }
    if (not deferredStep.isFree())
    {
        SC_TEST_EXPECT(deferredStep.stop(loop));
    }
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(server.stop());
    SC_TEST_EXPECT(server.close());
    SC_TEST_EXPECT(loop.close());
}

namespace SC
{
void runHttpKeepAliveTest(SC::TestReport& report) { HttpKeepAliveTest test(report); }
} // namespace SC
