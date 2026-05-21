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
        HttpAsyncServer&     httpServer;
        AsyncEventLoop*      loop       = nullptr;
        HttpAsyncFileServer* fileServer = nullptr;
        FileSystem           fs         = {};

        int getCount               = 0;
        int putCount               = 0;
        int multipartCount         = 0;
        int badMultipartCount      = 0;
        int safetyCount            = 0;
        int mimeCount              = 0;
        int binaryMimeCount        = 0;
        int missingCount           = 0;
        int spaCount               = 0;
        int disabledValidatorCount = 0;
        int disabledRangeCount     = 0;
        int etagCount              = 0;
        int rangeCount             = 0;
        int ifRangeMismatchCount   = 0;
        int invalidRangeCount      = 0;
        int conditionalCount       = 0;
        int headCount              = 0;
        int optionsCount           = 0;

        HttpTestClient queryClient             = {};
        HttpTestClient badPathClient           = {};
        HttpTestClient mimeClient              = {};
        HttpTestClient zipMimeClient           = {};
        HttpTestClient binaryMimeClient        = {};
        HttpTestClient missingClient           = {};
        HttpTestClient spaClient               = {};
        HttpTestClient disabledValidatorClient = {};
        HttpTestClient disabledRangeClient     = {};
        HttpTestClient etagClient              = {};
        HttpTestClient rangeClient             = {};
        HttpTestClient ifRangeMismatchClient   = {};
        HttpTestClient invalidRangeClient      = {};
        HttpTestClient conditionalClient       = {};
        HttpTestClient headClient              = {};
        HttpTestClient optionsClient           = {};
        HttpTestClient getClient               = {};
        HttpTestClient putStream               = {};
        HttpTestClient putInline               = {};
        HttpTestClient putLarge                = {};
        HttpTestClient postMultipart           = {};
        HttpTestClient badMultipart            = {};
        Buffer         multipartPayload;
        Buffer         largePutPayload;

        String fileURL    = StringEncoding::Ascii;
        String queryURL   = StringEncoding::Ascii;
        String webpURL    = StringEncoding::Ascii;
        String zipURL     = StringEncoding::Ascii;
        String binaryURL  = StringEncoding::Ascii;
        String missingURL = StringEncoding::Ascii;
        String spaURL     = StringEncoding::Ascii;
        String serverURL  = StringEncoding::Ascii;
        String streamURL  = StringEncoding::Ascii;
        String inlineURL  = StringEncoding::Ascii;
        String largeURL   = StringEncoding::Ascii;
        String uploadURL  = StringEncoding::Ascii;

        explicit Context(HttpAsyncServer& server) : httpServer(server) {}
    } context(httpServer);
    context.loop       = &eventLoop;
    context.fileServer = &fileServer;

    SC_TEST_EXPECT(context.fs.init(webServerFolder));
    SC_TEST_EXPECT(context.fs.writeString("file.html", "<html><body>Response from file</body></html>"));
    SC_TEST_EXPECT(context.fs.setLastModifiedTime("file.html", TimeMs{static_cast<int64_t>(1445412480000LL)}));
    SC_TEST_EXPECT(context.fs.writeString("asset.webp", "webp"));
    SC_TEST_EXPECT(context.fs.writeString("archive.zip", "zip"));
    SC_TEST_EXPECT(context.fs.writeString("payload.unknown", "unknown"));
    SC_TEST_EXPECT(context.fs.writeString("app.html", "<html><body>SPA fallback</body></html>"));

    SC_TEST_EXPECT(StringBuilder::format(context.fileURL, "http://127.0.0.1:{}/file.html", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.queryURL, "http://127.0.0.1:{}/file.html?download=1", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.webpURL, "http://127.0.0.1:{}/asset.webp", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.zipURL, "http://127.0.0.1:{}/archive.zip", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.binaryURL, "http://127.0.0.1:{}/payload.unknown", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.missingURL, "http://127.0.0.1:{}/missing.html", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.spaURL, "http://127.0.0.1:{}/app/route", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.serverURL, "http://127.0.0.1:{}", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.streamURL, "http://127.0.0.1:{}/stream.html", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.inlineURL, "http://127.0.0.1:{}/inline.html", serverPort));
    SC_TEST_EXPECT(StringBuilder::format(context.largeURL, "http://127.0.0.1:{}/large-put.bin", serverPort));
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

        SC_TEST_EXPECT(context.zipMimeClient.get(*context.loop, context.zipURL.view()));
    };

    context.zipMimeClient.callback = [this, &context](HttpTestClient& result)
    {
        context.binaryMimeCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("Content-Type: application/zip"));

        SC_TEST_EXPECT(context.binaryMimeClient.get(*context.loop, context.binaryURL.view()));
    };

    context.binaryMimeClient.callback = [this, &context](HttpTestClient& result)
    {
        context.binaryMimeCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("Content-Type: application/octet-stream"));

        SC_TEST_EXPECT(context.missingClient.get(*context.loop, context.missingURL.view()));
    };

    context.missingClient.callback = [this, &context](HttpTestClient& result)
    {
        context.missingCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("404 Not Found"));
        SC_TEST_EXPECT(str.containsString("Content-Length: 0"));

        HttpAsyncFileServerOptions options;
        options.spaFallbackPath = "app.html";
        SC_TEST_EXPECT(context.fileServer->setOptions(options));
        SC_TEST_EXPECT(context.spaClient.get(*context.loop, context.spaURL.view()));
    };

    context.spaClient.callback = [this, &context](HttpTestClient& result)
    {
        context.spaCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("200 OK"));
        SC_TEST_EXPECT(str.containsString("Content-Type: text/html"));
        SC_TEST_EXPECT(str.containsString("SPA fallback"));

        HttpAsyncFileServerOptions options;
        options.spaFallbackPath  = "app.html";
        options.enableValidators = false;
        SC_TEST_EXPECT(context.fileServer->setOptions(options));

        static constexpr StringSpan disabledValidatorRequest = "GET /file.html HTTP/1.1\r\n"
                                                               "Host: 127.0.0.1\r\n"
                                                               "If-None-Match: W/\"44-1445412480000\"\r\n"
                                                               "If-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT\r\n"
                                                               "Connection: close\r\n\r\n";
        SC_TEST_EXPECT(
            context.disabledValidatorClient.sendRaw(*context.loop, context.serverURL.view(), disabledValidatorRequest));
    };

    context.disabledValidatorClient.callback = [this, &context](HttpTestClient& result)
    {
        context.disabledValidatorCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("200 OK"));
        SC_TEST_EXPECT(str.containsString("Response from file"));
        SC_TEST_EXPECT(not str.containsString("ETag:"));
        SC_TEST_EXPECT(not str.containsString("Last-Modified:"));

        HttpAsyncFileServerOptions options;
        options.spaFallbackPath     = "app.html";
        options.enableRangeRequests = false;
        SC_TEST_EXPECT(context.fileServer->setOptions(options));

        static constexpr StringSpan disabledRangeRequest = "GET /file.html HTTP/1.1\r\n"
                                                           "Host: 127.0.0.1\r\n"
                                                           "Range: bytes=6-9\r\n"
                                                           "Connection: close\r\n\r\n";
        SC_TEST_EXPECT(
            context.disabledRangeClient.sendRaw(*context.loop, context.serverURL.view(), disabledRangeRequest));
    };

    context.disabledRangeClient.callback = [this, &context](HttpTestClient& result)
    {
        context.disabledRangeCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("200 OK"));
        SC_TEST_EXPECT(str.containsString("Content-Length: 44"));
        SC_TEST_EXPECT(str.containsString("Response from file"));
        SC_TEST_EXPECT(not str.containsString("Content-Range:"));
        SC_TEST_EXPECT(not str.containsString("Accept-Ranges:"));

        HttpAsyncFileServerOptions options;
        options.spaFallbackPath = "app.html";
        SC_TEST_EXPECT(context.fileServer->setOptions(options));

        SC_TEST_EXPECT(context.etagClient.get(*context.loop, context.fileURL.view()));
    };

    context.etagClient.callback = [this, &context](HttpTestClient& result)
    {
        context.etagCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("200 OK"));
        SC_TEST_EXPECT(str.containsString("ETag: W/\"44-1445412480000\""));
        SC_TEST_EXPECT(str.containsString("Response from file"));

        static constexpr StringSpan rangeRequest = "GET /file.html HTTP/1.1\r\n"
                                                   "Host: 127.0.0.1\r\n"
                                                   "Range: bytes=6-9\r\n"
                                                   "Connection: close\r\n\r\n";
        SC_TEST_EXPECT(context.rangeClient.sendRaw(*context.loop, context.serverURL.view(), rangeRequest));
    };

    context.rangeClient.callback = [this, &context](HttpTestClient& result)
    {
        context.rangeCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("206 Partial Content"));
        SC_TEST_EXPECT(str.containsString("Content-Length: 4"));
        SC_TEST_EXPECT(str.containsString("Content-Range: bytes 6-9/44"));
        SC_TEST_EXPECT(str.containsString("Accept-Ranges: bytes"));
        SC_TEST_EXPECT(str.containsString("<bod"));
        SC_TEST_EXPECT(not str.containsString("Response from file"));

        static constexpr StringSpan ifRangeMismatchRequest = "GET /file.html HTTP/1.1\r\n"
                                                             "Host: 127.0.0.1\r\n"
                                                             "Range: bytes=6-9\r\n"
                                                             "If-Range: W/\"different\"\r\n"
                                                             "Connection: close\r\n\r\n";
        SC_TEST_EXPECT(
            context.ifRangeMismatchClient.sendRaw(*context.loop, context.serverURL.view(), ifRangeMismatchRequest));
    };

    context.ifRangeMismatchClient.callback = [this, &context](HttpTestClient& result)
    {
        context.ifRangeMismatchCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("200 OK"));
        SC_TEST_EXPECT(str.containsString("Content-Length: 44"));
        SC_TEST_EXPECT(str.containsString("Response from file"));
        SC_TEST_EXPECT(not str.containsString("Content-Range:"));

        static constexpr StringSpan invalidRangeRequest = "GET /file.html HTTP/1.1\r\n"
                                                          "Host: 127.0.0.1\r\n"
                                                          "Range: bytes=1000-1001\r\n"
                                                          "Connection: close\r\n\r\n";
        SC_TEST_EXPECT(
            context.invalidRangeClient.sendRaw(*context.loop, context.serverURL.view(), invalidRangeRequest));
    };

    context.invalidRangeClient.callback = [this, &context](HttpTestClient& result)
    {
        context.invalidRangeCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("416 Range Not Satisfiable"));
        SC_TEST_EXPECT(str.containsString("Content-Range: bytes */44"));
        SC_TEST_EXPECT(str.containsString("Content-Length: 0"));

        static constexpr StringSpan conditionalRequest = "GET /file.html HTTP/1.1\r\n"
                                                         "Host: 127.0.0.1\r\n"
                                                         "If-None-Match: \"miss\", W/\"44-1445412480000\"\r\n"
                                                         "If-Modified-Since: Tue, 20 Oct 2015 07:28:00 GMT\r\n"
                                                         "Connection: close\r\n\r\n";
        SC_TEST_EXPECT(context.conditionalClient.sendRaw(*context.loop, context.serverURL.view(), conditionalRequest));
    };

    context.conditionalClient.callback = [this, &context](HttpTestClient& result)
    {
        context.conditionalCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("304 Not Modified"));
        SC_TEST_EXPECT(str.containsString("Last-Modified: Wed, 21 Oct 2015 07:28:00 GMT"));
        SC_TEST_EXPECT(str.containsString("ETag: W/\"44-1445412480000\""));
        SC_TEST_EXPECT(not str.containsString("Response from file"));

        SC_TEST_EXPECT(context.headClient.head(*context.loop, context.fileURL.view()));
    };

    context.headClient.callback = [this, &context](HttpTestClient& result)
    {
        context.headCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("200 OK"));
        SC_TEST_EXPECT(str.containsString("Content-Type: text/html"));
        SC_TEST_EXPECT(not str.containsString("Response from file"));

        static constexpr StringSpan optionsRequest =
            "OPTIONS /file.html HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
        SC_TEST_EXPECT(context.optionsClient.sendRaw(*context.loop, context.serverURL.view(), optionsRequest));
    };

    context.optionsClient.callback = [this, &context](HttpTestClient& result)
    {
        context.optionsCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("204 No Content"));
        SC_TEST_EXPECT(str.containsString("Allow: GET, HEAD, PUT, POST, OPTIONS"));

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

        SC_TEST_EXPECT(context.largePutPayload.resizeWithoutInitializing(600 * 1024 + 17));
        for (size_t idx = 0; idx < context.largePutPayload.size(); ++idx)
        {
            context.largePutPayload.data()[idx] = static_cast<char>((idx * 17 + 3) & 0x7F);
        }
        StringSpan largePutContent = {
            {context.largePutPayload.data(), context.largePutPayload.size()}, false, StringEncoding::Ascii};
        SC_TEST_EXPECT(context.putLarge.put(*context.loop, context.largeURL.view(), largePutContent));
    };

    context.putLarge.callback = [this, &context](HttpTestClient& result)
    {
        context.putCount++;
        StringView str(result.getResponse());
        // Expect 201 Created and Content-Length: 0
        SC_TEST_EXPECT(str.containsString("201 Created"));

        // Verify file content
        Buffer content;
        SC_TEST_EXPECT(context.fs.read("large-put.bin", content));
        SC_TEST_EXPECT(content.size() == context.largePutPayload.size());
        SC_TEST_EXPECT(::memcmp(content.data(), context.largePutPayload.data(), content.size()) == 0);
        SC_TEST_EXPECT(context.fs.removeFile("large-put.bin"));

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
        StringSpan multipartContent = {
            {context.multipartPayload.data(), context.multipartPayload.size()}, false, StringEncoding::Ascii};
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

        SC_TEST_EXPECT(context.badMultipart.postMultipart(*context.loop, context.uploadURL.view(), "file",
                                                          "../multipart_escape.bin", "bad"));
    };

    context.badMultipart.callback = [this, &context](HttpTestClient& result)
    {
        context.badMultipartCount++;
        StringView str(result.getResponse());
        SC_TEST_EXPECT(str.containsString("400 Bad Request"));
        SC_TEST_EXPECT(not context.fs.existsAndIsFile("multipart_escape.bin"));

        SC_TEST_EXPECT(context.httpServer.stop());
    };

    // Safety timout against hangs
    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{5000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(fileServer.close());
    SC_TEST_EXPECT(httpServer.close());

    SC_TEST_EXPECT(context.getCount == 1);
    SC_TEST_EXPECT(context.putCount == 3);
    SC_TEST_EXPECT(context.multipartCount == 1);
    SC_TEST_EXPECT(context.badMultipartCount == 1);
    SC_TEST_EXPECT(context.safetyCount == 2);
    SC_TEST_EXPECT(context.mimeCount == 1);
    SC_TEST_EXPECT(context.binaryMimeCount == 2);
    SC_TEST_EXPECT(context.missingCount == 1);
    SC_TEST_EXPECT(context.spaCount == 1);
    SC_TEST_EXPECT(context.disabledValidatorCount == 1);
    SC_TEST_EXPECT(context.disabledRangeCount == 1);
    SC_TEST_EXPECT(context.etagCount == 1);
    SC_TEST_EXPECT(context.rangeCount == 1);
    SC_TEST_EXPECT(context.ifRangeMismatchCount == 1);
    SC_TEST_EXPECT(context.invalidRangeCount == 1);
    SC_TEST_EXPECT(context.conditionalCount == 1);
    SC_TEST_EXPECT(context.headCount == 1);
    SC_TEST_EXPECT(context.optionsCount == 1);
    SC_TEST_EXPECT(context.fs.removeFile("file.html"));
    SC_TEST_EXPECT(context.fs.removeFile("asset.webp"));
    SC_TEST_EXPECT(context.fs.removeFile("archive.zip"));
    SC_TEST_EXPECT(context.fs.removeFile("payload.unknown"));
    SC_TEST_EXPECT(context.fs.removeFile("app.html"));
}

namespace SC
{
void runHttpAsyncFileServerTest(SC::TestReport& report) { HttpAsyncFileServerTest test(report); }
} // namespace SC
