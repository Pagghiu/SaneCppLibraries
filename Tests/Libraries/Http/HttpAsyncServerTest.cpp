// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpAsyncServer.h"
#include "HttpStringAppend.h"
#include "HttpTestClient.h"
#include "Libraries/Http/HttpAsyncClient.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct HttpAsyncServerTest;
} // namespace SC

namespace
{
using ClientConnection = SC::HttpAsyncClientConnection<4, 6, 8 * 1024, 8 * 1024>;

struct ResponseCollector
{
    SC::Buffer buffer;

    SC::AsyncReadableStream*     readable = nullptr;
    SC::HttpAsyncClientResponse* response = nullptr;

    SC::Function<void(SC::HttpAsyncClientResponse&)> onEnd;

    void reset() { buffer = {}; }

    SC::Result append(SC::Span<const char> data)
    {
        SC::GrowableBuffer<SC::Buffer> gb(buffer);
        SC::HttpStringAppend&          sb = static_cast<SC::HttpStringAppend&>(static_cast<SC::IGrowableBuffer&>(gb));
        return SC::Result(sb.append(data, 0));
    }

    void attach(SC::HttpAsyncClientResponse&                       newResponse,
                SC::Function<void(SC::HttpAsyncClientResponse&)>&& endCallback = {})
    {
        detach();
        reset();
        response = &newResponse;
        readable = &newResponse.getReadableStream();
        onEnd    = SC::move(endCallback);

        const bool added = readable->eventData.addListener<ResponseCollector, &ResponseCollector::onData>(*this);
        SC_ASSERT_RELEASE(added);
        if (onEnd.isValid())
        {
            const bool addedEnd =
                readable->eventEnd.addListener<ResponseCollector, &ResponseCollector::onStreamEnd>(*this);
            SC_ASSERT_RELEASE(addedEnd);
        }
    }

    void detach()
    {
        if (readable)
        {
            (void)readable->eventData.removeListener<ResponseCollector, &ResponseCollector::onData>(*this);
            if (onEnd.isValid())
            {
                (void)readable->eventEnd.removeListener<ResponseCollector, &ResponseCollector::onStreamEnd>(*this);
            }
            readable = nullptr;
        }
        response = nullptr;
        onEnd    = {};
    }

    void onData(SC::AsyncBufferView::ID bufferID)
    {
        SC::Span<const char> data;
        SC_ASSERT_RELEASE(readable != nullptr);
        SC_ASSERT_RELEASE(readable->getBuffersPool().getReadableData(bufferID, data));
        SC_ASSERT_RELEASE(append(data));
    }

    void onStreamEnd()
    {
        SC_ASSERT_RELEASE(response != nullptr);
        if (onEnd.isValid())
        {
            onEnd(*response);
        }
    }

    [[nodiscard]] SC::StringSpan view() const { return {buffer.toSpanConst(), false, SC::StringEncoding::Ascii}; }
};

struct ChunkedBodyStream : public SC::AsyncReadableStream
{
    SC::AsyncReadableStream::Request readQueue[2];

    SC::AsyncBufferView::ID rootBufferID;
    SC::Span<const char>    sourceData;

    SC::size_t offset    = 0;
    SC::size_t chunkSize = 0;

    ChunkedBodyStream() { setReadQueue(readQueue); }

    SC::Result init(SC::AsyncBuffersPool& pool, SC::Span<const char> data, SC::size_t chunk)
    {
        sourceData   = data;
        offset       = 0;
        chunkSize    = chunk;
        rootBufferID = {};
        SC_TRY(SC::AsyncReadableStream::init(pool));
        SC_TRY(pool.pushBuffer(SC::AsyncBufferView(data), rootBufferID));
        pool.refBuffer(rootBufferID);
        return SC::Result(true);
    }

  private:
    virtual SC::Result asyncRead() override
    {
        if (offset >= sourceData.sizeInBytes())
        {
            pushEnd();
            return SC::Result(true);
        }

        const SC::size_t remaining = sourceData.sizeInBytes() - offset;
        const SC::size_t length    = chunkSize < remaining ? chunkSize : remaining;

        SC::AsyncBufferView::ID childID;
        SC_TRY(getBuffersPool().createChildView(rootBufferID, offset, length, childID));
        const bool shouldContinue = push(childID, length);
        getBuffersPool().unrefBuffer(childID);
        offset += length;
        reactivate(offset < sourceData.sizeInBytes() and shouldContinue);
        return SC::Result(true);
    }

