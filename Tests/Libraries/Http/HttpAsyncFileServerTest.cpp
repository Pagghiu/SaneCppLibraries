// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpAsyncFileServer.h"
#include "HttpTestClient.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

#include <string.h>

namespace SC
{
struct HttpAsyncFileServerTest;
} // namespace SC

struct SC::HttpAsyncFileServerTest : public SC::TestCase
{
    HttpAsyncFileServerTest(SC::TestReport& report) : TestCase(report, "HttpAsyncFileServerTest")
    {
        if (test_section("AsyncFileSend=true"))
        {
            httpFileServerTest(true);
        }
        if (test_section("AsyncFileSend=false"))
        {
            httpFileServerTest(false);
        }
    }
    void httpFileServerTest(bool useAsyncFileSend);
};

void SC::HttpAsyncFileServerTest::httpFileServerTest(bool useAsyncFileSend)
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
    const uint16_t      serverPort = report.mapPort(8090);

    ThreadPool threadPool;
    if (eventLoop.needsThreadPoolForFileOperations()) // no thread pool needed for io_uring
    {
        SC_TEST_EXPECT(threadPool.create(NUM_FS_THREADS));
    }
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));
    SC_TEST_EXPECT(fileServer.init(threadPool, eventLoop, webServerFolder));
    fileServer.setUseAsyncFileSend(useAsyncFileSend);

    // Forward all http requests to the file server in order to serve files
    httpServer.onRequest = [&](HttpConnection& connection)
    { SC_ASSERT_RELEASE(fileServer.handleRequest(streams[connection.getConnectionID().getIndex()], connection)); };
    //! [HttpFileServerSnippet]

    struct Context
    {
        HttpAsyncServer& httpServer;
        AsyncEventLoop*  loop = nullptr;
        FileSystem       fs   = {};

        int getCount       = 0;
        int putCount       = 0;
        int multipartCount = 0;
        int safetyCount    = 0;
        int mimeCount      = 0;

        HttpTestClient queryClient   = {};
        HttpTestClient badPathClient = {};
        HttpTestClient mimeClient    = {};
        HttpTestClient getClient     = {};
        HttpTestClient putStream     = {};
        HttpTestClient putInline     = {};
        HttpTestClient postMultipart = {};
        Buffer         multipartPayload;

        String fileURL   = StringEncoding::Ascii;
        String queryURL  = StringEncoding::Ascii;
        String webpURL   = StringEncoding::Ascii;
        String serverURL = StringEncoding::Ascii;
        String streamURL = StringEncoding::Ascii;
        String inlineURL = StringEncoding::Ascii;
        String uploadURL = StringEncoding::Ascii;
    } context    = {httpServer};
    context.loop = &eventLoop;

    SC_TEST_EXPECT(context.fs.init(webServerFolder));
    SC_TEST_EXPECT(context.fs.writeString("file.html", "<html><body>Response from file</body></html>"));
    SC_TEST_EXPECT(context.fs.writeString("asset.webp", "webp"));

    SC_TEST_EXPECT(StringBuilder::format(context.fileURL, "http://127.0.0.1:{}/file.html", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.queryURL, "http://127.0.0.1:{}/file.html?download=1", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.webpURL, "http://127.0.0.1:{}/asset.webp", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.serverURL, "http://127.0.0.1:{}", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.streamURL, "http://127.0.0.1:{}/stream.html", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.inlineURL, "http://127.0.0.1:{}/inline.html", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.uploadURL, "http://127.0.0.1:{}/upload", serverPort));

    // Query strings must not be treated as part of the file name.
    SC_TEST_EXPECT(context.queryClient.get(eventLoop, context.queryURL.view()));
    context.queryClient.callback = [this, &context](HttpTestClient& result)
    {
        context.safetyCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("Response from file"));

        static constexpr StringSpan badRequest =
            "GET /../file.html HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
        SC_TEST_EXPECT(context.badPathClient.sendRaw(*context.loop, context.serverURL.view(), badRequest));
    };

    context.badPathClient.callback = [this, &context](HttpTestClient& result)
    {
        context.safetyCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("400 Bad Request"));

        SC_TEST_EXPECT(context.mimeClient.get(*context.loop, context.webpURL.view()));
    };

    context.mimeClient.callback = [this, &context](HttpTestClient& result)
    {
        context.mimeCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("Content-Type: image/webp"));

        // Create an Http Client request for that file
        SC_TEST_EXPECT(context.getClient.get(*context.loop, context.fileURL.view()));
    };

    context.getClient.callback = [this, &context](HttpTestClient& result)
    {
        context.getCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("Response from file"));

        // Test PUT with a 10 ms delay between headers and body to induce two separate reads on the receiving side
        // one with the headers, and one with the body contents, that will trigger the pipeline streaming code path.
        SC_TEST_EXPECT(context.putStream.put(*context.loop, context.streamURL.view(), "StreamBody", {10}));
    };

    context.putStream.callback = [this, &context](HttpTestClient& result)
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
        SC_TEST_EXPECT(context.putInline.put(*context.loop, context.inlineURL.view(), "InlineBody"));
    };
    context.putInline.callback = [this, &context](HttpTestClient& result)
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

        SC_TEST_EXPECT(context.multipartPayload.resizeWithoutInitializing(8 * 1024));
        for (size_t idx = 0; idx < context.multipartPayload.size(); ++idx)
        {
            context.multipartPayload.data()[idx] = static_cast<char>((idx * 37 + 11) & 0xFF);
        }
        for (size_t idx = 997; idx + 8 < context.multipartPayload.size(); idx += 997)
        {
            context.multipartPayload.data()[idx + 0] = '\r';
            context.multipartPayload.data()[idx + 1] = '\n';
            context.multipartPayload.data()[idx + 2] = '-';
            context.multipartPayload.data()[idx + 3] = '-';
            context.multipartPayload.data()[idx + 4] = 'n';
            context.multipartPayload.data()[idx + 5] = 'o';
            context.multipartPayload.data()[idx + 6] = 'p';
            context.multipartPayload.data()[idx + 7] = 'e';
        }

        // Test multipart POST with binary file upload spanning multiple request buffers.
        StringSpan multipartContent = {{context.multipartPayload.data(), context.multipartPayload.size()}, false,
                                       StringEncoding::Ascii};
        SC_TEST_EXPECT(context.postMultipart.postMultipart(*context.loop, context.uploadURL.view(), "file",
                                                           "multipart.bin", multipartContent));
    };

    context.postMultipart.callback = [this, &context](HttpTestClient& result)
    {
        context.multipartCount++;
        StringView str(result.getResponse());
        // Expect 201 Created
        SC_TEST_EXPECT(str.containsString("201 Created"));

        // Verify file content
        Buffer content;
        SC_TEST_EXPECT(context.fs.read("multipart.bin", content));
        SC_TEST_EXPECT(content.size() == context.multipartPayload.size());
        SC_TEST_EXPECT(::memcmp(content.data(), context.multipartPayload.data(), content.size()) == 0);
        SC_TEST_EXPECT(context.fs.removeFile("multipart.bin"));

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
    SC_TEST_EXPECT(context.safetyCount == 2);
    SC_TEST_EXPECT(context.mimeCount == 1);
    SC_TEST_EXPECT(context.fs.removeFile("file.html"));
    SC_TEST_EXPECT(context.fs.removeFile("asset.webp"));
}

namespace SC
{
void runHttpAsyncFileServerTest(SC::TestReport& report) { HttpAsyncFileServerTest test(report); }
} // namespace SC
