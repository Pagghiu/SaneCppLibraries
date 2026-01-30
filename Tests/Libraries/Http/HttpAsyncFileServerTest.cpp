// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpAsyncFileServer.h"
#include "HttpClient.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Memory/String.h"
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
    StringView     webServerFolder = report.applicationRootDirectory.view();
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    //! [HttpFileServerSnippet]
    constexpr int MAX_CONNECTIONS = 1;        // Max number of concurrent http connections (1 disables keep-alive)
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
    { SC_ASSERT_RELEASE(fileServer.handleRequest(streams[connection.getConnectionID().getIndex()], connection)); };
    //! [HttpFileServerSnippet]

    struct Context
    {
        HttpAsyncServer& httpServer;
        AsyncEventLoop*  loop = nullptr;

        int        getCount       = 0;
        int        putCount       = 0;
        int        multipartCount = 0;
        FileSystem fs             = {};
        HttpClient getClient      = {};
        HttpClient putStream      = {};
        HttpClient putInline      = {};
        HttpClient postMultipart  = {};
    } context    = {httpServer};
    context.loop = &eventLoop;

    SC_TEST_EXPECT(context.fs.init(webServerFolder));
    SC_TEST_EXPECT(context.fs.writeString("file.html", "<html><body>Response from file</body></html>"));

    // Create an Http Client request for that file
    SC_TEST_EXPECT(context.getClient.get(eventLoop, "http://localhost:8090/file.html"));
    context.getClient.callback = [this, &context](HttpClient& result)
    {
        context.getCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("Response from file"));

        // Test PUT with a 10 ms delay between headers and body to induce two separate reads on the receiving side
        // one with the headers, and one with the body contents, that will trigger the pipeline streaming code path.
        SC_TEST_EXPECT(context.putStream.put(*context.loop, "http://localhost:8090/stream.html", "StreamBody", {10}));
    };

    context.putStream.callback = [this, &context](HttpClient& result)
    {
        context.putCount++;
        StringView str(result.getResponse());
        // Expect 201 Created and Content-Length: 0
        SC_TEST_EXPECT(str.containsString("201 Created"));

        // Verify file content
        String content;
        SC_TEST_EXPECT(context.fs.read("stream.html", content));
        SC_TEST_EXPECT(content == "StreamBody");
        SC_TEST_EXPECT(context.fs.removeFile("stream.html"));

        // Test PUT writing headers and body content in a single write, that will avoid the pipeline streaming code path
        // HttpRequest::getFirstBodySlice() will contain the entire body contents.
        SC_TEST_EXPECT(context.putInline.put(*context.loop, "http://localhost:8090/inline.html", "InlineBody"));
    };
    context.putInline.callback = [this, &context](HttpClient& result)
    {
        context.putCount++;
        StringView str(result.getResponse());
        // Expect 201 Created and Content-Length: 0
        SC_TEST_EXPECT(str.containsString("201 Created"));

        // Verify file content
        String content;
        SC_TEST_EXPECT(context.fs.read("inline.html", content));
        SC_TEST_EXPECT(content == "InlineBody");
        SC_TEST_EXPECT(context.fs.removeFile("inline.html"));

        // Test multipart POST with file upload
        SC_TEST_EXPECT(context.postMultipart.postMultipart(*context.loop, "http://localhost:8090/upload", "file",
                                                           "multipart.txt", "MultipartContent"));
    };

    context.postMultipart.callback = [this, &context](HttpClient& result)
    {
        context.multipartCount++;
        StringView str(result.getResponse());
        // Expect 201 Created
        SC_TEST_EXPECT(str.containsString("201 Created"));

        // Verify file content
        String content;
        SC_TEST_EXPECT(context.fs.read("multipart.txt", content));
        SC_TEST_EXPECT(content == "MultipartContent");
        SC_TEST_EXPECT(context.fs.removeFile("multipart.txt"));

        SC_TEST_EXPECT(context.httpServer.stop());
    };

    // Safety timout against hangs
    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(fileServer.close());
    SC_TEST_EXPECT(httpServer.close());

    SC_TEST_EXPECT(context.getCount == 1);
    SC_TEST_EXPECT(context.putCount == 2);
    SC_TEST_EXPECT(context.multipartCount == 1);
    SC_TEST_EXPECT(context.fs.removeFile("file.html"));
}

namespace SC
{
void runHttpAsyncFileServerTest(SC::TestReport& report) { HttpAsyncFileServerTest test(report); }
} // namespace SC
