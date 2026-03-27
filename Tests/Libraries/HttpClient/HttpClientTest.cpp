// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/HttpClient/HttpClient.h"
#include "Libraries/AsyncStreams/AsyncStreams.h"
#include "Libraries/Foundation/Deferred.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/HttpClient/HttpClientAsync.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"
#include "Libraries/Time/Time.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace SC
{
struct HttpClientTest;
}

namespace
{
using ServerConnection     = SC::HttpAsyncConnection<3, 3, 8 * 1024, 8 * 1024>;
using AsyncClientOperation = SC::HttpClientAsyncT<SC::AsyncEventLoop, SC::AsyncStreams>;
} // namespace

struct SC::HttpClientTest : public SC::TestCase
{
    HttpClientTest(SC::TestReport& report) : TestCase(report, "HttpClientTest")
    {
        if (test_section("init and close"))
        {
            initAndClose();
        }
        if (test_section("blocking GET local"))
        {
            blockingGet();
        }
        if (test_section("blocking POST local"))
        {
            blockingPost();
        }
        if (test_section("custom headers"))
        {
            blockingCustomHeaders();
        }
        if (test_section("timeout"))
        {
            blockingTimeout();
        }
        if (test_section("poll GET local"))
        {
            pollGet();
        }
        if (test_section("poll concurrent GETs"))
        {
            pollConcurrentGets();
        }
        if (test_section("async GET local"))
        {
            asyncGet();
        }
        if (test_section("async concurrent GETs"))
        {
            asyncConcurrentGets();
        }
        if (test_section("async download large"))
        {
            asyncDownloadLarge();
        }
        if (test_section("async upload pipeline"))
        {
            asyncUploadPipeline();
        }
    }

    struct TestServer
    {
        AsyncEventLoop&  loop;
        ServerConnection connections[8];
        HttpAsyncServer  server;
        uint16_t         port = 0;
        String           endpoint;
        AsyncLoopWakeUp  wakeUpStop;

        TestServer(AsyncEventLoop& loopValue) : loop(loopValue) { endpoint = String(StringEncoding::Ascii); }

        Result start(TestReport& testReport)
        {
            port = testReport.mapPort(6152);
            SC_TRY(server.init(Span<ServerConnection>(connections)));
            server.setDefaultKeepAlive(false);
            wakeUpStop.callback = [this](AsyncLoopWakeUp::Result& result)
            {
                (void)server.stop();
                result.reactivateRequest(false);
            };
            SC_TRY(wakeUpStop.start(loop));
            SC_TRY(server.start(loop, "127.0.0.1", port));
            SC_TRY(StringBuilder::format(endpoint, "http://127.0.0.1:{}", port));
            return Result(true);
        }

        Result scheduleStop() { return wakeUpStop.wakeUp(loop); }
    };

    template <size_t ResponseBytes, size_t NumResponseBuffers, size_t EventQueueSize, size_t HeaderBytes,
              size_t ScratchBytes>
    struct CoreOperationMemory
    {
        HttpClientResponseBuffer  responseBuffers[NumResponseBuffers];
        HttpClientOperationEvent  eventQueue[EventQueueSize];
        HttpClientOperationMemory memory;

        char responseMemory[ResponseBytes];
        char responseHeaders[HeaderBytes];
        char backendScratch[ScratchBytes];

        CoreOperationMemory()
        {
            memory.responseBuffers      = {responseBuffers, NumResponseBuffers};
            memory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
            memory.eventQueue           = {eventQueue, EventQueueSize};
            memory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
            memory.backendScratch       = {backendScratch, sizeof(backendScratch)};
        }
    };

    template <size_t ResponseBytes, size_t NumResponseBuffers, size_t EventQueueSize, size_t RequestWriteQueueSize,
              size_t HeaderBytes, size_t ScratchBytes>
    struct AsyncOperationMemory
    {
        HttpClientResponseBuffer     coreResponseBuffers[NumResponseBuffers];
        HttpClientOperationEvent     eventQueue[EventQueueSize];
        AsyncBufferView              asyncResponseBuffers[NumResponseBuffers];
        AsyncReadableStream::Request responseReadQueue[NumResponseBuffers];
        AsyncWritableStream::Request requestWriteQueue[RequestWriteQueueSize];
        HttpClientOperationMemory    coreMemory;

