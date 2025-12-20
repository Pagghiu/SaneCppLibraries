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
    constexpr int REQUEST_SIZE    = 1024;     // How many bytes are allocated to stream data for each connection
    constexpr int HEADER_SIZE     = 1024 * 8; // How many bytes are dedicated to hold request and response headers
    constexpr int EXTRA_BUFFERS   = 1;        // Extra write slice needed to write headers buffer
    constexpr int NUM_FS_THREADS  = 4;        // Number of threads in the thread pool for async file stream operations

    // Note: All fixed arrays could be created dynamically at startup time with new / malloc.
    // Alternatively it's also possible to reserve memory for some insane large amount of clients (using VirtualMemory
    // class for example) and just dynamically commit as much memory as one needs to handle a given number of clients

    // 1. Memory for all http headers of all connections
    Buffer headersMemory;
    SC_TEST_EXPECT(headersMemory.resize(MAX_CONNECTIONS * HEADER_SIZE));

    // 2. Memory to hold all sliced buffers used by the read queues
    AsyncReadableStream::Request readQueue[MAX_CONNECTIONS * REQUEST_SLICES];

    // 3. Memory to hold all sliced buffers used by the write queues
    AsyncWritableStream::Request writeQueue[MAX_CONNECTIONS * (REQUEST_SLICES + EXTRA_BUFFERS)];

    // 4. Memory to hold all pre-registered / re-usable buffers used by the read and write queues.
    // EXTRA_BUFFERS is used to accommodate some empty slots for external bufs (Strings or other
    // pieces of memory allocated, owned and pushed by the user to the write queue)
    AsyncBufferView  buffers[MAX_CONNECTIONS * (REQUEST_SLICES + EXTRA_BUFFERS)];
    AsyncBuffersPool buffersPool = {buffers};

    // Slice a buffer in equal parts to create re-usable slices of memory when streaming files.
    // It's not required to slice the buffer in equal parts, that's just an arbitrary choice.
    Buffer requestsMemory;
    SC_TEST_EXPECT(requestsMemory.resize(MAX_CONNECTIONS * REQUEST_SIZE));
    SC_TEST_EXPECT(HttpAsyncServer::sliceReusableEqualMemoryBuffers(buffers, requestsMemory.toSpan(), MAX_CONNECTIONS,
                                                                    REQUEST_SLICES, REQUEST_SIZE));

    // 5. Memory to hold all http connections
    HttpConnection connections[MAX_CONNECTIONS];

    // 6. Memory used by the async streams handled by the async file server
    HttpAsyncFileServerStream streams[MAX_CONNECTIONS];

    // Initialize and start the http and the file server
    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    ThreadPool threadPool;
    if (eventLoop.needsThreadPoolForFileOperations()) // no thread pool needed for io_uring
    {
        SC_TEST_EXPECT(threadPool.create(NUM_FS_THREADS));
    }
    SC_TEST_EXPECT(httpServer.init(buffersPool, connections, headersMemory.toSpan(), readQueue, writeQueue));
    SC_TEST_EXPECT(fileServer.init(buffersPool, threadPool, webServerFolder, streams));
    SC_TEST_EXPECT(fileServer.start(httpServer));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", 8090));

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
    SC_TEST_EXPECT(fileServer.stop());
    SC_TEST_EXPECT(fileServer.close());
    SC_TEST_EXPECT(httpServer.close());

    // Remove the test file
    SC_TEST_EXPECT(fs.removeFile("file.html"));
}

namespace SC
{
void runHttpAsyncFileServerTest(SC::TestReport& report) { HttpAsyncFileServerTest test(report); }
} // namespace SC
