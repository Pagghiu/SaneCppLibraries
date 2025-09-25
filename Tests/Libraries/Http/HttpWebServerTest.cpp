// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpWebServer.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Http/HttpClient.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct HttpWebServerTest;
} // namespace SC

struct SC::HttpWebServerTest : public SC::TestCase
{
    HttpWebServerTest(SC::TestReport& report) : TestCase(report, "HttpServerTest")
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
    // Creates an HttpServer that serves files from application root directory
    HttpServer    server;
    HttpWebServer webServer;
    SC_TEST_EXPECT(server.start(eventLoop, 16, "127.0.0.1", 8090));
    SC_TEST_EXPECT(webServer.init(report.applicationRootDirectory.view()));

    server.onRequest = [&](HttpRequest& req, HttpResponse& res) { webServer.serveFile(req, res); };
    //! [HttpWebServerSnippet]

    struct Context
    {
        int            numRequests = 0;
        HttpWebServer& httpWebServer;
        HttpServer&    httpServer;
    } context = {0, webServer, server};

    // Create an Http Client request for that file
    HttpClient client;
    SC_TEST_EXPECT(client.get(eventLoop, "http://localhost:8090/file.html"));
    client.callback = [this, &context](HttpClient& result)
    {
        context.numRequests++;
        SC_TEST_EXPECT(result.getResponse().containsString("Response from file"));
        SC_TEST_EXPECT(context.httpServer.stopAsync());
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