        HttpClientAsyncOperationMemoryT<AsyncStreams> asyncMemory;

        char responseMemory[ResponseBytes];
        char responseHeaders[HeaderBytes];
        char backendScratch[ScratchBytes];

        AsyncOperationMemory()
        {
            coreMemory.responseBuffers      = {coreResponseBuffers, NumResponseBuffers};
            coreMemory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
            coreMemory.eventQueue           = {eventQueue, EventQueueSize};
            coreMemory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
            coreMemory.backendScratch       = {backendScratch, sizeof(backendScratch)};

            asyncMemory.responseBuffers      = {asyncResponseBuffers, NumResponseBuffers};
            asyncMemory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
            asyncMemory.responseReadQueue    = {responseReadQueue, NumResponseBuffers};
            asyncMemory.requestWriteQueue    = {requestWriteQueue, RequestWriteQueueSize};
        }
    };

    template <size_t BufferBytes, size_t NumBuffers>
    struct UploadBuffers
    {
        AsyncBufferView              buffers[NumBuffers];
        AsyncReadableStream::Request readQueue[NumBuffers];
        char                         memory[BufferBytes];
        AsyncBuffersPool             pool;

        UploadBuffers()
        {
            pool.setBuffers({buffers, NumBuffers});
            SC_ASSERT_RELEASE(
                AsyncBuffersPool::sliceInEqualParts({buffers, NumBuffers}, {memory, sizeof(memory)}, NumBuffers));
        }
    };

    struct PollResponseCollector final : public HttpClientOperationListener
    {
        Span<char> bodyBuffer;

        size_t bodyLength = 0;
        bool   completed  = false;
        Result finalRes   = Result(true);
        int    headCount  = 0;

        virtual void onResponseHead(HttpClientResponse&) override { headCount += 1; }

        virtual void onResponseBody(Span<const char> data) override
        {
            const size_t remaining = bodyBuffer.sizeInBytes() - bodyLength;
            const size_t toCopy    = data.sizeInBytes() < remaining ? data.sizeInBytes() : remaining;
            if (toCopy > 0)
            {
                memcpy(bodyBuffer.data() + bodyLength, data.data(), toCopy);
                bodyLength += toCopy;
            }
        }

        virtual void onResponseComplete() override { completed = true; }

        virtual void onError(Result error) override
        {
            completed = true;
            finalRes  = error;
        }
    };

    struct AsyncResponseCollector
    {
        AsyncClientOperation* operation = nullptr;
        Span<char>            bodyBuffer;

        size_t bodyLength = 0;
        bool   completed  = false;
        Result finalRes   = Result(true);
        int    headCount  = 0;

        void onHead(HttpClientResponse&) { headCount += 1; }

        void onData(AsyncBufferView::ID bufferID)
        {
            Span<const char> data;
            SC_ASSERT_RELEASE(operation->getResponseBodyStream().getBuffersPool().getReadableData(bufferID, data));
            const size_t remaining = bodyBuffer.sizeInBytes() - bodyLength;
            const size_t toCopy    = data.sizeInBytes() < remaining ? data.sizeInBytes() : remaining;
            if (toCopy > 0)
            {
                memcpy(bodyBuffer.data() + bodyLength, data.data(), toCopy);
                bodyLength += toCopy;
            }
        }

        void onEnd() { completed = true; }

        void onError(Result error)
        {
            completed = true;
            finalRes  = error;
        }
    };

    struct AsyncEndCollector
    {
        bool*   completed = nullptr;
        Result* finalRes  = nullptr;

        void onEnd() { *completed = true; }
        void onError(Result error)
        {
            *completed = true;
            *finalRes  = error;
        }
    };

    void initAndClose()
    {
        HttpClient client;
        SC_TEST_EXPECT(client.init());

        CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> coreMemory;
        HttpClientOperation                                    operation;
        SC_TEST_EXPECT(operation.init(client, coreMemory.memory));
        SC_TEST_EXPECT(operation.close());

        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        AsyncOperationMemory<64 * 1024, 8, 16, 8, 4096, 16 * 1024> asyncMemory;
        AsyncClientOperation                                       asyncOperation;
        SC_TEST_EXPECT(asyncOperation.init(client, loop, asyncMemory.coreMemory, asyncMemory.asyncMemory));
        SC_TEST_EXPECT(asyncOperation.close());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(loop.close());
    }

