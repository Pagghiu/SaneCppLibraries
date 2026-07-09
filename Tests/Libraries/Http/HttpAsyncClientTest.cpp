// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpAsyncClient.h"
#include "HttpStringAppend.h"
#include "Libraries/AsyncStreams/Internal/ZLibAPI.h"
#include "Libraries/AsyncStreams/ZLibTransformStreams.h"
#include "Libraries/Common/Assert.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Http/HttpAsyncFileServer.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct HttpAsyncClientTest;
}

namespace
{
using ServerConnection = SC::HttpAsyncConnection<3, 3, 8 * 1024, 8 * 1024>;
using ClientConnection = SC::HttpAsyncClientConnection<4, 6, 8 * 1024, 8 * 1024>;
using size_t           = SC::size_t;

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

        SC::HttpStringAppend& sb = static_cast<SC::HttpStringAppend&>(static_cast<SC::IGrowableBuffer&>(gb));
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

    size_t offset    = 0;
    size_t chunkSize = 0;

    ChunkedBodyStream() { setReadQueue(readQueue); }

    SC::Result init(SC::AsyncBuffersPool& pool, SC::Span<const char> data, size_t chunk)
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

        const size_t remaining = sourceData.sizeInBytes() - offset;
        const size_t length    = chunkSize < remaining ? chunkSize : remaining;

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

static bool compressionTestsAvailable(SC::TestReport& report)
{
    (void)(report);
    SC::ZLibAPI zlib;
    if (not zlib.load())
    {
        return false;
    }
    zlib.unload();
    return true;
}

static SC::Result decompressForTest(SC::ZLibStream::Algorithm algorithm, SC::Span<const char> input,
                                    SC::Span<char>& decoded)
{
    SC::ZLibStream stream;
    SC_TRY(stream.init(algorithm));

    static char    outputStorage[512];
    SC::Span<char> remaining = outputStorage;
    SC_TRY(stream.process(input, remaining));
    SC_TRY_MSG(input.empty(), "HttpAsyncClientTest compressed fixture output too small");

    bool streamEnded = false;
    SC_TRY(stream.finalize(remaining, streamEnded));
    SC_TRY_MSG(streamEnded, "HttpAsyncClientTest compressed fixture did not end");
    decoded = {outputStorage, sizeof(outputStorage) - remaining.sizeInBytes()};
    return SC::Result(true);
}

static bool resultMessageEquals(SC::Result result, SC::StringSpan expected)
{
    if (result or result.message == nullptr)
    {
        return false;
    }
    return SC::StringSpan::fromNullTerminated(result.message, SC::StringEncoding::Ascii) == expected;
}
} // namespace

struct SC::HttpAsyncClientTest : public SC::TestCase
{
    HttpAsyncClientTest(SC::TestReport& report) : TestCase(report, "HttpAsyncClientTest")
    {
        if (test_section("basic GET"))
        {
            basicGet();
        }
        if (test_section("PUT with span body"))
        {
            putSpanBody();
        }
        if (test_section("request options"))
        {
            requestOptions();
        }
        if (test_section("request option body helpers"))
        {
            requestOptionBodyHelpers();
        }
        if (test_section("multipart writer validation"))
        {
            multipartWriterValidation();
        }
        if (test_section("PUT with streamed body"))
        {
            putStreamBody();
        }
        if (test_section("PUT with chunked streamed body"))
        {
            putChunkedStreamBody();
        }
        if (test_section("PUT with writable body"))
        {
            putWritableBody();
        }
        if (test_section("keep-alive reuse and origin reconnect"))
        {
            keepAliveAndReconnect();
        }
        if (test_section("transport setup hook defers request"))
        {
            transportSetupHookDefersRequest();
        }
        if (test_section("HTTPS transport setup hook dispatch"))
        {
            httpsTransportSetupHookDispatch();
        }
        if (test_section("HTTPS transport setup reports TLS backend errors"))
        {
            httpsTransportSetupReportsTlsBackendError();
        }
        if (test_section("HTTPS requires transport adapter"))
        {
            httpsRequiresTransportAdapter();
        }
        if (test_section("zero-length response"))
        {
            zeroLengthResponse();
        }
        if (test_section("chunked response"))
        {
            chunkedResponse();
        }
        if (test_section("chunked response rejects trailers"))
        {
            chunkedResponseRejectsTrailers();
        }
        if (test_section("close-delimited response"))
        {
            closeDelimitedResponse();
        }
        if (test_section("gzip response decompression") and compressionTestsAvailable(report))
        {
            gzipResponseDecompression();
        }
        if (test_section("deflate response decompression") and compressionTestsAvailable(report))
        {
            deflateResponseDecompression();
        }
        if (test_section("gzip request compression") and compressionTestsAvailable(report))
        {
            gzipRequestCompression();
        }
        if (test_section("multipart upload"))
        {
            multipartUpload();
        }
        if (test_section("HEAD response"))
        {
            headResponse();
        }
        if (test_section("common method wrappers"))
        {
            commonMethodWrappers();
        }
    }

    void basicGet();
    void headResponse();
    void commonMethodWrappers();
    void putSpanBody();
    void requestOptions();
    void requestOptionBodyHelpers();
    void multipartWriterValidation();
    void putStreamBody();
    void putChunkedStreamBody();
    void putWritableBody();
    void keepAliveAndReconnect();
    void transportSetupHookDefersRequest();
    void httpsTransportSetupHookDispatch();
    void httpsTransportSetupReportsTlsBackendError();
    void httpsRequiresTransportAdapter();
    void zeroLengthResponse();
    void chunkedResponse();
    void chunkedResponseRejectsTrailers();
    void closeDelimitedResponse();
    void gzipResponseDecompression();
    void deflateResponseDecompression();
    void gzipRequestCompression();
    void multipartUpload();
};

