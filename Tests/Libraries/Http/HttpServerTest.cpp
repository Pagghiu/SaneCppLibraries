// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpServer.h"
#include "Libraries/Http/HttpClient.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct HttpServerTest;
} // namespace SC

struct SC::HttpServerTest : public SC::TestCase
{
    HttpServerTest(SC::TestReport& report) : TestCase(report, "HttpServerTest")
    {
        if (test_section("HttpServer"))
        {
            httpServerTest();
        }
    }
    void httpServerTest();
};

void SC::HttpServerTest::httpServerTest()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    //! [HttpServerSnippet]
    constexpr int NUM_CLIENTS = 3;

    HttpServerClient   clients[NUM_CLIENTS];
    HttpServer::Memory serverMemory = {clients};
    HttpServer         server;
    SC_TEST_EXPECT(server.start(eventLoop, "127.0.0.1", 6152, serverMemory));

    struct ServerContext
    {
        int         numRequests;
        HttpServer& server;
    } serverContext = {0, server};

    server.onRequest = [this, &serverContext](HttpRequest& request, HttpResponse& response)
    {
        if (request.getParser().method != HttpParser::Method::HttpGET)
        {
            SC_TEST_EXPECT(response.startResponse(405));
            SC_TEST_EXPECT(response.end(""));
            return;
        }
        if (request.getURL() != "/index.html" and request.getURL() != "/")
        {
            SC_TEST_EXPECT(response.startResponse(404));
            SC_TEST_EXPECT(response.end(""));
            return;
        }
        serverContext.numRequests++;
        SC_TEST_EXPECT(response.startResponse(200));
        SC_TEST_EXPECT(response.addHeader("Connection", "Closed"));
        SC_TEST_EXPECT(response.addHeader("Content-Type", "text/html"));
        SC_TEST_EXPECT(response.addHeader("Server", "SC"));
        SC_TEST_EXPECT(response.addHeader("Date", "Mon, 27 Aug 2023 16:37:00 GMT"));
        SC_TEST_EXPECT(response.addHeader("Last-Modified", "Wed, 27 Aug 2023 16:37:00 GMT"));
        const char sampleHtml[] = "<html>\r\n"
                                  "<body bgcolor=\"#000000\" text=\"#ffffff\">\r\n"
                                  "<h1>This is a title {}!</h1>\r\n"
                                  "We must start from somewhere\r\n"
                                  "</body>\r\n"
                                  "</html>\r\n";
        String     str;
        SC_TEST_EXPECT(StringBuilder::format(str, sampleHtml, serverContext.numRequests));
        SC_TEST_EXPECT(response.end(str.view().toCharSpan()));
    };

    //! [HttpServerSnippet]

    HttpClient       client[3];
    SmallString<255> buffer;
    struct ClientContext
    {
        int         numRequests;
        HttpServer& server;
        int         wantedNumRequests = 3;
    } clientContext = {0, server};
    for (int idx = 0; idx < clientContext.wantedNumRequests; ++idx)
    {
        SC_TEST_EXPECT(StringBuilder::format(buffer, "HttpClient [{}]", idx));
        SC_TEST_EXPECT(client[idx].setCustomDebugName(buffer.view()));
        client[idx].callback = [this, &clientContext](HttpClient& client)
        {
            SC_TEST_EXPECT(StringView(client.getResponse()).containsString("This is a title"));
            clientContext.numRequests++;
            if (clientContext.numRequests == clientContext.wantedNumRequests)
            {
                SC_TEST_EXPECT(clientContext.server.stopAsync());
            }
        };
        SC_TEST_EXPECT(client[idx].get(eventLoop, "http://localhost:6152/index.html"));
    }
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(serverContext.numRequests == clientContext.wantedNumRequests);
    SC_TEST_EXPECT(clientContext.numRequests == clientContext.wantedNumRequests);
    SC_TEST_EXPECT(eventLoop.close());
}

namespace SC
{
void runHttpServerTest(SC::TestReport& report) { HttpServerTest test(report); }
} // namespace SC
