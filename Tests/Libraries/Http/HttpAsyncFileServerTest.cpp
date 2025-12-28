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
    constexpr int MAX_CONNECTIONS = 16;       // Max number of concurrent http connections
    constexpr int REQUEST_SLICES  = 2;        // Number of slices of the request buffer for each connection
    constexpr int REQUEST_SIZE    = 1 * 1024; // How many bytes are allocated to stream data for each connection
    constexpr int HEADER_SIZE     = 8 * 1024; // How many bytes are dedicated to hold request and response headers
    constexpr int NUM_FS_THREADS  = 4;        // Number of threads in the thread pool for async file stream operations

    // This class is fixing buffer sizes at compile time for simplicity but it's possible to size them at runtime
    using HttpConnectionType = HttpAsyncConnection<REQUEST_SLICES, REQUEST_SLICES, HEADER_SIZE, REQUEST_SIZE>;

    // 1. Memory to hold all http connections (single array for simplicity).
    // WebServerExample (SCExample) shows how to leverage virtual memory, to handle dynamic number of clients
    HttpConnectionType connections[MAX_CONNECTIONS];

    // 2. Memory used by the async file streams started by file server.
    HttpAsyncFileServer::StreamQueue<REQUEST_SLICES> streams[MAX_CONNECTIONS];

    // Initialize and start the http and the file server
    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    ThreadPool threadPool;
    if (eventLoop.needsThreadPoolForFileOperations()) // no thread pool needed for io_uring
    {
        SC_TEST_EXPECT(threadPool.create(NUM_FS_THREADS));
    }
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 8090));
    SC_TEST_EXPECT(fileServer.init(threadPool, eventLoop, webServerFolder));

    // Forward all http requests to the file server in order to serve files
    httpServer.onRequest = [&](HttpConnection& connection)
    { SC_ASSERT_RELEASE(fileServer.serveFile(streams[connection.getConnectionID().getIndex()], connection)); };
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
    SC_TEST_EXPECT(fileServer.close());
    SC_TEST_EXPECT(httpServer.close());

    // Remove the test file
    SC_TEST_EXPECT(fs.removeFile("file.html"));
}

namespace SC
{
void runHttpAsyncFileServerTest(SC::TestReport& report) { HttpAsyncFileServerTest test(report); }
} // namespace SC