void SC::HttpAsyncClientTest::requestOptionBodyHelpers()
{
    HttpAsyncClient::RequestOptions options;
    SC_TEST_EXPECT(options.bodyMode == HttpAsyncClient::RequestOptions::BodyMode::None);

    HttpAsyncClient::Header headers[] = {{"X-Test", "yes"}, {"X-Mode", "options"}};
    SC_TEST_EXPECT(&options.setRequest(HttpParser::Method::HttpPOST, "http://example.com/post", true) == &options);
    SC_TEST_EXPECT(options.method == HttpParser::Method::HttpPOST);
    SC_TEST_EXPECT(options.url == "http://example.com/post");
    SC_TEST_EXPECT(options.keepAlive);
    SC_TEST_EXPECT(&options.setHeaders(headers) == &options);
    SC_TEST_EXPECT(options.headers.sizeInElements() == 2);
    SC_TEST_EXPECT(options.headers[0].name == "X-Test");
    SC_TEST_EXPECT(options.headers[1].value == "options");
    SC_TEST_EXPECT(&options.setKeepAlive(false) == &options);
    SC_TEST_EXPECT(not options.keepAlive);
    SC_TEST_EXPECT(&options.setKeepAlive() == &options);
    SC_TEST_EXPECT(options.keepAlive);

    HttpAsyncClient::RequestOptions* returned = &options.setBody(StringSpan("abc"));
    SC_TEST_EXPECT(returned == &options);
    SC_TEST_EXPECT(options.bodyMode == HttpAsyncClient::RequestOptions::BodyMode::Span);
    SC_TEST_EXPECT(options.body.sizeInBytes() == 3);
    SC_TEST_EXPECT(options.bodyLength == 3);
    SC_TEST_EXPECT(options.bodyStream == nullptr);
    SC_TEST_EXPECT(options.multipartWriter == nullptr);

    ChunkedBodyStream bodyStream;
    SC_TEST_EXPECT(&options.setBody(bodyStream, 42) == &options);
    SC_TEST_EXPECT(options.bodyMode == HttpAsyncClient::RequestOptions::BodyMode::Stream);
    SC_TEST_EXPECT(options.body.empty());
    SC_TEST_EXPECT(options.bodyLength == 42);
    SC_TEST_EXPECT(options.bodyStream == &bodyStream);
    SC_TEST_EXPECT(options.multipartWriter == nullptr);

    HttpMultipartWriter writer;
    SC_TEST_EXPECT(&options.setMultipart(writer) == &options);
    SC_TEST_EXPECT(options.bodyMode == HttpAsyncClient::RequestOptions::BodyMode::Multipart);
    SC_TEST_EXPECT(options.body.empty());
    SC_TEST_EXPECT(options.bodyLength == 0);
    SC_TEST_EXPECT(options.bodyStream == nullptr);
    SC_TEST_EXPECT(options.multipartWriter == &writer);

    SC_TEST_EXPECT(&options.clearBody() == &options);
    SC_TEST_EXPECT(options.bodyMode == HttpAsyncClient::RequestOptions::BodyMode::None);
    SC_TEST_EXPECT(options.body.empty());
    SC_TEST_EXPECT(options.bodyLength == 0);
    SC_TEST_EXPECT(options.bodyStream == nullptr);
    SC_TEST_EXPECT(options.multipartWriter == nullptr);
}

void SC::HttpAsyncClientTest::multipartWriterValidation()
{
    HttpMultipartWriter writer;

    SC_TEST_EXPECT(
        resultMessageEquals(writer.addField("field", "value"), "HttpMultipartWriter::addField boundary not set"));
    SC_TEST_EXPECT(
        resultMessageEquals(writer.setBoundary("bad boundary"), "HttpMultipartWriter::setBoundary unsafe boundary"));
    SC_TEST_EXPECT(resultMessageEquals(writer.setBoundary(StringSpan({"\r\n", 2}, false, StringEncoding::Ascii)),
                                       "HttpMultipartWriter::setBoundary unsafe boundary"));

    SC_TEST_EXPECT(writer.setBoundary("----SCMultipartBoundary"));
    SC_TEST_EXPECT(resultMessageEquals(writer.addField("", "value"), "HttpMultipartWriter::addField empty field name"));
    SC_TEST_EXPECT(
        resultMessageEquals(writer.addField("bad\"name", "value"), "HttpMultipartWriter::addField unsafe field name"));
    SC_TEST_EXPECT(resultMessageEquals(writer.addFile("", "ok.txt", StringSpan("body").toCharSpan()),
                                       "HttpMultipartWriter::addFile empty field name"));
    SC_TEST_EXPECT(resultMessageEquals(writer.addFile("file", "bad\"name.txt", StringSpan("body").toCharSpan()),
                                       "HttpMultipartWriter::addFile unsafe file name"));
    SC_TEST_EXPECT(resultMessageEquals(writer.addFile("file", "bad\\name.txt", StringSpan("body").toCharSpan()),
                                       "HttpMultipartWriter::addFile unsafe file name"));
    SC_TEST_EXPECT(
        resultMessageEquals(writer.addFile("file", "ok.txt", StringSpan("body").toCharSpan(), "text/plain\r\nX-Bad: 1"),
                            "HttpMultipartWriter::addFile unsafe content type"));
    SC_TEST_EXPECT(writer.getNumParts() == 0);

    SC_TEST_EXPECT(writer.addField("field", "value"));
    SC_TEST_EXPECT(writer.addFile("file", "safe-name.txt", StringSpan("body").toCharSpan(), "text/plain"));
    SC_TEST_EXPECT(writer.getNumParts() == 2);

    for (size_t idx = writer.getNumParts(); idx < HttpMultipartWriter::MaxParts; ++idx)
    {
        SC_TEST_EXPECT(writer.addField("field", "value"));
    }
    SC_TEST_EXPECT(
        resultMessageEquals(writer.addField("field", "value"), "HttpMultipartWriter::addField too many parts"));
    SC_TEST_EXPECT(resultMessageEquals(writer.addFile("file", "safe-name.txt", StringSpan("body").toCharSpan()),
                                       "HttpMultipartWriter::addFile too many parts"));
}

