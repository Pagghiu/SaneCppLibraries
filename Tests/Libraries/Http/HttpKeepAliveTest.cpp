// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpClient.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Socket/Socket.h"
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
            keepAliveMultipleRequests();
        }
        if (test_section("keep-alive disabled by response"))
        {
            keepAliveDisabledByResponse();
        }
        if (test_section("keep-alive max requests"))
        {
            keepAliveMaxRequests();
        }
        if (test_section("keep-alive server default disabled"))
        {
            keepAliveServerDefaultDisabled();
        }
    }

    void keepAliveMultipleRequests();
    void keepAliveDisabledByResponse();
    void keepAliveMaxRequests();
    void keepAliveServerDefaultDisabled();
};

void SC::HttpKeepAliveTest::keepAliveMultipleRequests()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    constexpr int MAX_CONNECTIONS = 1;
    constexpr int REQUEST_SLICES  = 2;
    constexpr int REQUEST_SIZE    = 1 * 1024;
    constexpr int HEADER_SIZE     = 8 * 1024;

    using HttpConnectionType = HttpAsyncConnection<REQUEST_SLICES, REQUEST_SLICES, HEADER_SIZE, REQUEST_SIZE>;
    HttpConnectionType connections[MAX_CONNECTIONS];

    HttpAsyncServer httpServer;
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 6160));

    struct Context
    {
        HttpKeepAliveTest* self;
        int                numServerRequests  = 0;
        int                numClientResponses = 0;
        AsyncEventLoop*    eventLoop;
        HttpAsyncServer*   httpServer;
    } ctx = {this, 0, 0, &eventLoop, &httpServer};

    httpServer.onRequest = [&ctx, this](HttpConnection& client)
    {
        ctx.numServerRequests++;
        HttpResponse& response = client.response;
        SC_TEST_EXPECT(response.startResponse(200));
        SC_TEST_EXPECT(response.addHeader("Content-Length", "2"));
        SC_TEST_EXPECT(response.sendHeaders());
        SC_TEST_EXPECT(response.getWritableStream().write("OK"));
        SC_TEST_EXPECT(response.end());
    };

    // Make 3 sequential requests on the same connection
    HttpClient client;

    client.callback = [&ctx, this](HttpClient& c)
    {
        ctx.numClientResponses++;
        StringView response(c.getResponse());
        SC_TEST_EXPECT(response.containsString("OK"));

        if (ctx.numClientResponses < 3)
        {
            // Make another request (reusing same client)
            SC_TEST_EXPECT(c.get(*ctx.eventLoop, "http://localhost:6160/test", true));
        }
        else
        {
            SC_TEST_EXPECT(ctx.httpServer->stop());
        }
    };

    SC_TEST_EXPECT(client.get(eventLoop, "http://localhost:6160/test"));
    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());

    // Verify 3 requests were handled
    SC_TEST_EXPECT(ctx.numServerRequests == 3);
    SC_TEST_EXPECT(ctx.numClientResponses == 3);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpKeepAliveTest::keepAliveDisabledByResponse()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    constexpr int MAX_CONNECTIONS = 2;
    constexpr int REQUEST_SLICES  = 2;
    constexpr int REQUEST_SIZE    = 1 * 1024;
    constexpr int HEADER_SIZE     = 8 * 1024;

    using HttpConnectionType = HttpAsyncConnection<REQUEST_SLICES, REQUEST_SLICES, HEADER_SIZE, REQUEST_SIZE>;
    HttpConnectionType connections[MAX_CONNECTIONS];

    HttpAsyncServer httpServer;
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 6161));

    struct Context
    {
        HttpKeepAliveTest* self;
        int                numServerRequests  = 0;
        int                numClientResponses = 0;
        HttpAsyncServer*   httpServer;
    } ctx = {this, 0, 0, &httpServer};

    httpServer.onRequest = [&ctx, this](HttpConnection& client)
    {
        ctx.numServerRequests++;
        HttpResponse& response = client.response;
        response.setKeepAlive(false); // Force close
        SC_TEST_EXPECT(response.startResponse(200));
        SC_TEST_EXPECT(response.addHeader("Content-Length", "2"));
        SC_TEST_EXPECT(response.sendHeaders());
        SC_TEST_EXPECT(response.getWritableStream().write("OK"));
        SC_TEST_EXPECT(response.end());
    };

    HttpClient client;

    client.callback = [&ctx, this](HttpClient& c)
    {
        ctx.numClientResponses++;
        StringView response(c.getResponse());
        SC_TEST_EXPECT(response.containsString("OK"));
        SC_TEST_EXPECT(ctx.httpServer->stop());
    };

    SC_TEST_EXPECT(client.get(eventLoop, "http://localhost:6161/test"));
    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());

    // Connection was closed, only 1 request
    SC_TEST_EXPECT(ctx.numServerRequests == 1);
    SC_TEST_EXPECT(ctx.numClientResponses == 1);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpKeepAliveTest::keepAliveMaxRequests()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    constexpr int MAX_CONNECTIONS = 1;
    constexpr int REQUEST_SLICES  = 2;
    constexpr int REQUEST_SIZE    = 1 * 1024;
    constexpr int HEADER_SIZE     = 8 * 1024;

    using HttpConnectionType = HttpAsyncConnection<REQUEST_SLICES, REQUEST_SLICES, HEADER_SIZE, REQUEST_SIZE>;
    HttpConnectionType connections[MAX_CONNECTIONS];

    HttpAsyncServer httpServer;
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    httpServer.setMaxRequestsPerConnection(2); // Max 2 requests per connection
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 6162));

    struct Context
    {
        HttpKeepAliveTest* self;
        int                numServerRequests  = 0;
        int                numClientResponses = 0;
        AsyncEventLoop*    eventLoop;
        HttpAsyncServer*   httpServer;
    } ctx = {this, 0, 0, &eventLoop, &httpServer};

    httpServer.onRequest = [&ctx, this](HttpConnection& client)
    {
        ctx.numServerRequests++;
        HttpResponse& response = client.response;
        SC_TEST_EXPECT(response.startResponse(200));
        SC_TEST_EXPECT(response.addHeader("Content-Length", "2"));
        SC_TEST_EXPECT(response.sendHeaders());
        SC_TEST_EXPECT(response.getWritableStream().write("OK"));
        SC_TEST_EXPECT(response.end());
    };

    HttpClient client;

    client.callback = [&ctx, this](HttpClient& c)
    {
        ctx.numClientResponses++;
        StringView response(c.getResponse());
        SC_TEST_EXPECT(response.containsString("OK"));

        if (ctx.numClientResponses < 2)
        {
            // Make another request
            SC_TEST_EXPECT(c.get(*ctx.eventLoop, "http://localhost:6162/test", true));
        }
        else
        {
            SC_TEST_EXPECT(ctx.httpServer->stop());
        }
    };

    SC_TEST_EXPECT(client.get(eventLoop, "http://localhost:6162/test"));
    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());

    // Max 2 requests enforced
    SC_TEST_EXPECT(ctx.numServerRequests == 2);
    SC_TEST_EXPECT(ctx.numClientResponses == 2);
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpKeepAliveTest::keepAliveServerDefaultDisabled()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    constexpr int MAX_CONNECTIONS = 2;
    constexpr int REQUEST_SLICES  = 2;
    constexpr int REQUEST_SIZE    = 1 * 1024;
    constexpr int HEADER_SIZE     = 8 * 1024;

    using HttpConnectionType = HttpAsyncConnection<REQUEST_SLICES, REQUEST_SLICES, HEADER_SIZE, REQUEST_SIZE>;
    HttpConnectionType connections[MAX_CONNECTIONS];

    HttpAsyncServer httpServer;
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    httpServer.setDefaultKeepAlive(false); // Disable keep-alive server-wide
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 6163));

    struct Context
    {
        HttpKeepAliveTest* self;
        int                numServerRequests  = 0;
        int                numClientResponses = 0;
        HttpAsyncServer*   httpServer;
    } ctx = {this, 0, 0, &httpServer};

    httpServer.onRequest = [&ctx, this](HttpConnection& client)
    {
        ctx.numServerRequests++;
        HttpResponse& response = client.response;
        SC_TEST_EXPECT(response.startResponse(200));
        SC_TEST_EXPECT(response.addHeader("Content-Length", "2"));
        SC_TEST_EXPECT(response.sendHeaders());
        SC_TEST_EXPECT(response.getWritableStream().write("OK"));
        SC_TEST_EXPECT(response.end());
    };

    HttpClient client;

    client.callback = [&ctx, this](HttpClient& c)
    {
        ctx.numClientResponses++;
        StringView response(c.getResponse());
        SC_TEST_EXPECT(response.containsString("OK"));
        SC_TEST_EXPECT(ctx.httpServer->stop());
    };

    SC_TEST_EXPECT(client.get(eventLoop, "http://localhost:6163/test"));
    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());

    // Server default is no keep-alive, only 1 request
    SC_TEST_EXPECT(ctx.numServerRequests == 1);
    SC_TEST_EXPECT(ctx.numClientResponses == 1);
    SC_TEST_EXPECT(eventLoop.close());
}

namespace SC
{
void runHttpKeepAliveTest(SC::TestReport& report) { HttpKeepAliveTest test(report); }
} // namespace SC
