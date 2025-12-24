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
    constexpr int MAX_CONNECTIONS = 3;        // Max number of concurrent http connections
    constexpr int REQUEST_SIZE    = 1024;     // How many bytes are allocated to stream data for each connection
    constexpr int REQUEST_SLICES  = 2;        // Number of slices of the request buffer for each connection
    constexpr int HEADER_SIZE     = 8 * 1024; // How many bytes are dedicated to hold request and response headers
    constexpr int EXTRA_SLICES    = 2;        // Extra write slices needed to write headers buffer and user string

    // Note: All fixed arrays could be created dynamically at startup time with new / malloc.
    // Alternatively it's also possible to reserve memory for some insane large amount of clients (using VirtualMemory
    // class for example) and just dynamically commit as much memory as one needs to handle a given number of clients

    // 1. Memory for all http headers of all connections
    Buffer headersMemory;
    SC_TEST_EXPECT(headersMemory.resize(MAX_CONNECTIONS * HEADER_SIZE));

    // 2. Memory to hold all pre-registered / re-usable buffers used by the read and write queues.
    AsyncBufferView  buffers[MAX_CONNECTIONS * (REQUEST_SLICES + EXTRA_SLICES)];
    AsyncBuffersPool buffersPool;
    buffersPool.setBuffers(buffers);

    // Slice a buffer in equal parts to create re-usable slices of memory when streaming files.
    // It's not required to slice the buffer in equal parts, that's just an arbitrary choice.
    Buffer requestsMemory;
    SC_TEST_EXPECT(requestsMemory.resize(MAX_CONNECTIONS * REQUEST_SIZE));
    SC_TEST_EXPECT(
        AsyncBuffersPool::sliceInEqualParts(buffers, requestsMemory.toSpan(), MAX_CONNECTIONS * REQUEST_SLICES));

    // 3. Memory to hold all http connections
    using HttpConnectionType = HttpAsyncConnection<REQUEST_SLICES, REQUEST_SLICES>;
    HttpConnectionType connections[MAX_CONNECTIONS];

    // Initialize and start the http server
    HttpAsyncServer httpServer;
    SC_TEST_EXPECT(httpServer.init(buffersPool, Span<HttpConnectionType>(connections), headersMemory.toSpan()));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 6152));

    struct ServerContext
    {
        int numRequests;
    } serverContext = {0};

    // Handle the request and answer accordingly
    httpServer.onRequest = [this, &serverContext](HttpConnection& client)
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

        // Create a "user provided" dynamically allocated string, to show this is possible
        String content;
        SC_TEST_EXPECT(StringBuilder::format(content, sampleHtml, serverContext.numRequests));
        SmallString<16> contentLength;
        SC_TEST_EXPECT(StringBuilder::format(contentLength, "{}", content.view().sizeInBytes()));
        SC_TEST_EXPECT(response.addHeader("Content-Length", contentLength.view()));
        SC_TEST_EXPECT(response.sendHeaders());
        // Note that the system takes ownership of the dynamically allocated user provided string
        // through type erasure and it will invoke its destructor after th write operation will finish,
        // freeing user memory as expected.
        // This write operation succeeds because EXTRA_SLICES allocates one more slot buffer exactly
        // to hold this user provided buffer, that is not part of the "re-usable" buffers created
        // at the beginning of this sample.
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
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(serverContext.numRequests == clientContext.wantedNumRequests);
    SC_TEST_EXPECT(clientContext.numRequests == clientContext.wantedNumRequests);
    SC_TEST_EXPECT(eventLoop.close());
}

namespace SC
{
void runHttpAsyncServerTest(SC::TestReport& report) { HttpAsyncServerTest test(report); }
} // namespace SC