void SC::HttpAsyncClientTest::basicGet()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[2];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26100);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", "5"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.getWritableStream().write("hello"));
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;

    TimeoutGuard timeout;
    String       url = StringEncoding::Ascii;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
    } ctx = {collector, httpServer};

    SC_TEST_EXPECT(client.init(clientStorage));
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/hello", port));

    //! [HttpAsyncClientBasicSnippet]
    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(StringView(ctx.collector.view()) == "hello");
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(client.get(loop, url.view()));
    //! [HttpAsyncClientBasicSnippet]

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::httpsRequiresTransportAdapter()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    const uint16_t tcpPort       = report.mapPort(8051);
    StringSpan     serverAddress = "127.0.0.1";

    SocketIPAddress nativeAddress;
    SC_TEST_EXPECT(nativeAddress.fromAddressPort(serverAddress, tcpPort));

    SocketDescriptor serverSocket;
    SocketServer     server(serverSocket);
    SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily()));
    SC_TEST_EXPECT(server.bind(nativeAddress));
    SC_TEST_EXPECT(server.listen(1));

    ClientConnection clientStorage;
    HttpAsyncClient  client;

    Result error(false);
    SC_TEST_EXPECT(client.init(clientStorage));
    client.onError = [&error](Result result) { error = result; };

    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "https://127.0.0.1:{}/", tcpPort));

    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(resultMessageEquals(error, "HttpAsyncClient HTTPS transport not configured"));
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(server.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::headResponse()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[2];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26110);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        SC_TEST_EXPECT(connection.request.getParser().method == HttpParser::Method::HttpHEAD);
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", "5"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;

    TimeoutGuard timeout;
    String       url = StringEncoding::Ascii;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
    } ctx = {collector, httpServer};

    SC_TEST_EXPECT(client.init(clientStorage));
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/hello", port));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(ctx.collector.view().isEmpty());
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(client.head(loop, url.view()));

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::commonMethodWrappers()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[2];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26111);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    struct ServerContext
    {
        HttpAsyncClientTest* test       = nullptr;
        HttpConnection*      connection = nullptr;

        Buffer patchBody;

        int patchRequests   = 0;
        int optionsRequests = 0;
        int deleteRequests  = 0;

        Result append(Span<const char> data)
        {
            GrowableBuffer<Buffer> gb(patchBody);
            HttpStringAppend&      sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
            return Result(sb.append(data, 0));
        }

        Result sendEmptyResponse(int statusCode)
        {
            SC_TRY(connection->response.startResponse(statusCode));
            SC_TRY(connection->response.addHeader("Content-Length", "0"));
            SC_TRY(connection->response.sendHeaders());
            return connection->response.end();
        }

        void onPatchData(AsyncBufferView::ID bufferID)
        {
            Span<const char> data;
            SC_ASSERT_RELEASE(connection != nullptr);
            SC_ASSERT_RELEASE(connection->request.getReadableStream().getBuffersPool().getReadableData(bufferID, data));
            SC_ASSERT_RELEASE(append(data));
            SC_ASSERT_RELEASE(connection->request.consumeBodyBytes(data.sizeInBytes()));
        }

        void onPatchEnd()
        {
            test->recordExpectation("patch body",
                                    StringView(patchBody.toSpanConst(), false, StringEncoding::Ascii) == "patch");
            test->recordExpectation("patch response", sendEmptyResponse(200));
        }

        explicit ServerContext(HttpAsyncClientTest* testCase) : test(testCase) {}
    } serverContext(this);

    httpServer.onRequest = [this, &serverContext](HttpConnection& connection)
    {
        serverContext.connection = &connection;
        if (connection.request.getParser().method == HttpParser::Method::HttpPATCH)
        {
            serverContext.patchRequests++;
            SC_TEST_EXPECT(connection.request.getRequestTarget() == "/patch");
            const bool addedData =
                connection.request.getReadableStream()
                    .eventData.addListener<ServerContext, &ServerContext::onPatchData>(serverContext);
            const bool addedEnd =
                connection.request.getReadableStream().eventEnd.addListener<ServerContext, &ServerContext::onPatchEnd>(
                    serverContext);
            SC_TEST_EXPECT(addedData);
            SC_TEST_EXPECT(addedEnd);
            return;
        }
        if (connection.request.getParser().method == HttpParser::Method::HttpOPTIONS)
        {
            serverContext.optionsRequests++;
            SC_TEST_EXPECT(connection.request.getRequestTarget() == "/options");
            SC_TEST_EXPECT(serverContext.sendEmptyResponse(204));
            return;
        }
        if (connection.request.getParser().method == HttpParser::Method::HttpDELETE)
        {
            serverContext.deleteRequests++;
            SC_TEST_EXPECT(connection.request.getRequestTarget() == "/delete");
            SC_TEST_EXPECT(serverContext.sendEmptyResponse(204));
            return;
        }
        SC_TEST_EXPECT(false);
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;
    AsyncLoopTimeout  deferredStep;

    String patchURL   = StringEncoding::Ascii;
    String optionsURL = StringEncoding::Ascii;
    String deleteURL  = StringEncoding::Ascii;

    int completions = 0;
    struct ClientContext
    {
        enum class DeferredAction : uint8_t
        {
            None,
            Options,
            Delete,
            StopServer,
        };

        HttpAsyncClientTest* test = nullptr;

        HttpAsyncClient&   client;
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;

        AsyncEventLoop&   loop;
        AsyncLoopTimeout& deferredStep;

        String& patchURL;
        String& optionsURL;
        String& deleteURL;

        int& completions;

        DeferredAction deferredAction = DeferredAction::None;

        Result scheduleDeferred(DeferredAction action)
        {
            deferredAction = action;
            deferredStep.callback.bind<ClientContext, &ClientContext::onDeferred>(*this);
            return deferredStep.start(loop, TimeMs{0});
        }

        void onDeferred(AsyncLoopTimeout::Result&)
        {
            switch (deferredAction)
            {
            case DeferredAction::Options:
                test->recordExpectation("options", client.options(loop, optionsURL.view(), true));
                break;
            case DeferredAction::Delete:
                test->recordExpectation("deleteRequest", client.deleteRequest(loop, deleteURL.view(), true));
                break;
            case DeferredAction::StopServer: test->recordExpectation("stop server", httpServer.stop()); break;
            case DeferredAction::None: break;
            }
            deferredAction = DeferredAction::None;
        }
    } clientContext = {this,         client,   collector,  httpServer, loop,
                       deferredStep, patchURL, optionsURL, deleteURL,  completions};

    SC_TEST_EXPECT(client.init(clientStorage));
    SC_TEST_EXPECT(StringBuilder::format(patchURL, "http://127.0.0.1:{}/patch", port));
    SC_TEST_EXPECT(StringBuilder::format(optionsURL, "http://127.0.0.1:{}/options", port));
    SC_TEST_EXPECT(StringBuilder::format(deleteURL, "http://127.0.0.1:{}/delete", port));

    client.onResponse = [this, &clientContext](HttpAsyncClientResponse& response)
    {
        clientContext.collector.attach(
            response,
            [this, &clientContext](HttpAsyncClientResponse& completedResponse)
            {
                clientContext.collector.detach();
                clientContext.completions++;
                SC_TEST_EXPECT(clientContext.collector.view().isEmpty());
                if (clientContext.completions == 1)
                {
                    SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                    SC_TEST_EXPECT(clientContext.scheduleDeferred(ClientContext::DeferredAction::Options));
                }
                else if (clientContext.completions == 2)
                {
                    SC_TEST_EXPECT(completedResponse.getParser().statusCode == 204);
                    SC_TEST_EXPECT(clientContext.scheduleDeferred(ClientContext::DeferredAction::Delete));
                }
                else
                {
                    SC_TEST_EXPECT(completedResponse.getParser().statusCode == 204);
                    SC_TEST_EXPECT(clientContext.scheduleDeferred(ClientContext::DeferredAction::StopServer));
                }
            });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.patch(loop, patchURL.view(), StringSpan("patch"), true));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(serverContext.patchRequests == 1);
    SC_TEST_EXPECT(serverContext.optionsRequests == 1);
    SC_TEST_EXPECT(serverContext.deleteRequests == 1);
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::putSpanBody()
{
    StringView     webServerFolder = report.applicationRootDirectory.view();
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    HttpAsyncFileServer::StreamQueue<2> streams[1];

    ServerConnection    connections[1];
    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    const uint16_t port = report.mapPort(26101);

    ThreadPool threadPool;
    if (loop.needsThreadPoolForFileOperations())
    {
        SC_TEST_EXPECT(threadPool.create(2));
    }
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));
    SC_TEST_EXPECT(fileServer.init(threadPool, loop, webServerFolder));
    struct ServerContext
    {
        HttpAsyncFileServer&                 fileServer;
        HttpAsyncFileServer::StreamQueue<2>* streams;
    } serverCtx          = {fileServer, streams};
    httpServer.onRequest = [this, &serverCtx](HttpConnection& connection)
    {
        SC_TEST_EXPECT(
            serverCtx.fileServer.handleRequest(serverCtx.streams[connection.getConnectionID().getIndex()], connection));
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;
    FileSystem        fs;

    String url = StringEncoding::Ascii;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
        FileSystem&        fs;
    } ctx = {collector, httpServer, fs};

    SC_TEST_EXPECT(fs.init(webServerFolder));
    SC_TEST_EXPECT(client.init(clientStorage));
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/client-put-span.txt", port));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 201);
                                 String content;
                                 SC_TEST_EXPECT(ctx.fs.read("client-put-span.txt", content));
                                 SC_TEST_EXPECT(content == "InlineBody");
                                 SC_TEST_EXPECT(ctx.fs.removeFile("client-put-span.txt"));
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.put(loop, url.view(), StringSpan("InlineBody")));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(fileServer.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::requestOptions()
{
    StringView     webServerFolder = report.applicationRootDirectory.view();
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    HttpAsyncFileServer::StreamQueue<2> streams[1];

    ServerConnection    connections[1];
    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    const uint16_t port = report.mapPort(26118);

    ThreadPool threadPool;
    if (loop.needsThreadPoolForFileOperations())
    {
        SC_TEST_EXPECT(threadPool.create(2));
    }
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));
    SC_TEST_EXPECT(fileServer.init(threadPool, loop, webServerFolder));
    struct ServerContext
    {
        HttpAsyncFileServer&                 fileServer;
        HttpAsyncFileServer::StreamQueue<2>* streams;
    } serverCtx          = {fileServer, streams};
    httpServer.onRequest = [this, &serverCtx](HttpConnection& connection)
    {
        StringSpan header;
        SC_TEST_EXPECT(connection.request.getHeader("X-Upload-Mode", header));
        SC_TEST_EXPECT(header == "request-options");
        SC_TEST_EXPECT(
            serverCtx.fileServer.handleRequest(serverCtx.streams[connection.getConnectionID().getIndex()], connection));
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;
    FileSystem        fs;

    String url = StringEncoding::Ascii;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
        FileSystem&        fs;
    } ctx = {collector, httpServer, fs};

    SC_TEST_EXPECT(fs.init(webServerFolder));
    SC_TEST_EXPECT(client.init(clientStorage));
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/client-request-options.txt", port));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 201);
                                 String content;
                                 SC_TEST_EXPECT(ctx.fs.read("client-request-options.txt", content));
                                 SC_TEST_EXPECT(content == "OptionsBody");
                                 SC_TEST_EXPECT(ctx.fs.removeFile("client-request-options.txt"));
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    HttpAsyncClient::RequestOptions invalidOptions;
    invalidOptions.method     = HttpParser::Method::HttpPUT;
    invalidOptions.url        = url.view();
    invalidOptions.bodyMode   = HttpAsyncClient::RequestOptions::BodyMode::Stream;
    invalidOptions.bodyLength = 11;
    SC_TEST_EXPECT(resultMessageEquals(client.sendRequest(loop, invalidOptions),
                                       "HttpAsyncClient RequestOptions body stream missing"));

    HttpAsyncClient::Header headers[] = {{"X-Upload-Mode", "request-options"}};

    HttpAsyncClient::RequestOptions options;
    options.setRequest(HttpParser::Method::HttpPUT, url.view()).setHeaders(headers);
    options.setBody("OptionsBody");

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.sendRequest(loop, options));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(fileServer.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::putStreamBody()
{
    StringView     webServerFolder = report.applicationRootDirectory.view();
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    HttpAsyncFileServer::StreamQueue<2> streams[1];

    ServerConnection    connections[1];
    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    const uint16_t port = report.mapPort(26102);

    ThreadPool threadPool;
    if (loop.needsThreadPoolForFileOperations())
    {
        SC_TEST_EXPECT(threadPool.create(2));
    }
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));
    SC_TEST_EXPECT(fileServer.init(threadPool, loop, webServerFolder));
    struct ServerContext
    {
        HttpAsyncFileServer&                 fileServer;
        HttpAsyncFileServer::StreamQueue<2>* streams;
    } serverCtx          = {fileServer, streams};
    httpServer.onRequest = [this, &serverCtx](HttpConnection& connection)
    {
        SC_TEST_EXPECT(
            serverCtx.fileServer.handleRequest(serverCtx.streams[connection.getConnectionID().getIndex()], connection));
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    ChunkedBodyStream bodyStream;
    TimeoutGuard      timeout;

    FileSystem fs;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
        FileSystem&        fs;
    } ctx = {collector, httpServer, fs};

    SC_TEST_EXPECT(fs.init(webServerFolder));
    SC_TEST_EXPECT(client.init(clientStorage));
    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/client-put-stream.txt", port));
    SC_TEST_EXPECT(bodyStream.init(clientStorage.buffersPool, StringSpan("ChunkedBody").toCharSpan(), 3));

    //! [HttpAsyncClientStreamSnippet]
    client.onPrepareRequest = [this, &bodyStream](HttpAsyncClientRequest& request)
    {
        request.setBody(bodyStream, 11);
        SC_TEST_EXPECT(request.sendHeaders());
    };

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 201);
                                 String content;
                                 SC_TEST_EXPECT(ctx.fs.read("client-put-stream.txt", content));
                                 SC_TEST_EXPECT(content == "ChunkedBody");
                                 SC_TEST_EXPECT(ctx.fs.removeFile("client-put-stream.txt"));
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(client.start(loop, HttpParser::Method::HttpPUT, url.view()));
    //! [HttpAsyncClientStreamSnippet]

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(fileServer.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::putChunkedStreamBody()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26111);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    struct ServerContext
    {
        HttpConnection* connection = nullptr;

        Buffer body;
        bool   responseSent = false;

        Result append(Span<const char> data)
        {
            GrowableBuffer<Buffer> gb(body);
            HttpStringAppend&      sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
            return Result(sb.append(data, 0));
        }

        void onData(AsyncBufferView::ID bufferID)
        {
            SC_ASSERT_RELEASE(connection != nullptr);
            AsyncReadableStream& readable = connection->request.getReadableStream();
            Span<const char>     data;
            SC_ASSERT_RELEASE(readable.getBuffersPool().getReadableData(bufferID, data));
            SC_ASSERT_RELEASE(append(data));
            SC_ASSERT_RELEASE(connection->request.consumeBodyBytes(data.sizeInBytes()));
        }

        void onEnd()
        {
            SC_ASSERT_RELEASE(connection != nullptr);
            SC_ASSERT_RELEASE(connection->request.getBodyFramingKind() == HttpBodyFramingKind::Chunked);
            SC_ASSERT_RELEASE(StringView(StringSpan(body.toSpanConst(), false, StringEncoding::Ascii)) ==
                              "ChunkedBody");
            responseSent = true;
            SC_ASSERT_RELEASE(connection->response.startResponse(200));
            SC_ASSERT_RELEASE(connection->response.addHeader("Content-Length", "6"));
            SC_ASSERT_RELEASE(connection->response.sendHeaders());
            SC_ASSERT_RELEASE(connection->response.getWritableStream().write("stored"));
            SC_ASSERT_RELEASE(connection->response.end());
        }

        [[nodiscard]] StringSpan view() const { return {body.toSpanConst(), false, StringEncoding::Ascii}; }
    } serverCtx;

    httpServer.onRequest = [this, &serverCtx](HttpConnection& connection)
    {
        serverCtx.connection = &connection;
        const bool addedData =
            connection.request.getReadableStream().eventData.addListener<ServerContext, &ServerContext::onData>(
                serverCtx);
        SC_TEST_EXPECT(addedData);
        const bool addedEnd =
            connection.request.getReadableStream().eventEnd.addListener<ServerContext, &ServerContext::onEnd>(
                serverCtx);
        SC_TEST_EXPECT(addedEnd);
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    ChunkedBodyStream bodyStream;
    TimeoutGuard      timeout;

    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
        ServerContext&     serverCtx;
    } ctx = {collector, httpServer, serverCtx};

    SC_TEST_EXPECT(client.init(clientStorage));
    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/chunked-upload", port));
    SC_TEST_EXPECT(bodyStream.init(clientStorage.buffersPool, StringSpan("ChunkedBody").toCharSpan(), 3));

    client.onPrepareRequest = [this, &bodyStream](HttpAsyncClientRequest& request)
    {
        SC_TEST_EXPECT(request.setBody(bodyStream));
        SC_TEST_EXPECT(request.sendHeaders());
    };
    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(StringView(ctx.collector.view()) == "stored");
                                 SC_TEST_EXPECT(StringView(ctx.serverCtx.view()) == "ChunkedBody");
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.start(loop, HttpParser::Method::HttpPUT, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::putWritableBody()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26108);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    struct ServerContext
    {
        HttpConnection* connection = nullptr;

        Buffer body;
        bool   responseSent = false;

        Result append(Span<const char> data)
        {
            GrowableBuffer<Buffer> gb(body);
            HttpStringAppend&      sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
            return Result(sb.append(data, 0));
        }

        void onData(AsyncBufferView::ID bufferID)
        {
            SC_ASSERT_RELEASE(connection != nullptr);
            AsyncReadableStream& readable = connection->request.getReadableStream();
            Span<const char>     data;
            SC_ASSERT_RELEASE(readable.getBuffersPool().getReadableData(bufferID, data));
            SC_ASSERT_RELEASE(data.sizeInBytes() <= connection->request.getBodyBytesRemaining());
            SC_ASSERT_RELEASE(append(data));
            SC_ASSERT_RELEASE(connection->request.consumeBodyBytes(data.sizeInBytes()));
            if (connection->request.getBodyBytesRemaining() == 0 and not responseSent)
            {
                responseSent       = true;
                const bool removed = readable.eventData.removeListener<ServerContext, &ServerContext::onData>(*this);
                SC_ASSERT_RELEASE(removed);
                SC_ASSERT_RELEASE(connection->response.startResponse(200));
                SC_ASSERT_RELEASE(connection->response.addHeader("Content-Length", "6"));
                SC_ASSERT_RELEASE(connection->response.sendHeaders());
                SC_ASSERT_RELEASE(connection->response.getWritableStream().write("stored"));
                SC_ASSERT_RELEASE(connection->response.end());
            }
        }

        [[nodiscard]] StringSpan view() const { return {body.toSpanConst(), false, StringEncoding::Ascii}; }
    } serverCtx;

    httpServer.onRequest = [this, &serverCtx](HttpConnection& connection)
    {
        serverCtx.connection = &connection;
        SC_TEST_EXPECT(connection.request.getBodyBytesRemaining() == 11);
        const bool added =
            connection.request.getReadableStream().eventData.addListener<ServerContext, &ServerContext::onData>(
                serverCtx);
        SC_TEST_EXPECT(added);
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;

    String url = StringEncoding::Ascii;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
        ServerContext&     serverCtx;
    } ctx = {collector, httpServer, serverCtx};

    SC_TEST_EXPECT(client.init(clientStorage));
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/writable", port));

    client.onPrepareRequest = [this](HttpAsyncClientRequest& request)
    {
        SC_TEST_EXPECT(request.setExpectedBodyLength(11));
        SC_TEST_EXPECT(request.sendHeaders());
        SC_TEST_EXPECT(request.getWritableStream().write("ChunkedBody"));
        SC_TEST_EXPECT(request.end());
    };
    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(StringView(ctx.collector.view()) == "stored");
                                 SC_TEST_EXPECT(StringView(ctx.serverCtx.view()) == "ChunkedBody");
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.start(loop, HttpParser::Method::HttpPUT, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::keepAliveAndReconnect()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection server1Connections[2];
    ServerConnection server2Connections[2];
    HttpAsyncServer  server1;
    HttpAsyncServer  server2;
    const uint16_t   port1 = report.mapPort(26103);
    const uint16_t   port2 = report.mapPort(26104);
    SC_TEST_EXPECT(server1.init(Span<ServerConnection>(server1Connections)));
    SC_TEST_EXPECT(server2.init(Span<ServerConnection>(server2Connections)));
    SC_TEST_EXPECT(server1.start(loop, "127.0.0.1", port1));
    SC_TEST_EXPECT(server2.start(loop, "127.0.0.1", port2));

    int server1Requests = 0;
    int server2Requests = 0;
    server1.onRequest   = [this, &server1Requests](HttpConnection& connection)
    {
        server1Requests++;
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", "5"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.getWritableStream().write("first"));
        SC_TEST_EXPECT(connection.response.end());
    };
    server2.onRequest = [this, &server2Requests](HttpConnection& connection)
    {
        server2Requests++;
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", "6"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.getWritableStream().write("second"));
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;
    AsyncLoopTimeout  deferredStep;

    String url1 = StringEncoding::Ascii;
    String url2 = StringEncoding::Ascii;

    int completions = 0;
    struct Context
    {
        enum class DeferredAction : uint8_t
        {
            None,
            RepeatFirstRequest,
            SecondOriginRequest,
            StopServers,
        };

        ResponseCollector& collector;

        HttpAsyncClient& client;
        HttpAsyncServer& server1;
        HttpAsyncServer& server2;

        String& url1;
        String& url2;

        int& completions;
        int& server1Requests;
        int& server2Requests;

        AsyncEventLoop&   loop;
        AsyncLoopTimeout& deferredStep;

        DeferredAction deferredAction = DeferredAction::None;

        Result scheduleDeferred(DeferredAction action)
        {
            deferredAction = action;
            deferredStep.callback.bind<Context, &Context::onDeferred>(*this);
            SC_TRY(deferredStep.start(loop, TimeMs{0}));
            return Result(true);
        }

        void onDeferred(AsyncLoopTimeout::Result&)
        {
            switch (deferredAction)
            {
            case DeferredAction::RepeatFirstRequest: SC_ASSERT_RELEASE(client.get(loop, url1.view(), true)); break;
            case DeferredAction::SecondOriginRequest: SC_ASSERT_RELEASE(client.get(loop, url2.view(), true)); break;
            case DeferredAction::StopServers:
                SC_ASSERT_RELEASE(server1.stop());
                SC_ASSERT_RELEASE(server2.stop());
                break;
            case DeferredAction::None: break;
            }
            deferredAction = DeferredAction::None;
        }
    } ctx = {collector,   client,          server1,         server2, url1,        url2,
             completions, server1Requests, server2Requests, loop,    deferredStep};

    SC_TEST_EXPECT(client.init(clientStorage));
    SC_TEST_EXPECT(StringBuilder::format(url1, "http://127.0.0.1:{}/first", port1));
    SC_TEST_EXPECT(StringBuilder::format(url2, "http://127.0.0.1:{}/second", port2));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 ctx.completions++;
                                 if (ctx.completions == 1)
                                 {
                                     SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                     SC_TEST_EXPECT(StringView(ctx.collector.view()) == "first");
                                     SC_TEST_EXPECT(ctx.scheduleDeferred(Context::DeferredAction::RepeatFirstRequest));
                                 }
                                 else if (ctx.completions == 2)
                                 {
                                     SC_TEST_EXPECT(StringView(ctx.collector.view()) == "first");
                                     SC_TEST_EXPECT(ctx.scheduleDeferred(Context::DeferredAction::SecondOriginRequest));
                                 }
                                 else
                                 {
                                     SC_TEST_EXPECT(StringView(ctx.collector.view()) == "second");
                                     SC_TEST_EXPECT(ctx.server1Requests == 2);
                                     SC_TEST_EXPECT(ctx.server2Requests == 1);
                                     SC_TEST_EXPECT(ctx.scheduleDeferred(Context::DeferredAction::StopServers));
                                 }
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{3000}));
    SC_TEST_EXPECT(client.get(loop, url1.view(), true));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(server1.close());
    SC_TEST_EXPECT(server2.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::transportSetupHookDefersRequest()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26124);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;

    String url = StringEncoding::Ascii;

    struct Context
    {
        HttpAsyncClientTest* test = nullptr;

        ResponseCollector* collector = nullptr;
        HttpAsyncServer*   server    = nullptr;

        AsyncLoopTimeout completeDelay;

        Function<void(Result)> completeTransport;

        bool setupCalled       = false;
        bool setupCompleted    = false;
        bool requestAfterSetup = false;

        Result onSetup(HttpAsyncClientTransportSetup& setup)
        {
            test->recordExpectation("transport setup connection", setup.connection != nullptr);
            test->recordExpectation("transport setup loop", setup.eventLoop != nullptr);
            test->recordExpectation("transport setup url", setup.url != nullptr and setup.url->protocol == "http");
            test->recordExpectation("transport setup native socket", setup.nativeSocket != SocketDescriptor::Invalid);
            test->recordExpectation("transport setup complete", setup.complete.isValid());

            setupCalled       = true;
            completeTransport = setup.complete;

            completeDelay.callback.bind<Context, &Context::onCompleteDelay>(*this);
            return completeDelay.start(*setup.eventLoop, TimeMs{1});
        }

        void onCompleteDelay(AsyncLoopTimeout::Result&)
        {
            setupCompleted = true;
            completeTransport(Result(true));
        }
    } ctx;

    ctx.test      = this;
    ctx.collector = &collector;
    ctx.server    = &httpServer;

    httpServer.onRequest = [this, &ctx](HttpConnection& connection)
    {
        ctx.requestAfterSetup = ctx.setupCompleted;
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", "5"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.getWritableStream().write("ready"));
        SC_TEST_EXPECT(connection.response.end());
    };

    SC_TEST_EXPECT(client.init(clientStorage));
    client.setTransportSetup({[&ctx](HttpAsyncClientTransportSetup& setup) -> Result { return ctx.onSetup(setup); }});
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/deferred", port));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector->attach(response,
                              [this, &ctx](HttpAsyncClientResponse& completedResponse)
                              {
                                  ctx.collector->detach();
                                  SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                  SC_TEST_EXPECT(StringView(ctx.collector->view()) == "ready");
                                  SC_TEST_EXPECT(ctx.setupCalled);
                                  SC_TEST_EXPECT(ctx.setupCompleted);
                                  SC_TEST_EXPECT(ctx.requestAfterSetup);
                                  SC_TEST_EXPECT(ctx.server->stop());
                              });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::httpsTransportSetupHookDispatch()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26125);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;

    String url = StringEncoding::Ascii;

    struct Context
    {
        HttpAsyncClientTest* test = nullptr;

        ResponseCollector* collector = nullptr;
        HttpAsyncServer*   server    = nullptr;

        bool setupCalled = false;

        Result onSetup(HttpAsyncClientTransportSetup& setup)
        {
            test->recordExpectation("https transport setup connection", setup.connection != nullptr);
            test->recordExpectation("https transport setup loop", setup.eventLoop != nullptr);
            test->recordExpectation("https transport setup url",
                                    setup.url != nullptr and setup.url->protocol == "https");
            test->recordExpectation("https transport setup native socket",
                                    setup.nativeSocket != SocketDescriptor::Invalid);
            test->recordExpectation("https transport setup complete", setup.complete.isValid());

            setupCalled = true;
            setup.complete(Result(true));
            return Result(true);
        }
    } ctx;

    ctx.test      = this;
    ctx.collector = &collector;
    ctx.server    = &httpServer;

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", "5"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.getWritableStream().write("https"));
        SC_TEST_EXPECT(connection.response.end());
    };

    SC_TEST_EXPECT(client.init(clientStorage));
    client.setTransportSetup({[&ctx](HttpAsyncClientTransportSetup& setup) -> Result { return ctx.onSetup(setup); }});
    SC_TEST_EXPECT(StringBuilder::format(url, "https://127.0.0.1:{}/hook", port));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector->attach(response,
                              [this, &ctx](HttpAsyncClientResponse& completedResponse)
                              {
                                  ctx.collector->detach();
                                  SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                  SC_TEST_EXPECT(StringView(ctx.collector->view()) == "https");
                                  SC_TEST_EXPECT(ctx.setupCalled);
                                  SC_TEST_EXPECT(ctx.server->stop());
                              });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::httpsTransportSetupReportsTlsBackendError()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26126);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    ClientConnection clientStorage;
    HttpAsyncClient  client;
    TimeoutGuard     timeout;

    String url = StringEncoding::Ascii;

    struct Context
    {
        HttpAsyncClientTest* test   = nullptr;
        HttpAsyncServer*     server = nullptr;

        bool setupCalled         = false;
        bool errorCalled         = false;
        bool errorMessageMatched = false;

        Result onSetup(HttpAsyncClientTransportSetup& setup)
        {
            test->recordExpectation("https tls error setup connection", setup.connection != nullptr);
            test->recordExpectation("https tls error setup loop", setup.eventLoop != nullptr);
            test->recordExpectation("https tls error setup url",
                                    setup.url != nullptr and setup.url->protocol == "https");
            test->recordExpectation("https tls error setup native socket",
                                    setup.nativeSocket != SocketDescriptor::Invalid);
            test->recordExpectation("https tls error setup complete", setup.complete.isValid());

            setupCalled = true;
            return Result::Error("HttpAsyncClient TLS backend unavailable");
        }
    } ctx;

    ctx.test   = this;
    ctx.server = &httpServer;

    httpServer.onRequest = [this](HttpConnection&) { SC_TEST_EXPECT(false); };

    SC_TEST_EXPECT(client.init(clientStorage));
    client.setTransportSetup({[&ctx](HttpAsyncClientTransportSetup& setup) -> Result { return ctx.onSetup(setup); }});
    SC_TEST_EXPECT(StringBuilder::format(url, "https://127.0.0.1:{}/tls-error", port));

    client.onResponse = [this](HttpAsyncClientResponse&) { SC_TEST_EXPECT(false); };
    client.onError    = [this, &ctx](Result result)
    {
        ctx.errorCalled         = true;
        ctx.errorMessageMatched = resultMessageEquals(result, "HttpAsyncClient TLS backend unavailable");
        SC_TEST_EXPECT(ctx.server->stop());
    };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(ctx.setupCalled);
    SC_TEST_EXPECT(ctx.errorCalled);
    SC_TEST_EXPECT(ctx.errorMessageMatched);
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(client.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::zeroLengthResponse()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26105);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", "0"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
    } ctx = {collector, httpServer};

    SC_TEST_EXPECT(client.init(clientStorage));
    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/empty", port));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(ctx.collector.view().sizeInBytes() == 0);
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::chunkedResponse()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26106);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.setChunkedTransferEncoding());
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.getWritableStream().write("hello"));
        SC_TEST_EXPECT(connection.response.getWritableStream().write(" world"));
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
    } ctx = {collector, httpServer};

    SC_TEST_EXPECT(client.init(clientStorage));
    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/chunked", port));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        SC_TEST_EXPECT(response.getBodyFramingKind() == HttpBodyFramingKind::Chunked);
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(ctx.collector.view() == "hello world");
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::chunkedResponseRejectsTrailers()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26109);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Transfer-Encoding", "chunked"));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.getWritableStream().write("5\r\nhello\r\n0\r\nX-Test: yes\r\n\r\n"));
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection clientStorage;
    HttpAsyncClient  client;
    TimeoutGuard     timeout;
    struct Context
    {
        bool             sawError;
        HttpAsyncServer& httpServer;
    } ctx = {false, httpServer};

    SC_TEST_EXPECT(client.init(clientStorage));
    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/chunked-trailer", port));

    client.onResponse = [this](HttpAsyncClientResponse&) {};
    client.onError    = [this, &ctx](Result result)
    {
        ctx.sawError = true;
        SC_TEST_EXPECT(resultMessageEquals(result, "HttpIncomingMessage non-empty trailers are not supported"));
        SC_TEST_EXPECT(ctx.httpServer.stop());
    };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(ctx.sawError);
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::closeDelimitedResponse()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26110);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        connection.response.setKeepAlive(false);
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        SC_TEST_EXPECT(connection.response.getWritableStream().write("goodbye"));
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
    } ctx = {collector, httpServer};

    SC_TEST_EXPECT(client.init(clientStorage));
    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/close-delimited", port));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        SC_TEST_EXPECT(response.getBodyFramingKind() == HttpBodyFramingKind::CloseDelimited);
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(ctx.collector.view() == "goodbye");
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::gzipResponseDecompression()
{
    static constexpr uint8_t compressedGzip[] = {0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                 0x00, 0x13, 0x2B, 0x49, 0x2D, 0x2E, 0x01, 0x00,
                                                 0x0C, 0x7E, 0x7F, 0xD8, 0x04, 0x00, 0x00, 0x00};

    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26111);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        String contentLength = StringEncoding::Ascii;
        SC_TEST_EXPECT(StringBuilder::format(contentLength, "{}", sizeof(compressedGzip)));
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Encoding", "gzip"));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", contentLength.view()));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        const Span<const char> body = Span<const uint8_t>(compressedGzip).reinterpret_as_span_of<const char>();
        SC_TEST_EXPECT(connection.response.getWritableStream().write(AsyncBufferView(body)));
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection clientStorage;
    HttpAsyncClient  client;
    SC_TEST_EXPECT(client.init(clientStorage));

    SyncZLibTransformStream      decoder;
    AsyncReadableStream::Request decoderReadQueue[4];
    AsyncWritableStream::Request decoderWriteQueue[4];
    SC_TEST_EXPECT(decoder.init(clientStorage.buffersPool, decoderReadQueue, decoderWriteQueue));
    client.setResponseDecompression(decoder);

    ResponseCollector collector;
    TimeoutGuard      timeout;
    String            url = StringEncoding::Ascii;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
    } ctx = {collector, httpServer};

    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/gzip", port));
    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(ctx.collector.view() == "test");
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::deflateResponseDecompression()
{
    static constexpr uint8_t compressedDeflate[] = {0x2B, 0x49, 0x2D, 0x2E, 0x01, 0x00};

    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26112);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    httpServer.onRequest = [this](HttpConnection& connection)
    {
        String contentLength = StringEncoding::Ascii;
        SC_TEST_EXPECT(StringBuilder::format(contentLength, "{}", sizeof(compressedDeflate)));
        SC_TEST_EXPECT(connection.response.startResponse(200));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Encoding", "deflate"));
        SC_TEST_EXPECT(connection.response.addHeader("Content-Length", contentLength.view()));
        SC_TEST_EXPECT(connection.response.sendHeaders());
        const Span<const char> body = Span<const uint8_t>(compressedDeflate).reinterpret_as_span_of<const char>();
        SC_TEST_EXPECT(connection.response.getWritableStream().write(AsyncBufferView(body)));
        SC_TEST_EXPECT(connection.response.end());
    };

    ClientConnection clientStorage;
    HttpAsyncClient  client;
    SC_TEST_EXPECT(client.init(clientStorage));

    SyncZLibTransformStream      decoder;
    AsyncReadableStream::Request decoderReadQueue[4];
    AsyncWritableStream::Request decoderWriteQueue[4];
    SC_TEST_EXPECT(decoder.init(clientStorage.buffersPool, decoderReadQueue, decoderWriteQueue));
    client.setResponseDecompression(decoder);

    ResponseCollector collector;
    TimeoutGuard      timeout;
    String            url = StringEncoding::Ascii;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
    } ctx = {collector, httpServer};

    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/deflate", port));
    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 200);
                                 SC_TEST_EXPECT(ctx.collector.view() == "test");
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.get(loop, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::gzipRequestCompression()
{
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    ServerConnection connections[1];
    HttpAsyncServer  httpServer;
    const uint16_t   port = report.mapPort(26113);
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));

    struct ServerContext
    {
        HttpAsyncClientTest* test;
        HttpConnection*      connection = nullptr;
        HttpAsyncServer*     httpServer = nullptr;
        Buffer               compressedBody;

        Result append(Span<const char> data)
        {
            GrowableBuffer<Buffer> gb(compressedBody);
            HttpStringAppend&      sb = static_cast<HttpStringAppend&>(static_cast<IGrowableBuffer&>(gb));
            return Result(sb.append(data, 0));
        }

        void onData(AsyncBufferView::ID bufferID)
        {
            Span<const char> data;
            SC_ASSERT_RELEASE(connection != nullptr);
            SC_ASSERT_RELEASE(connection->request.getReadableStream().getBuffersPool().getReadableData(bufferID, data));
            SC_ASSERT_RELEASE(append(data));
            SC_ASSERT_RELEASE(connection->request.consumeBodyBytes(data.sizeInBytes()));
        }

        void onEnd()
        {
            StringSpan contentEncoding;
            test->recordExpectation("request content encoding",
                                    connection->request.getHeader("Content-Encoding", contentEncoding) and
                                        contentEncoding == "gzip");
            test->recordExpectation("request transfer encoding",
                                    connection->request.getBodyFramingKind() == HttpBodyFramingKind::Chunked);

            Span<char> decoded;
            test->recordExpectation(
                "decompress request body",
                decompressForTest(ZLibStream::DecompressGZip, compressedBody.toSpanConst(), decoded));
            test->recordExpectation("decoded request body",
                                    StringView(decoded, false, StringEncoding::Ascii) == "compressed hello");

            test->recordExpectation("startResponse", connection->response.startResponse(200));
            test->recordExpectation("addHeader", connection->response.addHeader("Content-Length", "0"));
            test->recordExpectation("sendHeaders", connection->response.sendHeaders());
            test->recordExpectation("end response", connection->response.end());
        }
    } serverContext = {this, nullptr, &httpServer, {}};

    httpServer.onRequest = [this, &serverContext](HttpConnection& connection)
    {
        serverContext.connection = &connection;
        const bool addedData =
            connection.request.getReadableStream().eventData.addListener<ServerContext, &ServerContext::onData>(
                serverContext);
        SC_TEST_EXPECT(addedData);
        const bool addedEnd =
            connection.request.getReadableStream().eventEnd.addListener<ServerContext, &ServerContext::onEnd>(
                serverContext);
        SC_TEST_EXPECT(addedEnd);
    };

    ClientConnection clientStorage;
    HttpAsyncClient  client;
    SC_TEST_EXPECT(client.init(clientStorage));

    ChunkedBodyStream bodyStream;
    SC_TEST_EXPECT(bodyStream.init(clientStorage.buffersPool, StringSpan("compressed hello").toCharSpan(), 4));

    SyncZLibTransformStream      compressor;
    AsyncReadableStream::Request compressorReadQueue[4];
    AsyncWritableStream::Request compressorWriteQueue[4];
    SC_TEST_EXPECT(compressor.init(clientStorage.buffersPool, compressorReadQueue, compressorWriteQueue));

    TimeoutGuard timeout;
    String       url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/compressed-upload", port));

    struct ClientPrepareContext
    {
        HttpAsyncClientTest*     test;
        ChunkedBodyStream*       bodyStream;
        SyncZLibTransformStream* compressor;

        void onPrepare(HttpAsyncClientRequest& request)
        {
            test->recordExpectation("set compressed body",
                                    request.setCompressedBody(*bodyStream, *compressor, HttpContentEncoding::GZip));
            test->recordExpectation("send compressed headers", request.sendHeaders());
        }
    } prepareContext = {this, &bodyStream, &compressor};

    client.onPrepareRequest.bind<ClientPrepareContext, &ClientPrepareContext::onPrepare>(prepareContext);
    client.onResponse = [this, &httpServer](HttpAsyncClientResponse& response)
    {
        ResponseCollector* unusedCollector = nullptr;
        (void)(unusedCollector);
        SC_TEST_EXPECT(response.getParser().statusCode == 200);
        SC_TEST_EXPECT(httpServer.stop());
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.start(loop, HttpParser::Method::HttpPUT, url.view()));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