    virtual SC::Result asyncDestroyReadable() override
    {
        if (rootBufferID.isValid())
        {
            getBuffersPool().unrefBuffer(rootBufferID);
            rootBufferID = {};
        }
        return finishedDestroyingReadable();
    }
};

struct RecordedWritableStream : SC::AsyncWritableStream
{
    SC::AsyncWritableStream::Request queue[8];
    SC::Buffer                       output;

    SC::AsyncBufferView::ID                     pendingBufferID;
    SC::Function<void(SC::AsyncBufferView::ID)> pendingCallback;

    RecordedWritableStream() { setWriteQueue(queue); }

    SC::Result append(SC::Span<const char> data)
    {
        SC::GrowableBuffer<SC::Buffer> gb(output);
        SC::HttpStringAppend&          sb = static_cast<SC::HttpStringAppend&>(static_cast<SC::IGrowableBuffer&>(gb));
        return SC::Result(sb.append(data, 0));
    }

    bool flushOne()
    {
        if (not pendingBufferID.isValid())
        {
            return false;
        }

        SC::Span<const char> data;
        SC_ASSERT_RELEASE(getBuffersPool().getReadableData(pendingBufferID, data));
        SC_ASSERT_RELEASE(append(data));

        SC::AsyncBufferView::ID                     savedBufferID = pendingBufferID;
        SC::Function<void(SC::AsyncBufferView::ID)> savedCallback = SC::move(pendingCallback);
        pendingBufferID                                           = {};
        pendingCallback                                           = {};
        getBuffersPool().unrefBuffer(savedBufferID);
        finishedWriting(savedBufferID, SC::move(savedCallback), SC::Result(true));
        return true;
    }

  private:
    virtual SC::Result asyncWrite(SC::AsyncBufferView::ID                     bufferID,
                                  SC::Function<void(SC::AsyncBufferView::ID)> cb) override
    {
        SC_ASSERT_RELEASE(not pendingBufferID.isValid());
        getBuffersPool().refBuffer(bufferID);
        pendingBufferID = bufferID;
        pendingCallback = SC::move(cb);
        return SC::Result(true);
    }
};

struct ProbeHttpResponse : SC::HttpResponse
{
    void setup(SC::Span<char> headers, SC::AsyncWritableStream& stream)
    {
        setHeaderMemory(headers);
        setWritableStream(stream);
        reset();
    }
};

struct TimeoutGuard
{
    SC::AsyncLoopTimeout timeout;

    SC::Result start(SC::AsyncEventLoop& loop, SC::TimeMs duration)
    {
        timeout.callback = [](SC::AsyncLoopTimeout::Result&)
        { SC_ASSERT_RELEASE("Test never finished. Event loop timeout expired." && false); };
        SC_TRY(timeout.start(loop, duration));
        loop.excludeFromActiveCount(timeout);
        return SC::Result(true);
    }
};
} // namespace

struct SC::HttpAsyncServerTest : public SC::TestCase
{
    HttpAsyncServerTest(SC::TestReport& report) : TestCase(report, "HttpAsyncServerTest")
    {
        if (test_section("HttpAsyncServer"))
        {
            httpAsyncServerTest();
        }
        if (test_section("custom response status"))
        {
            customResponseStatus();
        }
        if (test_section("standard response statuses"))
        {
            standardResponseStatuses();
        }
        if (test_section("empty response helper"))
        {
            emptyResponseHelper();
        }
        if (test_section("response body helpers"))
        {
            responseBodyHelpers();
        }
        if (test_section("connection body copy helper"))
        {
            connectionBodyCopyHelper();
        }
        if (test_section("TLS options are server scoped"))
        {
            tlsOptionsAreServerScoped();
        }
        if (test_section("chunked request decoding"))
        {
            chunkedRequestDecoding();
        }
        if (test_section("chunked request rejects trailers"))
        {
            chunkedRequestRejectsTrailers();
        }
        if (test_section("max header size error"))
        {
            maxHeaderSizeError();
        }
        if (test_section("chunked response writing"))
        {
            chunkedResponseWriting();
        }
        if (test_section("chunked client request writing"))
        {
            chunkedClientRequestWriting();
        }
    }
    void httpAsyncServerTest();
    void customResponseStatus();
    void standardResponseStatuses();
    void emptyResponseHelper();
    void responseBodyHelpers();
    void connectionBodyCopyHelper();
    void tlsOptionsAreServerScoped();
    void chunkedRequestDecoding();
    void chunkedRequestRejectsTrailers();
    void maxHeaderSizeError();
    void chunkedResponseWriting();
    void chunkedClientRequestWriting();
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
        if (request.getRequestTarget() != "/index.html" and request.getRequestTarget() != "/")
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

