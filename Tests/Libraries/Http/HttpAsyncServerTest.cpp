// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpAsyncServer.h"
#include "HttpClient.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct HttpAsyncServerTest;
} // namespace SC

struct SC::HttpAsyncServerTest : public SC::TestCase
{
    HttpAsyncServerTest(SC::TestReport& report) : TestCase(report, "HttpAsyncServerTest")
    {
        if (test_section("HttpAsyncServer"))
        {
            httpAsyncServerTest();
        }
    }
    void httpAsyncServerTest();
};

void SC::HttpAsyncServerTest::httpAsyncServerTest()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    //! [HttpAsyncServerSnippet]
    constexpr int NUM_CLIENTS    = 3;
    constexpr int CLIENT_HEADERS = 8 * 1024;
    constexpr int CLIENT_REQUEST = 1024;
    constexpr int REQUEST_SLICES = 2;

    AsyncBufferView  buffers[NUM_CLIENTS * (REQUEST_SLICES + 2)]; // +2 to accommodate some slots for external bufs
    HttpServerClient clients[NUM_CLIENTS];

    AsyncReadableStream::Request readQueue[NUM_CLIENTS * REQUEST_SLICES];
    AsyncWritableStream::Request writeQueue[NUM_CLIENTS * REQUEST_SLICES];

    Buffer headersMemory;
    SC_TEST_EXPECT(headersMemory.resize(NUM_CLIENTS * CLIENT_HEADERS));

    Buffer requestsMemory;
    SC_TEST_EXPECT(requestsMemory.resize(NUM_CLIENTS * CLIENT_REQUEST));
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

    HttpAsyncServer httpServer;
    SC_TEST_EXPECT(httpServer.init(clients, headersMemory.toSpan(), readQueue, writeQueue, buffers));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 6152));

    struct ServerContext
    {
        int numRequests;
    } serverContext = {0};

    httpServer.getHttpServer().onRequest = [this, &serverContext](HttpServerClient& client)
    {
        HttpRequest&  request  = client.request;
        HttpResponse& response = client.response;
        if (request.getParser().method != HttpParser::Method::HttpGET)
        {
            SC_TEST_EXPECT(response.startResponse(405));
            SC_TEST_EXPECT(response.sendHeaders());
            SC_TEST_EXPECT(response.end());
            return;
        }
        if (request.getURL() != "/index.html" and request.getURL() != "/")
        {
            SC_TEST_EXPECT(response.startResponse(404));
            SC_TEST_EXPECT(response.sendHeaders());
            SC_TEST_EXPECT(response.end());
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

        String content;
        SC_TEST_EXPECT(StringBuilder::format(content, sampleHtml, serverContext.numRequests));
        SmallString<16> contentLength;
        SC_TEST_EXPECT(StringBuilder::format(contentLength, "{}", content.view().sizeInBytes()));
        SC_TEST_EXPECT(response.addHeader("Content-Length", contentLength.view()));
        SC_TEST_EXPECT(response.sendHeaders());
        SC_TEST_EXPECT(response.getWritableStream().write(move(content)));
        SC_TEST_EXPECT(response.end());
    };

    //! [HttpAsyncServerSnippet]

    HttpClient client[3];
    struct ClientContext
    {
        int numRequests;
        int wantedNumRequests;

        HttpAsyncServer& httpServer;
    } clientContext = {0, 3, httpServer};
    for (int idx = 0; idx < clientContext.wantedNumRequests; ++idx)
    {
        client[idx].callback = [this, &clientContext](HttpClient& client)
        {
            StringView response(client.getResponse());
            SC_TEST_EXPECT(response.containsString("This is a title"));
            clientContext.numRequests++;
            if (clientContext.numRequests == clientContext.wantedNumRequests)
            {
                SC_TEST_EXPECT(clientContext.httpServer.stop());
            }
        };
        SC_TEST_EXPECT(client[idx].get(eventLoop, "http://localhost:6152/index.html"));
    }
    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.waitForStopToFinish());
    SC_TEST_EXPECT(serverContext.numRequests == clientContext.wantedNumRequests);
    SC_TEST_EXPECT(clientContext.numRequests == clientContext.wantedNumRequests);
    SC_TEST_EXPECT(eventLoop.close());
}

namespace SC
{
void runHttpAsyncServerTest(SC::TestReport& report) { HttpAsyncServerTest test(report); }
} // namespace SC
