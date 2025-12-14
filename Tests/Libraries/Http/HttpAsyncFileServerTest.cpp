// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpAsyncFileServer.h"
#include "HttpClient.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct HttpAsyncFileServerTest;
} // namespace SC

struct SC::HttpAsyncFileServerTest : public SC::TestCase
{
    HttpAsyncFileServerTest(SC::TestReport& report) : TestCase(report, "HttpAsyncFileServerTest")
    {
        if (test_section("HttpAsyncFileServer"))
        {
            httpFileServerTest();
        }
    }
    void httpFileServerTest();
};

void SC::HttpAsyncFileServerTest::httpFileServerTest()
{
    // Create a test file in the application root directory
    StringView webServerFolder = report.applicationRootDirectory.view();
    FileSystem fs;
    SC_TEST_EXPECT(fs.init(webServerFolder));
    SC_TEST_EXPECT(fs.write("file.html", "<html><body>Response from file</body></html>"));
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    //! [HttpFileServerSnippet]
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

    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    SC_TEST_EXPECT(httpServer.init(clients, headersMemory.toSpan(), readQueue, writeQueue, buffers));
    // Creates an HttpServer that serves files from application root directory
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 8090));

    HttpAsyncFileServerStream streams[NUM_CLIENTS];

    SC_TEST_EXPECT(fileServer.init(webServerFolder, streams, httpServer.getBuffersPool(), eventLoop));

    fileServer.serveFilesOn(httpServer.getHttpServer());

    //! [HttpFileServerSnippet]

    struct Context
    {
        int              numRequests = 0;
        HttpAsyncServer& httpServer;
    } context = {0, httpServer};

    // Create an Http Client request for that file
    HttpClient client;
    SC_TEST_EXPECT(client.get(eventLoop, "http://localhost:8090/file.html"));
    client.callback = [this, &context](HttpClient& result)
    {
        context.numRequests++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("Response from file"));
        SC_TEST_EXPECT(context.httpServer.stop());
    };
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.waitForStopToFinish());

    // Remove the test file
    SC_TEST_EXPECT(fs.removeFile("file.html"));
}

namespace SC
{
void runHttpAsyncFileServerTest(SC::TestReport& report) { HttpAsyncFileServerTest test(report); }
} // namespace SC