    HttpTestClient client[3];
    struct ClientContext
    {
        int numRequests;
        int wantedNumRequests;

        HttpAsyncServer& httpServer;
    } clientContext = {0, 3, httpServer};
    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/index.html", serverPort));
    for (int idx = 0; idx < clientContext.wantedNumRequests; ++idx)
    {
        client[idx].callback = [this, &clientContext](HttpTestClient& client)
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

    // Safety timeout against hangs
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

void SC::HttpAsyncServerTest::customResponseStatus()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<2, 2, 8 * 1024, 8 * 1024>;

    HttpConnectionType connections[1];
    HttpAsyncServer    httpServer;
    const uint16_t     serverPort = report.mapPort(6153);
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        SC_TEST_EXPECT(connection.response.startResponse(418, "I'm a Teapot"));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", "0"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.end());
    };

    HttpTestClient client;
    client.callback = [this, &httpServer](HttpTestClient& result)
    {
        StringView response(result.getResponse());
        SC_TEST_EXPECT(response.containsString("418 I'm a Teapot"));
        SC_TEST_EXPECT(httpServer.stop());
    };

    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/status", serverPort));
    SC_TEST_EXPECT(client.get(eventLoop, endpoint.view()));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpAsyncServerTest::standardResponseStatuses()
{
    struct ProbeResponse : public HttpResponse
    {
        using HttpOutgoingMessage::setHeaderMemory;

        [[nodiscard]] StringSpan written() const { return {responseHeaders.written(), false, StringEncoding::Ascii}; }
    };

    struct StatusCase
    {
        int        code;
        StringSpan statusLine;
    };

    const StatusCase cases[] = {
        {100, "HTTP/1.1 100 Continue\r\n"},
        {206, "HTTP/1.1 206 Partial Content\r\n"},
        {301, "HTTP/1.1 301 Moved Permanently\r\n"},
        {302, "HTTP/1.1 302 Found\r\n"},
        {304, "HTTP/1.1 304 Not Modified\r\n"},
        {403, "HTTP/1.1 403 Forbidden\r\n"},
        {416, "HTTP/1.1 416 Range Not Satisfiable\r\n"},
        {500, "HTTP/1.1 500 Internal Server Error\r\n"},
    };

    for (const StatusCase& statusCase : cases)
    {
        char          headerStorage[128];
        ProbeResponse response;
        response.setHeaderMemory(headerStorage);
        SC_TEST_EXPECT(response.startResponse(statusCase.code));
        SC_TEST_EXPECT(response.written() == statusCase.statusLine);
    }
}

void SC::HttpAsyncServerTest::emptyResponseHelper()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<2, 2, 8 * 1024, 8 * 1024>;

    HttpConnectionType connections[1];
    HttpAsyncServer    httpServer;
    const uint16_t     serverPort = report.mapPort(6157);
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    httpServer.onRequest = [this](HttpConnection& connection) { SC_TEST_EXPECT(connection.response.sendEmpty(204)); };