    void blockingGet()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            SC_TEST_EXPECT(client.request.getParser().method == HttpParser::Method::HttpGET);
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "9"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.getWritableStream().write("Hello GET"));
            SC_TEST_EXPECT(client.response.end());
        };

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;

                HttpClientRequest  request;
                HttpClientResponse response;
                char               body[1024] = {};
                size_t             bodyLength = 0;

                request.url = server.endpoint.view();
                SC_TEST_EXPECT(
                    HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength, memory.memory));
                SC_TEST_EXPECT(response.statusCode == 200);
                SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "Hello GET");
                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void blockingPost()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            SC_TEST_EXPECT(client.request.getParser().method == HttpParser::Method::HttpPOST);
            SC_TEST_EXPECT(client.request.getBodyBytesRemaining() == 9);
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "2"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.getWritableStream().write("OK"));
            SC_TEST_EXPECT(client.response.end());
        };

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;

                HttpClientRequest  request;
                HttpClientResponse response;
                char               body[1024] = {};
                size_t             bodyLength = 0;

                request.url    = server.endpoint.view();
                request.method = HttpClientRequest::HttpPOST;
                request.body   = {"HelloBody", 9};

                SC_TEST_EXPECT(
                    HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength, memory.memory));
                SC_TEST_EXPECT(response.statusCode == 200);
                SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "OK");
                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void blockingCustomHeaders()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            StringView headerName;
            SC_TEST_EXPECT(client.request.getHeader("X-Test"_a8, headerName));
            SC_TEST_EXPECT(headerName == "HeaderValue");

            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "0"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.end());
        };

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;

                StringSpan        headerName  = "X-Test"_a8;
                StringSpan        headerValue = "HeaderValue"_a8;
                HttpClientRequest request;
                request.url          = server.endpoint.view();
                request.headerNames  = {&headerName, 1};
                request.headerValues = {&headerValue, 1};

                HttpClientResponse response;
                char               body[64]   = {};
                size_t             bodyLength = 0;

                SC_TEST_EXPECT(
                    HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength, memory.memory));
                SC_TEST_EXPECT(response.statusCode == 200);
                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void blockingTimeout()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [](HttpConnection&) {};

        AsyncLoopTimeout backupTimeout;
        backupTimeout.callback = [this, &server](AsyncLoopTimeout::Result&) { SC_TEST_EXPECT(server.scheduleStop()); };
        SC_TEST_EXPECT(backupTimeout.start(loop, 500_ms));

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;

                HttpClientRequest  request;
                HttpClientResponse response;
                char               body[64]   = {};
                size_t             bodyLength = 0;

                request.url       = server.endpoint.view();
                request.timeoutMs = 100;

                SC_TEST_EXPECT(not HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               memory.memory));
                SC_TEST_EXPECT(client.close());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void pollGet()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "10"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.getWritableStream().write("Hello Poll"));
            SC_TEST_EXPECT(client.response.end());
        };

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;
                HttpClientOperation                                    operation;
                SC_TEST_EXPECT(operation.init(client, memory.memory));

                HttpClientRequest  request;
                HttpClientResponse response;
                char               body[1024] = {};

                PollResponseCollector collector;
                collector.bodyBuffer = {body, sizeof(body)};

                request.url = server.endpoint.view();
                SC_TEST_EXPECT(operation.start(request, response, &collector));
                while (not collector.completed)
                {
                    SC_TEST_EXPECT(operation.poll(50));
                }

                SC_TEST_EXPECT(collector.finalRes);
                SC_TEST_EXPECT(collector.headCount == 1);
                SC_TEST_EXPECT(response.statusCode == 200);
                SC_TEST_EXPECT(StringView({body, collector.bodyLength}, false, StringEncoding::Ascii) == "Hello Poll");

                SC_TEST_EXPECT(operation.close());
                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void pollConcurrentGets()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "6"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.getWritableStream().write("ABCDEF"));
            SC_TEST_EXPECT(client.response.end());
        };

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                static constexpr int NumClients = 3;

                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memories[NumClients];
                HttpClientOperation                                    operations[NumClients];
                HttpClientResponse                                     responses[NumClients];
                PollResponseCollector                                  collectors[NumClients];
                char                                                   bodies[NumClients][64] = {};

                for (int idx = 0; idx < NumClients; ++idx)
                {
                    collectors[idx].bodyBuffer = {bodies[idx], sizeof(bodies[idx])};
                    SC_TEST_EXPECT(operations[idx].init(client, memories[idx].memory));

                    HttpClientRequest request;
                    request.url = server.endpoint.view();
                    SC_TEST_EXPECT(operations[idx].start(request, responses[idx], &collectors[idx]));
                }

                bool allCompleted = false;
                while (not allCompleted)
                {
                    allCompleted = true;
                    for (int idx = 0; idx < NumClients; ++idx)
                    {
                        if (not collectors[idx].completed)
                        {
                            allCompleted = false;
                            SC_TEST_EXPECT(operations[idx].poll(10));
                        }
                    }
                }

                for (int idx = 0; idx < NumClients; ++idx)
                {
                    SC_TEST_EXPECT(collectors[idx].finalRes);
                    SC_TEST_EXPECT(collectors[idx].headCount == 1);
                    SC_TEST_EXPECT(responses[idx].statusCode == 200);
                    SC_TEST_EXPECT(StringView({bodies[idx], collectors[idx].bodyLength}, false,
                                              StringEncoding::Ascii) == "ABCDEF");
                    SC_TEST_EXPECT(operations[idx].close());
                }

                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void asyncGet()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "11"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.getWritableStream().write("Hello Async"));
            SC_TEST_EXPECT(client.response.end());
        };

        HttpClient client;
        SC_TEST_EXPECT(client.init());

        AsyncOperationMemory<64 * 1024, 8, 16, 8, 4096, 16 * 1024> memory;
        AsyncClientOperation                                       operation;
        SC_TEST_EXPECT(operation.init(client, loop, memory.coreMemory, memory.asyncMemory));

        HttpClientRequest  request;
        HttpClientResponse response;
        char               body[1024] = {};

        AsyncResponseCollector collector = {&operation, {body, sizeof(body)}};
        const bool             headAdded =
            operation.eventResponseHead.addListener<AsyncResponseCollector, &AsyncResponseCollector::onHead>(collector);
        const bool dataAdded =
            operation.getResponseBodyStream()
                .eventData.addListener<AsyncResponseCollector, &AsyncResponseCollector::onData>(collector);
        const bool endAdded =
            operation.getResponseBodyStream()
                .eventEnd.addListener<AsyncResponseCollector, &AsyncResponseCollector::onEnd>(collector);
        const bool errorAdded =
            operation.getResponseBodyStream()
                .eventError.addListener<AsyncResponseCollector, &AsyncResponseCollector::onError>(collector);
        SC_TEST_EXPECT(headAdded);
        SC_TEST_EXPECT(dataAdded);
        SC_TEST_EXPECT(endAdded);
        SC_TEST_EXPECT(errorAdded);

        request.url = server.endpoint.view();
        SC_TEST_EXPECT(operation.start(request, response));
        while (not collector.completed)
        {
            SC_TEST_EXPECT(loop.runOnce());
        }

        SC_TEST_EXPECT(collector.finalRes);
        SC_TEST_EXPECT(collector.headCount == 1);
        SC_TEST_EXPECT(response.statusCode == 200);
        SC_TEST_EXPECT(StringView({body, collector.bodyLength}, false, StringEncoding::Ascii) == "Hello Async");

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(server.server.stop());
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(loop.close());
    }

    void asyncConcurrentGets()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "4"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.getWritableStream().write("PING"));
            SC_TEST_EXPECT(client.response.end());
        };

        static constexpr int NumClients = 3;

        HttpClient client;
        SC_TEST_EXPECT(client.init());

        AsyncOperationMemory<64 * 1024, 8, 16, 8, 4096, 16 * 1024> memories[NumClients];
        AsyncClientOperation                                       operations[NumClients];
        AsyncResponseCollector                                     collectors[NumClients];
        HttpClientResponse                                         responses[NumClients];
        char                                                       bodies[NumClients][32] = {};

        for (int idx = 0; idx < NumClients; ++idx)
        {
            SC_TEST_EXPECT(operations[idx].init(client, loop, memories[idx].coreMemory, memories[idx].asyncMemory));
            collectors[idx].operation  = &operations[idx];
            collectors[idx].bodyBuffer = {bodies[idx], sizeof(bodies[idx])};
            const bool headAdded =
                operations[idx].eventResponseHead.addListener<AsyncResponseCollector, &AsyncResponseCollector::onHead>(
                    collectors[idx]);
            const bool dataAdded =
                operations[idx]
                    .getResponseBodyStream()
                    .eventData.addListener<AsyncResponseCollector, &AsyncResponseCollector::onData>(collectors[idx]);
            const bool endAdded =
                operations[idx]
                    .getResponseBodyStream()
                    .eventEnd.addListener<AsyncResponseCollector, &AsyncResponseCollector::onEnd>(collectors[idx]);
            const bool errorAdded =
                operations[idx]
                    .getResponseBodyStream()
                    .eventError.addListener<AsyncResponseCollector, &AsyncResponseCollector::onError>(collectors[idx]);
            SC_TEST_EXPECT(headAdded);
            SC_TEST_EXPECT(dataAdded);
            SC_TEST_EXPECT(endAdded);
            SC_TEST_EXPECT(errorAdded);

            HttpClientRequest request;
            request.url = server.endpoint.view();
            SC_TEST_EXPECT(operations[idx].start(request, responses[idx]));
        }

        bool allCompleted = false;
        while (not allCompleted)
        {
            allCompleted = true;
            for (int idx = 0; idx < NumClients; ++idx)
            {
                if (not collectors[idx].completed)
                {
                    allCompleted = false;
                }
            }
            if (not allCompleted)
            {
                SC_TEST_EXPECT(loop.runOnce());
            }
        }

        for (int idx = 0; idx < NumClients; ++idx)
        {
            SC_TEST_EXPECT(collectors[idx].finalRes);
            SC_TEST_EXPECT(collectors[idx].headCount == 1);
            SC_TEST_EXPECT(responses[idx].statusCode == 200);
            SC_TEST_EXPECT(StringView({bodies[idx], collectors[idx].bodyLength}, false, StringEncoding::Ascii) ==
                           "PING");
            SC_TEST_EXPECT(operations[idx].close());
        }

        SC_TEST_EXPECT(server.server.stop());
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(loop.close());
    }

    void asyncDownloadLarge()
    {
        static constexpr size_t PayloadSize = 8 * 1024 * 1024;

        using DownloadMemory = AsyncOperationMemory<1024 * 1024, 32, 64, 8, 8192, 16 * 1024>;
        void* memoryStorage  = malloc(sizeof(DownloadMemory));
        SC_TEST_EXPECT(memoryStorage != nullptr);
        if (memoryStorage == nullptr)
        {
            return;
        }
        DownloadMemory* memoryPointer = reinterpret_cast<DownloadMemory*>(memoryStorage);
        placementNew(*memoryPointer);
        DownloadMemory& memory = *memoryPointer;

        auto freeMemory = MakeDeferred(
            [&]
            {
                dtor(memory);
                free(memoryStorage);
            });

        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));

        struct LargeResponseWriter
        {
            HttpConnection* connection   = nullptr;
            size_t          remaining    = 0;
            bool            waitingDrain = false;

            void start(HttpConnection& client, size_t total)
            {
                connection = &client;
                remaining  = total;
                writeNext();
            }

            void onDrain()
            {
                if (waitingDrain)
                {
                    waitingDrain = false;
                    (void)connection->response.getWritableStream()
                        .eventDrain.removeListener<LargeResponseWriter, &LargeResponseWriter::onDrain>(*this);
                }
                writeNext();
            }

            void writeNext()
            {
                AsyncWritableStream& writable = connection->response.getWritableStream();
                AsyncBuffersPool&    pool     = writable.getBuffersPool();

                while (remaining > 0)
                {
                    AsyncBufferView::ID bufferID;
                    Span<char>          data;
                    if (not pool.requestNewBuffer(1, bufferID, data))
                    {
                        break;
                    }

                    const size_t toWrite = remaining < data.sizeInBytes() ? remaining : data.sizeInBytes();
                    memset(data.data(), 'A', toWrite);
                    pool.setNewBufferSize(bufferID, toWrite);
                    const Result res = writable.write(bufferID);
                    pool.unrefBuffer(bufferID);
                    if (res)
                    {
                        remaining -= toWrite;
                    }
                    else
                    {
                        break;
                    }
                }

                if (remaining == 0)
                {
                    (void)connection->response.end();
                }
                else if (not waitingDrain)
                {
                    waitingDrain = true;
                    const bool added =
                        writable.eventDrain.addListener<LargeResponseWriter, &LargeResponseWriter::onDrain>(*this);
                    SC_ASSERT_RELEASE(added);
                }
            }
        } writer;

        server.server.onRequest = [&writer](HttpConnection& client)
        {
            SC_ASSERT_RELEASE(client.response.startResponse(200));
            SC_ASSERT_RELEASE(client.response.addHeader("Content-Length"_a8, "8388608"_a8));
            SC_ASSERT_RELEASE(client.response.sendHeaders());
            writer.start(client, PayloadSize);
        };

        HttpClient client;
        SC_TEST_EXPECT(client.init());

        AsyncClientOperation operation;
        SC_TEST_EXPECT(operation.init(client, loop, memory.coreMemory, memory.asyncMemory));

        HttpClientRequest request;
        request.url = server.endpoint.view();

        HttpClientResponse response;

        bool   completed = false;
        Result finalRes(true);
        size_t received  = 0;
        int    headCount = 0;

        struct DownloadContext
        {
            AsyncClientOperation* operation = nullptr;

            size_t* received  = nullptr;
            bool*   completed = nullptr;
            Result* finalRes  = nullptr;
            int*    headCount = nullptr;

            void onHead(HttpClientResponse&) { *headCount += 1; }

            void onData(AsyncBufferView::ID bufferID)
            {
                Span<const char> data;
                SC_ASSERT_RELEASE(operation->getResponseBodyStream().getBuffersPool().getReadableData(bufferID, data));
                *received += data.sizeInBytes();
            }

            void onEnd() { *completed = true; }

            void onError(Result error)
            {
                *completed = true;
                *finalRes  = error;
            }
        } downloadCtx = {&operation, &received, &completed, &finalRes, &headCount};

        const bool headAdded =
            operation.eventResponseHead.addListener<DownloadContext, &DownloadContext::onHead>(downloadCtx);
        const bool dataAdded =
            operation.getResponseBodyStream().eventData.addListener<DownloadContext, &DownloadContext::onData>(
                downloadCtx);
        const bool endAdded =
            operation.getResponseBodyStream().eventEnd.addListener<DownloadContext, &DownloadContext::onEnd>(
                downloadCtx);
        const bool errorAdded =
            operation.getResponseBodyStream().eventError.addListener<DownloadContext, &DownloadContext::onError>(
                downloadCtx);
        SC_TEST_EXPECT(headAdded);
        SC_TEST_EXPECT(dataAdded);
        SC_TEST_EXPECT(endAdded);
        SC_TEST_EXPECT(errorAdded);

        SC_TEST_EXPECT(operation.start(request, response));
        while (not completed)
        {
            SC_TEST_EXPECT(loop.runOnce());
        }

        SC_TEST_EXPECT(finalRes);
        SC_TEST_EXPECT(headCount == 1);
        SC_TEST_EXPECT(received == PayloadSize);
        SC_TEST_EXPECT(response.statusCode == 200);

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(server.server.stop());
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(loop.close());
    }

    void asyncUploadPipeline()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));

        static constexpr char   UploadBody[] = "UploadBodyData";
        static constexpr size_t UploadSize   = sizeof(UploadBody) - 1;

        struct UploadCollector
        {
            HttpConnection* connection = nullptr;

            char   received[UploadSize] = {};
            size_t receivedSize         = 0;
            bool   responseSent         = false;

            void onData(AsyncBufferView::ID bufferID)
            {
                AsyncReadableStream& readable = connection->request.getReadableStream();
                Span<const char>     data;
                SC_ASSERT_RELEASE(readable.getBuffersPool().getReadableData(bufferID, data));
                memcpy(received + receivedSize, data.data(), data.sizeInBytes());
                receivedSize += data.sizeInBytes();
                SC_ASSERT_RELEASE(connection->request.consumeBodyBytes(data.sizeInBytes()));
                if (connection->request.getBodyBytesRemaining() == 0 and not responseSent)
                {
                    responseSent = true;
                    SC_ASSERT_RELEASE(connection->response.startResponse(200));
                    SC_ASSERT_RELEASE(connection->response.addHeader("Content-Length"_a8, "2"_a8));
                    SC_ASSERT_RELEASE(connection->response.sendHeaders());
                    SC_ASSERT_RELEASE(connection->response.getWritableStream().write("OK"));
                    SC_ASSERT_RELEASE(connection->response.end());
                }
            }
        } uploadCollector;

        server.server.onRequest = [this, &uploadCollector](HttpConnection& client)
        {
            uploadCollector.connection = &client;
            SC_TEST_EXPECT(client.request.getBodyBytesRemaining() == UploadSize);
            const bool dataListenerAdded =
                client.request.getReadableStream().eventData.addListener<UploadCollector, &UploadCollector::onData>(
                    uploadCollector);
            SC_TEST_EXPECT(dataListenerAdded);
        };

        struct UploadReadableStream : public AsyncReadableStream
        {
            Span<const char> payload;

            size_t offset = 0;

            Result asyncRead() override
            {
                if (offset >= payload.sizeInBytes())
                {
                    pushEnd();
                    return Result(true);
                }

                AsyncBufferView::ID bufferID;
                Span<char>          writable;
                if (getBufferOrPause(1, bufferID, writable))
                {
                    const size_t remaining = payload.sizeInBytes() - offset;
                    const size_t toCopy    = remaining < writable.sizeInBytes() ? remaining : writable.sizeInBytes();
                    memcpy(writable.data(), payload.data() + offset, toCopy);
                    offset += toCopy;
                    SC_TRY(push(bufferID, toCopy));
                    getBuffersPool().unrefBuffer(bufferID);
                    reactivate(true);
                }
                return Result(true);
            }
        } uploadReadable;

        UploadBuffers<1024, 4> uploadBuffers;
        uploadReadable.payload = {UploadBody, UploadSize};
        uploadReadable.setReadQueue({uploadBuffers.readQueue, 4});
        SC_TEST_EXPECT(uploadReadable.init(uploadBuffers.pool));

        HttpClient client;
        SC_TEST_EXPECT(client.init());

        AsyncOperationMemory<64 * 1024, 8, 16, 8, 4096, 16 * 1024> memory;
        AsyncClientOperation                                       operation;
        SC_TEST_EXPECT(operation.init(client, loop, memory.coreMemory, memory.asyncMemory));

        HttpClientRequest request;
        request.url              = server.endpoint.view();
        request.method           = HttpClientRequest::HttpPOST;
        request.streamedBodySize = UploadSize;

        HttpClientResponse response;

        bool              completed = false;
        Result            finalRes(true);
        AsyncEndCollector uploadClientCtx = {&completed, &finalRes};

        const bool endAdded =
            operation.getResponseBodyStream().eventEnd.addListener<AsyncEndCollector, &AsyncEndCollector::onEnd>(
                uploadClientCtx);
        const bool errorAdded =
            operation.getResponseBodyStream().eventError.addListener<AsyncEndCollector, &AsyncEndCollector::onError>(
                uploadClientCtx);
        SC_TEST_EXPECT(endAdded);
        SC_TEST_EXPECT(errorAdded);

        SC_TEST_EXPECT(operation.start(request, response, &uploadBuffers.pool));

        AsyncPipeline pipeline = {&uploadReadable, {}, {&operation.getRequestBodySink()}};
        SC_TEST_EXPECT(pipeline.pipe());
        SC_TEST_EXPECT(pipeline.start());

        while (not completed)
        {
            SC_TEST_EXPECT(loop.runOnce());
        }

        SC_TEST_EXPECT(finalRes);
        SC_TEST_EXPECT(response.statusCode == 200);
        SC_TEST_EXPECT(uploadCollector.receivedSize == UploadSize);
        SC_TEST_EXPECT(memcmp(uploadCollector.received, UploadBody, UploadSize) == 0);

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(server.server.stop());
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(loop.close());
    }
};

namespace SC
{
void runHttpClientTest(SC::TestReport& report) { HttpClientTest test(report); }
} // namespace SC
