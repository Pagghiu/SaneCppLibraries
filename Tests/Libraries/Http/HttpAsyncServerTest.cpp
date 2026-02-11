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
    constexpr int REQUEST_SLICES  = 2;        // Number of slices of the request buffer for each connection
    constexpr int REQUEST_SIZE    = 1 * 1024; // How many bytes are allocated to stream data for each connection
    constexpr int HEADER_SIZE     = 8 * 1024; // How many bytes are dedicated to hold request and response headers

    // The size of the header and request memory, and length of read/write queues are fixed here, but user can
    // set any arbitrary size for such queues doing the same being done in HttpAsyncConnection constructor.
    using HttpConnectionType = HttpAsyncConnection<REQUEST_SLICES, REQUEST_SLICES, HEADER_SIZE, REQUEST_SIZE>;

    // 1. Memory to hold all http connections (single array for simplicity).
    // WebServerExample (SCExample) shows how to leverage virtual memory, to handle dynamic number of clients
    HttpConnectionType connections[MAX_CONNECTIONS];

    // Initialize and start the http server
    HttpAsyncServer httpServer;
    const uint16_t  serverPort = report.mapPort(6152);
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

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
            SC_TEST_EXPECT(response.addHeader("Allow", "GET"));
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
    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://localhost:{}/index.html", serverPort));
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
        SC_TEST_EXPECT(client[idx].get(eventLoop, endpoint.view()));
    }

    // Safety timout against hangs
    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

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