    HttpTestClient client;
    client.callback = [this, &httpServer](HttpTestClient& result)
    {
        StringView response(result.getResponse());
        SC_TEST_EXPECT(response.containsString("204 No Content"));
        SC_TEST_EXPECT(response.containsString("Content-Length: 0"));
        SC_TEST_EXPECT(httpServer.stop());
    };

    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/empty", serverPort));
    SC_TEST_EXPECT(client.get(eventLoop, endpoint.view()));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpAsyncServerTest::tlsOptionsAreServerScoped()
{
    HttpAsyncServer server;

    const char certificate[] = "-----BEGIN CERTIFICATE-----\n-----END CERTIFICATE-----\n";
    const char privateKey[]  = "-----BEGIN PRIVATE KEY-----\n-----END PRIVATE KEY-----\n";
    StringSpan protocols[]   = {"h2", "http/1.1"};

    SC_TEST_EXPECT(not server.getTlsOptions().enabled);
    SC_TEST_EXPECT(server.getTlsOptions().pemCertificateChain.empty());
    SC_TEST_EXPECT(server.getTlsOptions().pemPrivateKey.empty());
    SC_TEST_EXPECT(server.getTlsOptions().alpnProtocols.empty());

    HttpAsyncServerTlsOptions options;
    options.enabled             = true;
    options.pemCertificateChain = {certificate, sizeof(certificate) - 1};
    options.pemPrivateKey       = {privateKey, sizeof(privateKey) - 1};
    options.alpnProtocols       = protocols;
    server.setTlsOptions(options);

    SC_TEST_EXPECT(server.getTlsOptions().enabled);
    SC_TEST_EXPECT(server.getTlsOptions().pemCertificateChain.sizeInBytes() == sizeof(certificate) - 1);
    SC_TEST_EXPECT(server.getTlsOptions().pemPrivateKey.sizeInBytes() == sizeof(privateKey) - 1);
    SC_TEST_EXPECT(server.getTlsOptions().alpnProtocols.sizeInElements() == 2);
    SC_TEST_EXPECT(server.getTlsOptions().alpnProtocols[0] == "h2");
    SC_TEST_EXPECT(server.getTlsOptions().alpnProtocols[1] == "http/1.1");

    server.clearTlsOptions();
    SC_TEST_EXPECT(not server.getTlsOptions().enabled);
    SC_TEST_EXPECT(server.getTlsOptions().pemCertificateChain.empty());
    SC_TEST_EXPECT(server.getTlsOptions().pemPrivateKey.empty());
    SC_TEST_EXPECT(server.getTlsOptions().alpnProtocols.empty());
}

void SC::HttpAsyncServerTest::chunkedRequestDecoding()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<2, 2, 8 * 1024, 8 * 1024>;

    HttpConnectionType connections[1];
    HttpAsyncServer    httpServer;
    const uint16_t     serverPort = report.mapPort(6154);
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct ServerContext
    {
        HttpAsyncServerTest* test;
        HttpConnection*      connection;
        HttpAsyncServer*     httpServer;
        Buffer               body;

        Result append(Span<const char> data)
        {
            GrowableBuffer<Buffer> gb(body);
            HttpStringAppend&      sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
            return Result(sb.append(data, 0));
        }

        void onData(AsyncBufferView::ID bufferID)
        {
            SC_ASSERT_RELEASE(connection != nullptr);
            Span<const char> data;
            SC_ASSERT_RELEASE(connection->request.getReadableStream().getBuffersPool().getReadableData(bufferID, data));
            SC_ASSERT_RELEASE(append(data));
            SC_ASSERT_RELEASE(connection->request.consumeBodyBytes(data.sizeInBytes()));
        }

        void onEnd()
        {
            SC_ASSERT_RELEASE(connection != nullptr);
            test->recordExpectation("chunked request framing",
                                    connection->request.getBodyFramingKind() == HttpBodyFramingKind::Chunked);
            test->recordExpectation("decoded chunked request body",
                                    StringSpan(body.toSpanConst(), false, StringEncoding::Ascii) == "hello world");
            test->recordExpectation("startResponse", connection->response.startResponse(200));
            test->recordExpectation("addHeader", connection->response.addHeader("Content-Length", "0"));
            test->recordExpectation("sendHeaders", connection->response.sendHeaders());
            test->recordExpectation("end response", connection->response.end());
        }
    } serverContext = {this, nullptr, &httpServer, {}};

    httpServer.onRequest = [this, &serverContext](HttpConnection& client)
    {
        serverContext.connection = &client;
        const bool addedData =
            client.request.getReadableStream().eventData.addListener<ServerContext, &ServerContext::onData>(
                serverContext);
        SC_TEST_EXPECT(addedData);
        const bool addedEnd =
            client.request.getReadableStream().eventEnd.addListener<ServerContext, &ServerContext::onEnd>(
                serverContext);
        SC_TEST_EXPECT(addedEnd);
    };

    HttpTestClient client;
    String         endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/chunked", serverPort));
    constexpr StringView request = "PUT /chunked HTTP/1.1\r\n"
                                   "Host: 127.0.0.1\r\n"
                                   "Transfer-Encoding: chunked\r\n"
                                   "\r\n"
                                   "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n";

    client.callback = [this, &httpServer](HttpTestClient& clientRef)
    {
        StringView response(clientRef.getResponse());
        SC_TEST_EXPECT(response.containsString("200 OK"));
        SC_TEST_EXPECT(httpServer.stop());
    };
    SC_TEST_EXPECT(client.sendRaw(eventLoop, endpoint.view(), request));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpAsyncServerTest::chunkedRequestRejectsTrailers()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<2, 2, 8 * 1024, 8 * 1024>;