void SC::HttpAsyncClientTest::multipartUpload()
{
    StringView     webServerFolder = report.applicationRootDirectory.view();
    AsyncEventLoop loop;
    SC_TEST_EXPECT(loop.create());

    HttpAsyncFileServer::StreamQueue<2> streams[1];

    ServerConnection    connections[1];
    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    const uint16_t port = report.mapPort(26107);

    ThreadPool threadPool;
    if (loop.needsThreadPoolForFileOperations())
    {
        SC_TEST_EXPECT(threadPool.create(2));
    }
    SC_TEST_EXPECT(httpServer.init(Span<ServerConnection>(connections)));
    SC_TEST_EXPECT(httpServer.start(loop, "127.0.0.1", port));
    SC_TEST_EXPECT(fileServer.init(threadPool, loop, webServerFolder));
    struct ServerContext
    {
        HttpAsyncFileServer&                 fileServer;
        HttpAsyncFileServer::StreamQueue<2>* streams;
    } serverCtx = {fileServer, streams};

    httpServer.onRequest = [this, &serverCtx](HttpConnection& connection)
    {
        SC_TEST_EXPECT(
            serverCtx.fileServer.handleRequest(serverCtx.streams[connection.getConnectionID().getIndex()], connection));
    };

    ClientConnection  clientStorage;
    HttpAsyncClient   client;
    ResponseCollector collector;
    TimeoutGuard      timeout;
    FileSystem        fs;
    struct Context
    {
        ResponseCollector& collector;
        HttpAsyncServer&   httpServer;
        FileSystem&        fs;
    } ctx = {collector, httpServer, fs};

    SC_TEST_EXPECT(fs.init(webServerFolder));
    SC_TEST_EXPECT(client.init(clientStorage));
    String url = StringEncoding::Ascii;
    SC_TEST_EXPECT(StringBuilder::format(url, "http://127.0.0.1:{}/upload", port));
    HttpMultipartWriter writer;
    SC_TEST_EXPECT(writer.setBoundary("----SCMultipartBoundary"));
    SC_TEST_EXPECT(writer.addFile("file", "multipart-public.txt", StringSpan("MultipartContent").toCharSpan()));

    client.onResponse = [this, &ctx](HttpAsyncClientResponse& response)
    {
        ctx.collector.attach(response,
                             [this, &ctx](HttpAsyncClientResponse& completedResponse)
                             {
                                 ctx.collector.detach();
                                 SC_TEST_EXPECT(completedResponse.getParser().statusCode == 201);
                                 String content;
                                 SC_TEST_EXPECT(ctx.fs.read("multipart-public.txt", content));
                                 SC_TEST_EXPECT(content == "MultipartContent");
                                 SC_TEST_EXPECT(ctx.fs.removeFile("multipart-public.txt"));
                                 SC_TEST_EXPECT(ctx.httpServer.stop());
                             });
    };
    client.onError = [this](Result result) { SC_TEST_EXPECT(result); };

    SC_TEST_EXPECT(timeout.start(loop, TimeMs{2000}));
    SC_TEST_EXPECT(client.postMultipart(loop, url.view(), writer));
    SC_TEST_EXPECT(loop.run());
    SC_TEST_EXPECT(fileServer.close());
    SC_TEST_EXPECT(httpServer.close());
    SC_TEST_EXPECT(loop.close());
}

namespace SC
{
void runHttpAsyncClientTest(SC::TestReport& report) { HttpAsyncClientTest test(report); }
} // namespace SC
