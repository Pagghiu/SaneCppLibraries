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
    StringView webServerFolder = report.applicationRootDirectory.view();
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(webServerFolder));
    SC_TEST_EXPECT(fs.write("file.html", "<html><body>Response from file</body></html>"));
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    //! [HttpWebServerSnippet]
    constexpr int NUM_CLIENTS    = 16;
    constexpr int REQUEST_SLICES = 2;
    constexpr int CLIENT_REQUEST = 1024;

    Buffer headersMemory;
    SC_TEST_EXPECT(headersMemory.resize(NUM_CLIENTS * 8 * CLIENT_REQUEST));

    Buffer requestsMemory;
    SC_TEST_EXPECT(requestsMemory.resize(NUM_CLIENTS * CLIENT_REQUEST * 2));

    HttpServerClient clients[NUM_CLIENTS];

    AsyncBufferView buffers[NUM_CLIENTS * (REQUEST_SLICES + 2)]; // +2 to accommodate some slots for external bufs

    AsyncReadableStream::Request readQueue[NUM_CLIENTS * REQUEST_SLICES];
    AsyncWritableStream::Request writeQueue[NUM_CLIENTS * REQUEST_SLICES];

    Span<char> requestsSpan = requestsMemory.toSpan();
    for (size_t idx = 0; idx < NUM_CLIENTS; ++idx)
    {
        for (size_t slice = 0; slice < REQUEST_SLICES; ++slice)
        {
            Span<char>   memory;
            const size_t offset = idx * CLIENT_REQUEST + slice * CLIENT_REQUEST / REQUEST_SLICES;
            SC_TEST_EXPECT(requestsSpan.sliceStartLength(offset, CLIENT_REQUEST / REQUEST_SLICES, memory));
            buffers[idx * REQUEST_SLICES + slice] = memory;
            buffers[idx * REQUEST_SLICES + slice].setReusable(true); // We want to recycle these buffers
        }
    }

    HttpAsyncServer asyncServer;
    HttpWebServer   webServer;

    SC_TEST_EXPECT(asyncServer.init(clients, headersMemory.toSpan(), readQueue, writeQueue, buffers));
    // Creates an HttpServer that serves files from application root directory
    SC_TEST_EXPECT(asyncServer.start(eventLoop, "127.0.0.1", 8090));

    HttpWebServerStream streams[NUM_CLIENTS];

    SC_TEST_EXPECT(webServer.init(webServerFolder, streams, asyncServer.getBuffersPool(), eventLoop));

    webServer.serveFilesOn(asyncServer.getHttpServer());

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
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("Response from file"));
        SC_TEST_EXPECT(context.httpAsyncServer.stop());
    };
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(asyncServer.waitForStopToFinish());

    // Remove the test file
    SC_TEST_EXPECT(fs.removeFile("file.html"));
}

namespace SC
{
void runHttpWebServerTest(SC::TestReport& report) { HttpWebServerTest test(report); }
} // namespace SC