    HttpConnectionType connections[1];
    HttpAsyncServer    httpServer;
    const uint16_t     serverPort = report.mapPort(6155);
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct ServerContext
    {
        bool sawRequest = false;
        bool sawEnd     = false;
        bool sawError   = false;

        void onEnd() { sawEnd = true; }
    } serverContext;

    httpServer.onError = [this, &serverContext](Result result)
    {
        serverContext.sawError = true;
        SC_TEST_EXPECT(not result);
    };

    httpServer.onRequest = [this, &serverContext](HttpConnection& client)
    {
        serverContext.sawRequest = true;
        const bool addedEnd =
            client.request.getReadableStream().eventEnd.addListener<ServerContext, &ServerContext::onEnd>(
                serverContext);
        SC_TEST_EXPECT(addedEnd);
    };

    HttpTestClient client;
    String         endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/chunked-trailer", serverPort));
    constexpr StringView request = "PUT /chunked-trailer HTTP/1.1\r\n"
                                   "Host: 127.0.0.1\r\n"
                                   "Transfer-Encoding: chunked\r\n"
                                   "\r\n"
                                   "5\r\nhello\r\n0\r\nX-Test: yes\r\n\r\n";

    client.callback = [this, &httpServer](HttpTestClient& clientRef)
    {
        StringView response(clientRef.getResponse());
        SC_TEST_EXPECT(not response.containsString("200 OK"));
        SC_TEST_EXPECT(httpServer.stop());
    };
    SC_TEST_EXPECT(client.sendRaw(eventLoop, endpoint.view(), request));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(serverContext.sawRequest);
    SC_TEST_EXPECT(not serverContext.sawEnd);
    SC_TEST_EXPECT(serverContext.sawError);
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpAsyncServerTest::maxHeaderSizeError()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<2, 2, 8 * 1024, 8 * 1024>;

    HttpConnectionType connections[1];
    HttpAsyncServer    httpServer;
    const uint16_t     serverPort = report.mapPort(6156);
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    httpServer.setMaxHeaderSize(64);
    SC_TEST_EXPECT(httpServer.getMaxHeaderSize() == 64);
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    struct ServerContext
    {
        bool sawRequest = false;
        bool sawError   = false;
    } serverContext;

    httpServer.onRequest = [&serverContext](HttpConnection&) { serverContext.sawRequest = true; };
    httpServer.onError   = [this, &serverContext](Result result)
    {
        serverContext.sawError = true;
        SC_TEST_EXPECT(not result);
    };

