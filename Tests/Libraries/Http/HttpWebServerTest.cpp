// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpWebServer.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Http/HttpClient.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct HttpWebServerTest;
} // namespace SC

struct SC::HttpWebServerTest : public SC::TestCase
{
    HttpWebServerTest(SC::TestReport& report) : TestCase(report, "HttpWebServerTest")
    {
        if (test_section("HttpWebServer"))
        {
            httpWebServerTest();
        }
    }
    void httpWebServerTest();
};

void SC::HttpWebServerTest::httpWebServerTest()
{
    // Create a test file in the application root directory
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
    SC_TEST_EXPECT(fs.write("file.html", "<html><body>Response from file</body></html>"));
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    //! [HttpWebServerSnippet]
    constexpr int NUM_CLIENTS = 16;
    Buffer        headersMemory;
    SC_TEST_EXPECT(headersMemory.resize(NUM_CLIENTS * 8 * 1024));

    Buffer requestsMemory;
    SC_TEST_EXPECT(requestsMemory.resize(NUM_CLIENTS * 1024 * 2));

    HttpServerClient clients[NUM_CLIENTS];

    GrowableBuffer<Buffer> headers      = {headersMemory};
    HttpServer::Memory     serverMemory = {headers, clients};

    HttpAsyncServer asyncServer;
    HttpWebServer   webServer;
    // Creates an HttpServer that serves files from application root directory
    SC_TEST_EXPECT(asyncServer.start(eventLoop, "127.0.0.1", 8090, serverMemory));
    SC_TEST_EXPECT(webServer.init(report.applicationRootDirectory.view()));

    asyncServer.httpServer.onRequest = [&](HttpRequest& req, HttpResponse& res) { webServer.serveFile(req, res); };
    //! [HttpWebServerSnippet]

    struct Context
    {
        int              numRequests = 0;
        HttpWebServer&   httpWebServer;
        HttpAsyncServer& httpAsyncServer;
    } context = {0, webServer, asyncServer};

    // Create an Http Client request for that file
    HttpClient client;
    SC_TEST_EXPECT(client.get(eventLoop, "http://localhost:8090/file.html"));
    client.callback = [this, &context](HttpClient& result)
    {
        context.numRequests++;
        SC_TEST_EXPECT(StringView(result.getResponse()).containsString("Response from file"));
        SC_TEST_EXPECT(context.httpAsyncServer.stopAsync());
        SC_TEST_EXPECT(context.httpWebServer.stopAsync());
    };
    SC_TEST_EXPECT(eventLoop.run());

    // Remove the test file
    SC_TEST_EXPECT(fs.removeFile("file.html"));
}

namespace SC
{
void runHttpWebServerTest(SC::TestReport& report) { HttpWebServerTest test(report); }
} // namespace SC
