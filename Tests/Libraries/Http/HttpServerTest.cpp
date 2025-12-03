// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpAsyncServer.h"
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
        if (test_section("HttpServer Async"))
        {
            httpServerTest(false);
        }
        if (test_section("HttpServer AsyncStreams"))
        {
            httpServerTest(true);
        }
    }
    void httpServerTest(bool useAsyncStreams);
};

void SC::HttpServerTest::httpServerTest(bool useAsyncStreams)
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());
    constexpr int NUM_CLIENTS    = 3;
    constexpr int CLIENT_HEADERS = 8 * 1024;
    constexpr int CLIENT_REQUEST = 1024;
    constexpr int REQUEST_SLICES = 2;

    Buffer requestsMemory;
    SC_TEST_EXPECT(requestsMemory.resize(NUM_CLIENTS * CLIENT_REQUEST));

    AsyncBufferView  buffers[NUM_CLIENTS * (REQUEST_SLICES + 2)]; // +2 to accomodate some slots for external bufs
    HttpServerClient clients[NUM_CLIENTS];

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
        }
    }

    //! [HttpServerSnippet]
    // constexpr int NUM_CLIENTS    = 3;
    // constexpr int CLIENT_HEADERS = 8 * 1024;

    Buffer headersMemory;
    SC_TEST_EXPECT(headersMemory.resize(NUM_CLIENTS * CLIENT_HEADERS));

    GrowableBuffer<Buffer> headers = {headersMemory};
    HttpServer::Memory     memory  = {headers, clients};
    HttpAsyncServer        server;

    SC_TEST_EXPECT(server.start(eventLoop, "127.0.0.1", 6152, memory));

    struct ServerContext
    {
        int numRequests;
    } serverContext = {0};

    server.httpServer.onRequest = [this, &serverContext](HttpRequest& request, HttpResponse& response)
    {
        if (request.getParser().method != HttpParser::Method::HttpGET)
        {
            SC_TEST_EXPECT(response.startResponse(405));
            SC_TEST_EXPECT(response.end());
            return;
        }
        if (request.getURL() != "/index.html" and request.getURL() != "/")
        {
            SC_TEST_EXPECT(response.startResponse(404));
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
        String     str;
        SC_TEST_EXPECT(StringBuilder::format(str, sampleHtml, serverContext.numRequests));
        SC_TEST_EXPECT(response.end(str.view().toCharSpan()));
    };

    //! [HttpServerSnippet]
    if (useAsyncStreams)
    {
        server.setUseAsyncStreams(true, readQueue, writeQueue, buffers);
    }

    HttpClient       client[3];
    SmallString<255> buffer;
    struct ClientContext
    {
        int numRequests;
        int wantedNumRequests;

        HttpAsyncServer& asyncServer;
    } clientContext = {0, 3, server};
    for (int idx = 0; idx < clientContext.wantedNumRequests; ++idx)
    {
        SC_TEST_EXPECT(StringBuilder::format(buffer, "HttpClient [{}]", idx));
        SC_TEST_EXPECT(client[idx].setCustomDebugName(buffer.view()));
        client[idx].callback = [this, &clientContext](HttpClient& client)
        {
            StringView response(client.getResponse());
            SC_TEST_EXPECT(response.containsString("This is a title"));
            clientContext.numRequests++;
            if (clientContext.numRequests == clientContext.wantedNumRequests)
            {
                SC_TEST_EXPECT(clientContext.asyncServer.stopAsync());
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