    HttpTestClient client;
    String         endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}", serverPort));
    constexpr StringView request = "GET /oversized HTTP/1.1\r\n"
                                   "Host: 127.0.0.1\r\n"
                                   "X-Large: 01234567890123456789012345678901234567890123456789\r\n"
                                   "\r\n";

    client.callback = [this, &httpServer](HttpTestClient&) { SC_TEST_EXPECT(httpServer.stop()); };
    SC_TEST_EXPECT(client.sendRaw(eventLoop, endpoint.view(), request));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(not serverContext.sawRequest);
    SC_TEST_EXPECT(serverContext.sawError);
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpAsyncServerTest::responseBodyHelpers()
{
    {
        SC::AsyncBufferView  buffers[4] = {};
        SC::AsyncBuffersPool pool;
        pool.setBuffers(buffers);

        RecordedWritableStream writable;
        SC_TEST_EXPECT(writable.init(pool));

        ProbeHttpResponse response;
        char              headers[256] = {0};
        response.setup(headers, writable);

        SC_TEST_EXPECT(response.sendText(200, "hello"));
        while (writable.flushOne()) {}

        constexpr StringView expected = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: text/plain; charset=utf-8\r\n"
                                        "Content-Length: 5\r\n"
                                        "Connection: keep-alive\r\n"
                                        "\r\n"
                                        "hello";
        SC_TEST_EXPECT(StringSpan(writable.output.toSpanConst(), false, StringEncoding::Ascii) == expected);
    }

    {
        SC::AsyncBufferView  buffers[4] = {};
        SC::AsyncBuffersPool pool;
        pool.setBuffers(buffers);

        RecordedWritableStream writable;
        SC_TEST_EXPECT(writable.init(pool));

        ProbeHttpResponse response;
        char              headers[256] = {0};
        response.setup(headers, writable);

        SC_TEST_EXPECT(response.sendRedirect(302, "/new-place"));
        while (writable.flushOne()) {}

        constexpr StringView expected = "HTTP/1.1 302 Found\r\n"
                                        "Content-Length: 0\r\n"
                                        "Location: /new-place\r\n"
                                        "Connection: keep-alive\r\n"
                                        "\r\n";
        SC_TEST_EXPECT(StringSpan(writable.output.toSpanConst(), false, StringEncoding::Ascii) == expected);
    }

    {
        SC::AsyncBufferView  buffers[4] = {};
        SC::AsyncBuffersPool pool;
        pool.setBuffers(buffers);

        RecordedWritableStream writable;
        SC_TEST_EXPECT(writable.init(pool));

        ProbeHttpResponse response;
        char              headers[256] = {0};
        response.setup(headers, writable);
        SC_TEST_EXPECT(not response.sendRedirect(200, "/not-a-redirect"));

        ProbeHttpResponse emptyLocationResponse;
        emptyLocationResponse.setup(headers, writable);
        SC_TEST_EXPECT(not emptyLocationResponse.sendRedirect(302, ""));
    }

    {
        SC::AsyncBufferView  buffers[4] = {};
        SC::AsyncBuffersPool pool;
        pool.setBuffers(buffers);

        RecordedWritableStream writable;
        SC_TEST_EXPECT(writable.init(pool));

        ProbeHttpResponse response;
        char              headers[256] = {0};
        response.setup(headers, writable);

        SC_TEST_EXPECT(response.sendMethodNotAllowed("GET, POST"));
        while (writable.flushOne()) {}

        constexpr StringView expected = "HTTP/1.1 405 Method Not Allowed\r\n"
                                        "Content-Length: 0\r\n"
                                        "Allow: GET, POST\r\n"
                                        "Connection: keep-alive\r\n"
                                        "\r\n";
        SC_TEST_EXPECT(StringSpan(writable.output.toSpanConst(), false, StringEncoding::Ascii) == expected);
    }

    {
        SC::AsyncBufferView  buffers[4] = {};
        SC::AsyncBuffersPool pool;
        pool.setBuffers(buffers);

        RecordedWritableStream writable;
        SC_TEST_EXPECT(writable.init(pool));

        ProbeHttpResponse response;
        char              headers[256] = {0};
        response.setup(headers, writable);

        SC_TEST_EXPECT(response.sendJson(200, "{\"ok\":true}"));
        while (writable.flushOne()) {}

        constexpr StringView expected = "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: application/json\r\n"
                                        "Content-Length: 11\r\n"
                                        "Connection: keep-alive\r\n"
                                        "\r\n"
                                        "{\"ok\":true}";
        SC_TEST_EXPECT(StringSpan(writable.output.toSpanConst(), false, StringEncoding::Ascii) == expected);
    }

    {
        SC::AsyncBufferView  buffers[4] = {};
        SC::AsyncBuffersPool pool;
        pool.setBuffers(buffers);

        RecordedWritableStream writable;
        SC_TEST_EXPECT(writable.init(pool));

        ProbeHttpResponse response;
        char              headers[256] = {0};
        response.setup(headers, writable);

        SC_TEST_EXPECT(response.startBody(201, 4, "application/json"));
        SC_TEST_EXPECT(response.addHeader("X-Test", "yes"));
        SC_TEST_EXPECT(response.sendHeaders());
        SC_TEST_EXPECT(response.getWritableStream().write("true"));
        SC_TEST_EXPECT(response.end());
        while (writable.flushOne()) {}

        constexpr StringView expected = "HTTP/1.1 201 Created\r\n"
                                        "Content-Type: application/json\r\n"
                                        "Content-Length: 4\r\n"
                                        "X-Test: yes\r\n"
                                        "Connection: keep-alive\r\n"
                                        "\r\n"
                                        "true";
        SC_TEST_EXPECT(StringSpan(writable.output.toSpanConst(), false, StringEncoding::Ascii) == expected);
    }

    {
        SC::AsyncBufferView  buffers[1] = {};
        SC::AsyncBuffersPool pool;
        pool.setBuffers(buffers);

        RecordedWritableStream writable;
        SC_TEST_EXPECT(writable.init(pool));

        ProbeHttpResponse response;
        char              headers[256] = {0};
        response.setup(headers, writable);

        SC_TEST_EXPECT(not response.sendBody(200, "body", "text/plain"));
    }
}

void SC::HttpAsyncServerTest::connectionBodyCopyHelper()
{
    AsyncEventLoop eventLoop;
    SC_TEST_EXPECT(eventLoop.create());

    using HttpConnectionType = HttpAsyncConnection<2, 2, 8 * 1024, 8 * 1024>;

    HttpConnectionType connections[1];
    HttpAsyncServer    httpServer;
    const uint16_t     serverPort = report.mapPort(6158);
    SC_TEST_EXPECT(httpServer.init(Span<HttpConnectionType>(connections)));
    SC_TEST_EXPECT(httpServer.start(eventLoop, "127.0.0.1", serverPort));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        char body[] = {'c', 'o', 'p', 'y'};
        SC_TEST_EXPECT(connection.sendJsonCopy(201, {{body, sizeof(body)}, false, StringEncoding::Ascii}));
        body[0] = 'x';
    };

    HttpTestClient client;
    client.callback = [this, &httpServer](HttpTestClient& result)
    {
        StringView response(result.getResponse());
        SC_TEST_EXPECT(response.containsString("201 Created"));
        SC_TEST_EXPECT(response.containsString("Content-Type: application/json"));
        SC_TEST_EXPECT(response.containsString("Content-Length: 4"));
        SC_TEST_EXPECT(response.containsString("\r\n\r\ncopy"));
        SC_TEST_EXPECT(not response.containsString("\r\n\r\nxopy"));
        SC_TEST_EXPECT(httpServer.stop());
    };

    String endpoint = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(endpoint, "http://127.0.0.1:{}/copy", serverPort));
    SC_TEST_EXPECT(client.get(eventLoop, endpoint.view()));

    AsyncLoopTimeout timeout;
    timeout.callback = [this](AsyncLoopTimeout::Result&)
    { SC_TEST_EXPECT("Test never finished. Event Loop is stuck. Timeout expired." && false); };
    SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{2000}));
    eventLoop.excludeFromActiveCount(timeout);

    SC_TEST_EXPECT(eventLoop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(eventLoop.close());
}

void SC::HttpAsyncServerTest::chunkedResponseWriting()
{
    struct RecordingWritableStream : AsyncWritableStream
    {
        AsyncWritableStream::Request queue[8];
        Buffer                       output;

        AsyncBufferView::ID                 pendingBufferID;
        Function<void(AsyncBufferView::ID)> pendingCallback;

        RecordingWritableStream() { setWriteQueue(queue); }

        Result append(Span<const char> data)
        {
            GrowableBuffer<Buffer> gb(output);
            HttpStringAppend&      sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
            return Result(sb.append(data, 0));
        }

        bool flushOne()
        {
            if (not pendingBufferID.isValid())
            {
                return false;
            }

            Span<const char> data;
            SC_ASSERT_RELEASE(getBuffersPool().getReadableData(pendingBufferID, data));
            SC_ASSERT_RELEASE(append(data));

            AsyncBufferView::ID                 savedBufferID = pendingBufferID;
            Function<void(AsyncBufferView::ID)> savedCallback = move(pendingCallback);
            pendingBufferID                                   = {};
            pendingCallback                                   = {};
            getBuffersPool().unrefBuffer(savedBufferID);
            finishedWriting(savedBufferID, move(savedCallback), Result(true));
            return true;
        }

      private:
        virtual Result asyncWrite(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb) override
        {
            SC_ASSERT_RELEASE(not pendingBufferID.isValid());
            getBuffersPool().refBuffer(bufferID);
            pendingBufferID = bufferID;
            pendingCallback = move(cb);
            return Result(true);
        }
    };

    struct ProbeResponse : HttpResponse
    {
        void setup(Span<char> headers, AsyncWritableStream& stream)
        {
            setHeaderMemory(headers);
            setWritableStream(stream);
            reset();
        }
    };

    AsyncBufferView  buffers[8] = {};
    AsyncBuffersPool pool;
    pool.setBuffers(buffers);

    RecordingWritableStream writable;
    SC_TEST_EXPECT(writable.init(pool));

    ProbeResponse response;
    char          headers[256] = {0};
    response.setup(headers, writable);

    SC_TEST_EXPECT(response.startResponse(200));
    SC_TEST_EXPECT(response.setChunkedTransferEncoding());
    SC_TEST_EXPECT(response.sendHeaders());
    SC_TEST_EXPECT(response.getWritableStream().write("hello"));
    SC_TEST_EXPECT(response.getWritableStream().write(" world"));
    SC_TEST_EXPECT(response.end());

    while (writable.flushOne()) {}

    constexpr StringView expected = "HTTP/1.1 200 OK\r\n"
                                    "Transfer-Encoding: chunked\r\n"
                                    "Connection: keep-alive\r\n"
                                    "\r\n"
                                    "5\r\nhello\r\n"
                                    "6\r\n world\r\n"
                                    "0\r\n\r\n";
    SC_TEST_EXPECT(StringSpan(writable.output.toSpanConst(), false, StringEncoding::Ascii) == expected);
}

void SC::HttpAsyncServerTest::chunkedClientRequestWriting()
{
    struct RecordingWritableStream : AsyncWritableStream
    {
        AsyncWritableStream::Request queue[8];
        Buffer                       output;

        AsyncBufferView::ID                 pendingBufferID;
        Function<void(AsyncBufferView::ID)> pendingCallback;

        RecordingWritableStream() { setWriteQueue(queue); }

        Result append(Span<const char> data)
        {
            GrowableBuffer<Buffer> gb(output);
            HttpStringAppend&      sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
            return Result(sb.append(data, 0));
        }

        bool flushOne()
        {
            if (not pendingBufferID.isValid())
            {
                return false;
            }

            Span<const char> data;
            SC_ASSERT_RELEASE(getBuffersPool().getReadableData(pendingBufferID, data));
            SC_ASSERT_RELEASE(append(data));

            AsyncBufferView::ID                 savedBufferID = pendingBufferID;
            Function<void(AsyncBufferView::ID)> savedCallback = move(pendingCallback);
            pendingBufferID                                   = {};
            pendingCallback                                   = {};
            getBuffersPool().unrefBuffer(savedBufferID);
            finishedWriting(savedBufferID, move(savedCallback), Result(true));
            return true;
        }

      private:
        virtual Result asyncWrite(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb) override
        {
            SC_ASSERT_RELEASE(not pendingBufferID.isValid());
            getBuffersPool().refBuffer(bufferID);
            pendingBufferID = bufferID;
            pendingCallback = move(cb);
            return Result(true);
        }
    };

    struct ProbeRequest : HttpAsyncClientRequest
    {
        void setup(Span<char> headers, AsyncWritableStream& stream)
        {
            setHeaderMemory(headers);
            setWritableStream(stream);
            reset();
        }
    };

    AsyncBufferView  buffers[8] = {};
    AsyncBuffersPool pool;
    pool.setBuffers(buffers);

    RecordingWritableStream writable;
    SC_TEST_EXPECT(writable.init(pool));

    ProbeRequest request;
    char         headers[256] = {0};
    request.setup(headers, writable);

    ChunkedBodyStream bodyStream;
    SC_TEST_EXPECT(bodyStream.init(pool, StringSpan("ChunkedBody").toCharSpan(), 3));

    SC_TEST_EXPECT(request.startRequest(HttpParser::Method::HttpPUT, "/chunked"));
    SC_TEST_EXPECT(request.addHeader("Host", "127.0.0.1"));
    SC_TEST_EXPECT(request.setBody(bodyStream));
    SC_TEST_EXPECT(request.sendHeaders());

    AsyncPipeline pipeline;
    pipeline.source   = &bodyStream;
    pipeline.sinks[0] = &request.getWritableStream();
    SC_TEST_EXPECT(pipeline.pipe());
    SC_TEST_EXPECT(pipeline.start());
    while (writable.flushOne()) {}

    SC_TEST_EXPECT(request.end());
    while (writable.flushOne()) {}

    constexpr StringView expected = "PUT /chunked HTTP/1.1\r\n"
                                    "Host: 127.0.0.1\r\n"
                                    "Transfer-Encoding: chunked\r\n"
                                    "User-Agent: SC\r\n"
                                    "Connection: keep-alive\r\n"
                                    "\r\n"
                                    "3\r\nChu\r\n"
                                    "3\r\nnke\r\n"
                                    "3\r\ndBo\r\n"
                                    "2\r\ndy\r\n"
                                    "0\r\n\r\n";
    SC_TEST_EXPECT(StringSpan(writable.output.toSpanConst(), false, StringEncoding::Ascii) == expected);
}

namespace SC
{
void runHttpAsyncServerTest(SC::TestReport& report) { HttpAsyncServerTest test(report); }
} // namespace SC
