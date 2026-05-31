// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/HttpClient/HttpClient.h"
#include "Libraries/AsyncStreams/AsyncStreams.h"
#include "Libraries/Foundation/Deferred.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/HttpClient/HttpClientAsync.h"
#include "Libraries/HttpClient/HttpClientScheduler.h"
#include "Libraries/HttpClient/HttpClientSession.h"
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
#if SC_COMPILER_FILC
        if (not report.quietMode)
        {
            report.console.printLine("HttpClientTest - Skipping under Fil-C: Linux backend depends on host libcurl "
                                     "ABI");
        }
#else
        if (test_section("init and close"))
        {
            initAndClose();
        }
        if (test_section("capabilities"))
        {
            capabilities();
        }
        if (test_section("request policy names"))
        {
            requestPolicyNames();
        }
        if (test_section("backend policy preflight"))
        {
            backendPolicyPreflight();
        }
        if (test_section("operation memory validation"))
        {
            operationMemoryValidation();
        }
        if (test_section("blocking GET local"))
        {
            blockingGet();
        }
        if (test_section("blocking response buffer overflow"))
        {
            blockingResponseBufferOverflow();
        }
        if (test_section("blocking response header buffer overflow"))
        {
            blockingResponseHeaderBufferOverflow();
        }
        if (test_section("blocking POST local"))
        {
            blockingPost();
        }
        if (test_section("custom headers"))
        {
            blockingCustomHeaders();
        }
        if (test_section("request header validation"))
        {
            requestHeaderValidation();
        }
        if (test_section("response header helpers"))
        {
            responseHeaderHelpers();
        }
        if (test_section("content coding policy"))
        {
            contentCodingPolicy();
        }
        if (test_section("session layer"))
        {
            sessionLayer();
        }
        if (test_section("timeout"))
        {
            blockingTimeout();
        }
        if (test_section("redirect policy"))
        {
            redirectPolicy();
        }
        if (test_section("protocol preference"))
        {
            protocolPreference();
        }
        if (test_section("proxy options"))
        {
            proxyOptions();
        }
        if (test_section("cancel"))
        {
            cancelRequest();
        }
        if (test_section("method coverage"))
        {
            methodCoverage();
        }
        if (test_section("streamed body size validation"))
        {
            streamedBodySizeValidation();
        }
        if (test_section("request body framing validation"))
        {
            requestBodyFramingValidation();
        }
        if (test_section("chunked upload local"))
        {
            chunkedUpload();
        }
        if (test_section("poll GET local"))
        {
            pollGet();
        }
        if (test_section("poll concurrent GETs"))
        {
            pollConcurrentGets();
        }
        if (test_section("operation scheduler"))
        {
            operationScheduler();
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
#endif
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
        char responseMetadata[4096];
        char backendScratch[ScratchBytes];

        CoreOperationMemory()
        {
            memory.responseBuffers      = {responseBuffers, NumResponseBuffers};
            memory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
            memory.eventQueue           = {eventQueue, EventQueueSize};
            memory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
            memory.responseMetadata     = {responseMetadata, sizeof(responseMetadata)};
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
        char responseMetadata[4096];
        char backendScratch[ScratchBytes];

        AsyncOperationMemory()
        {
            coreMemory.responseBuffers      = {coreResponseBuffers, NumResponseBuffers};
            coreMemory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
            coreMemory.eventQueue           = {eventQueue, EventQueueSize};
            coreMemory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
            coreMemory.responseMetadata     = {responseMetadata, sizeof(responseMetadata)};
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

        const HttpClientCapabilities    capabilities       = HttpClient::getCapabilities();
        HttpClientCapabilities::Feature requiredFeatures[] = {HttpClientCapabilities::MultipleOperationsPerClient,
                                                              HttpClientCapabilities::FixedRequestBody};
        {
            HttpClient requiredClient;
            SC_TEST_EXPECT(requiredClient.init(requiredFeatures));
            SC_TEST_EXPECT(requiredClient.close());
        }
        {
            HttpClient requiredClient;
            SC_TEST_EXPECT(requiredClient.init(capabilities.backend, requiredFeatures));
            SC_TEST_EXPECT(requiredClient.close());
        }
        {
            HttpClient                      rejectedClient;
            HttpClientCapabilities::Feature unsupportedFeatures[] = {HttpClientCapabilities::ContentCodingPolicy};
            SC_TEST_EXPECT(not rejectedClient.init(unsupportedFeatures));
            SC_TEST_EXPECT(not rejectedClient.isInitialized());
            SC_TEST_EXPECT(rejectedClient.close());
        }
        {
            HttpClient rejectedClient;
            SC_TEST_EXPECT(not rejectedClient.init(static_cast<HttpClientCapabilities::Backend>(0xFF)));
            SC_TEST_EXPECT(not rejectedClient.isInitialized());
            SC_TEST_EXPECT(rejectedClient.close());
        }
    }

    void capabilities()
    {
        const HttpClientCapabilities capabilities = HttpClient::getCapabilities();

#if SC_PLATFORM_APPLE
        SC_TEST_EXPECT(capabilities.backend == HttpClientCapabilities::AppleURLSession);
        SC_TEST_EXPECT(capabilities.hasBackend(HttpClientCapabilities::AppleURLSession));
        SC_TEST_EXPECT(capabilities.requireBackend(HttpClientCapabilities::AppleURLSession));
        SC_TEST_EXPECT(strcmp(capabilities.getBackendName(), "apple-url-session") == 0);
        SC_TEST_EXPECT(strcmp(HttpClientCapabilities::getBackendName(capabilities.backend), "apple-url-session") == 0);
#elif SC_PLATFORM_LINUX
        SC_TEST_EXPECT(capabilities.backend == HttpClientCapabilities::LibCurl);
        SC_TEST_EXPECT(capabilities.hasBackend(HttpClientCapabilities::LibCurl));
        SC_TEST_EXPECT(capabilities.requireBackend(HttpClientCapabilities::LibCurl));
        SC_TEST_EXPECT(strcmp(capabilities.getBackendName(), "libcurl") == 0);
        SC_TEST_EXPECT(strcmp(HttpClientCapabilities::getBackendName(capabilities.backend), "libcurl") == 0);
#elif SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(capabilities.backend == HttpClientCapabilities::WinHttp);
        SC_TEST_EXPECT(capabilities.hasBackend(HttpClientCapabilities::WinHttp));
        SC_TEST_EXPECT(capabilities.requireBackend(HttpClientCapabilities::WinHttp));
        SC_TEST_EXPECT(strcmp(capabilities.getBackendName(), "winhttp") == 0);
        SC_TEST_EXPECT(strcmp(HttpClientCapabilities::getBackendName(capabilities.backend), "winhttp") == 0);
#else
        SC_TEST_EXPECT(capabilities.backend == HttpClientCapabilities::Unsupported);
        SC_TEST_EXPECT(capabilities.hasBackend(HttpClientCapabilities::Unsupported));
        SC_TEST_EXPECT(capabilities.requireBackend(HttpClientCapabilities::Unsupported));
        SC_TEST_EXPECT(strcmp(capabilities.getBackendName(), "unsupported") == 0);
        SC_TEST_EXPECT(strcmp(HttpClientCapabilities::getBackendName(capabilities.backend), "unsupported") == 0);
#endif
        SC_TEST_EXPECT(not capabilities.hasBackend(static_cast<HttpClientCapabilities::Backend>(0xFF)));
        SC_TEST_EXPECT(not capabilities.requireBackend(static_cast<HttpClientCapabilities::Backend>(0xFF)));

        SC_TEST_EXPECT(
            strcmp(HttpClientCapabilities::getFeatureName(HttpClientCapabilities::MultipleOperationsPerClient),
                   "multiple-operations-per-client") == 0);
        SC_TEST_EXPECT(
            strcmp(HttpClientCapabilities::getFeatureName(HttpClientCapabilities::ProxyHttp), "proxy-http") == 0);
        SC_TEST_EXPECT(strcmp(HttpClientCapabilities::getFeatureName(HttpClientCapabilities::ContentCodingPolicy),
                              "content-coding-policy") == 0);
        SC_TEST_EXPECT(
            strcmp(HttpClientCapabilities::getFeatureName(static_cast<HttpClientCapabilities::Feature>(0xFF)),
                   "unknown") == 0);

        if (capabilities.backend == HttpClientCapabilities::Unsupported)
        {
            SC_TEST_EXPECT(not capabilities.multipleOperationsPerClient);
            SC_TEST_EXPECT(not capabilities.fixedRequestBody);
            SC_TEST_EXPECT(not capabilities.sizedStreamRequestBody);
            SC_TEST_EXPECT(not capabilities.chunkedStreamRequestBody);
            SC_TEST_EXPECT(not capabilities.redirectPolicy);
            SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::MultipleOperationsPerClient));
            SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::ContentCodingPolicy));
            return;
        }

        SC_TEST_EXPECT(capabilities.multipleOperationsPerClient);
        SC_TEST_EXPECT(capabilities.fixedRequestBody);
        SC_TEST_EXPECT(capabilities.sizedStreamRequestBody);
        SC_TEST_EXPECT(capabilities.chunkedStreamRequestBody);
        SC_TEST_EXPECT(capabilities.redirectPolicy);
        SC_TEST_EXPECT(capabilities.protocolHttp2Preferred);
        SC_TEST_EXPECT(not capabilities.contentCodingPolicy);
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::MultipleOperationsPerClient));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::FixedRequestBody));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::SizedStreamRequestBody));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ChunkedStreamRequestBody));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::RedirectPolicy));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProtocolHttp2Preferred));
        SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::ContentCodingPolicy));
        HttpClientCapabilities::Feature commonRequired[] = {HttpClientCapabilities::MultipleOperationsPerClient,
                                                            HttpClientCapabilities::FixedRequestBody,
                                                            HttpClientCapabilities::RedirectPolicy};
        SC_TEST_EXPECT(capabilities.supportsAll(commonRequired));
        SC_TEST_EXPECT(capabilities.requireFeatures(commonRequired));
        HttpClientCapabilities::Feature unsupportedRequired[] = {HttpClientCapabilities::ContentCodingPolicy};
        SC_TEST_EXPECT(not capabilities.supportsAll(unsupportedRequired));
        SC_TEST_EXPECT(not capabilities.requireFeatures(unsupportedRequired));

        {
            HttpClientRequestOptions options;
            SC_TEST_EXPECT(capabilities.supportsRequestOptions(options));
            SC_TEST_EXPECT(capabilities.requireRequestOptions(options));
        }
        {
            HttpClientRequestOptions options;
            options.redirect.mode = HttpClientRequestRedirectOptions::FollowAll;
            SC_TEST_EXPECT(capabilities.supportsRequestOptions(options) == capabilities.redirectPolicy);
            SC_TEST_EXPECT(static_cast<bool>(capabilities.requireRequestOptions(options)) ==
                           capabilities.redirectPolicy);
        }
        {
            HttpClientRequestOptions options;
            options.protocol.preference = HttpClientRequestProtocolOptions::Http2Required;
            SC_TEST_EXPECT(capabilities.supportsRequestOptions(options) == capabilities.protocolHttp2Required);
            SC_TEST_EXPECT(static_cast<bool>(capabilities.requireRequestOptions(options)) ==
                           capabilities.protocolHttp2Required);
        }
        {
            HttpClientRequestOptions options;
            options.tls.verifyPeer = false;
            SC_TEST_EXPECT(capabilities.supportsRequestOptions(options) == capabilities.tlsDisablePeerVerification);
            SC_TEST_EXPECT(static_cast<bool>(capabilities.requireRequestOptions(options)) ==
                           capabilities.tlsDisablePeerVerification);
        }
        {
            HttpClientRequestOptions options;
            options.protocol.preference = static_cast<HttpClientRequestProtocolOptions::Preference>(0xFF);
            SC_TEST_EXPECT(not capabilities.supportsRequestOptions(options));
            SC_TEST_EXPECT(not capabilities.requireRequestOptions(options));
        }
        {
            HttpClientRequestOptions options;
            options.proxy.mode = static_cast<HttpClientRequestProxyOptions::Mode>(0xFF);
            SC_TEST_EXPECT(not capabilities.supportsRequestOptions(options));
            SC_TEST_EXPECT(not capabilities.requireRequestOptions(options));
        }

#if SC_PLATFORM_APPLE
        SC_TEST_EXPECT(not capabilities.protocolHttp11Only);
        SC_TEST_EXPECT(not capabilities.protocolHttp2Required);
        SC_TEST_EXPECT(not capabilities.tlsDisablePeerVerification);
        SC_TEST_EXPECT(not capabilities.tlsCustomCaPath);
        SC_TEST_EXPECT(not capabilities.proxyNoProxy);
        SC_TEST_EXPECT(not capabilities.proxyHttp);
        SC_TEST_EXPECT(not capabilities.proxyAuthorization);
        SC_TEST_EXPECT(not capabilities.proxyBypassList);
        SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::ProtocolHttp11Only));
        SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::ProtocolHttp2Required));
        SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::ProxyHttp));
        SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::ProxyAuthorization));
        SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::ProxyBypassList));
#elif SC_PLATFORM_LINUX
        SC_TEST_EXPECT(capabilities.protocolHttp11Only);
        SC_TEST_EXPECT(capabilities.protocolHttp2Required);
        SC_TEST_EXPECT(capabilities.tlsDisablePeerVerification);
        SC_TEST_EXPECT(capabilities.tlsCustomCaPath);
        SC_TEST_EXPECT(capabilities.proxyNoProxy);
        SC_TEST_EXPECT(capabilities.proxyHttp);
        SC_TEST_EXPECT(capabilities.proxyAuthorization);
        SC_TEST_EXPECT(capabilities.proxyBypassList);
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProtocolHttp11Only));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProtocolHttp2Required));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::TlsCustomCaPath));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProxyHttp));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProxyAuthorization));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProxyBypassList));
#elif SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(capabilities.protocolHttp11Only);
        SC_TEST_EXPECT(capabilities.protocolHttp2Required);
        SC_TEST_EXPECT(capabilities.tlsDisablePeerVerification);
        SC_TEST_EXPECT(not capabilities.tlsCustomCaPath);
        SC_TEST_EXPECT(capabilities.proxyNoProxy);
        SC_TEST_EXPECT(capabilities.proxyHttp);
        SC_TEST_EXPECT(capabilities.proxyAuthorization);
        SC_TEST_EXPECT(capabilities.proxyBypassList);
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProtocolHttp11Only));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProtocolHttp2Required));
        SC_TEST_EXPECT(not capabilities.supports(HttpClientCapabilities::TlsCustomCaPath));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProxyHttp));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProxyAuthorization));
        SC_TEST_EXPECT(capabilities.supports(HttpClientCapabilities::ProxyBypassList));
#endif
    }

    void requestPolicyNames()
    {
        {
            HttpClientRequest request;
            request.method = HttpClientRequest::HttpPOST;
            SC_TEST_EXPECT(strcmp(request.getMethodName(), "POST") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequest::getMethodName(HttpClientRequest::HttpGET), "GET") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequest::getMethodName(HttpClientRequest::HttpPUT), "PUT") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequest::getMethodName(HttpClientRequest::HttpHEAD), "HEAD") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequest::getMethodName(HttpClientRequest::HttpDELETE), "DELETE") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequest::getMethodName(HttpClientRequest::HttpPATCH), "PATCH") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequest::getMethodName(HttpClientRequest::HttpOPTIONS), "OPTIONS") == 0);
            SC_TEST_EXPECT(
                strcmp(HttpClientRequest::getMethodName(static_cast<HttpClientRequest::Method>(0xFF)), "UNKNOWN") == 0);
        }
        {
            HttpClientRequestBody body;
            body.framing = HttpClientRequestBody::ChunkedStream;
            SC_TEST_EXPECT(strcmp(body.getFramingName(), "chunked-stream") == 0);
            SC_TEST_EXPECT(
                strcmp(HttpClientRequestBody::getFramingName(HttpClientRequestBody::FixedSize), "fixed-size") == 0);
            SC_TEST_EXPECT(
                strcmp(HttpClientRequestBody::getFramingName(HttpClientRequestBody::SizedStream), "sized-stream") == 0);
            SC_TEST_EXPECT(
                strcmp(HttpClientRequestBody::getFramingName(static_cast<HttpClientRequestBody::Framing>(0xFF)),
                       "unknown") == 0);
        }
        {
            HttpClientRequestRedirectOptions redirect;
            redirect.mode = HttpClientRequestRedirectOptions::FollowAll;
            SC_TEST_EXPECT(strcmp(redirect.getModeName(), "follow-all") == 0);
            SC_TEST_EXPECT(
                strcmp(HttpClientRequestRedirectOptions::getModeName(HttpClientRequestRedirectOptions::NoRedirects),
                       "no-redirects") == 0);
            SC_TEST_EXPECT(
                strcmp(HttpClientRequestRedirectOptions::getModeName(HttpClientRequestRedirectOptions::FollowGetHead),
                       "follow-get-head") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequestRedirectOptions::getModeName(
                                      static_cast<HttpClientRequestRedirectOptions::Mode>(0xFF)),
                                  "unknown") == 0);
        }
        {
            HttpClientRequestProtocolOptions protocol;
            protocol.preference = HttpClientRequestProtocolOptions::Http2Preferred;
            SC_TEST_EXPECT(strcmp(protocol.getPreferenceName(), "h2-preferred") == 0);
            SC_TEST_EXPECT(
                strcmp(HttpClientRequestProtocolOptions::getPreferenceName(HttpClientRequestProtocolOptions::Default),
                       "default") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequestProtocolOptions::getPreferenceName(
                                      HttpClientRequestProtocolOptions::Http11Only),
                                  "http/1.1-only") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequestProtocolOptions::getPreferenceName(
                                      HttpClientRequestProtocolOptions::Http2Required),
                                  "h2-required") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequestProtocolOptions::getPreferenceName(
                                      static_cast<HttpClientRequestProtocolOptions::Preference>(0xFF)),
                                  "unknown") == 0);
        }
        {
            HttpClientRequestProxyOptions proxy;
            proxy.mode = HttpClientRequestProxyOptions::NoProxy;
            SC_TEST_EXPECT(strcmp(proxy.getModeName(), "no-proxy") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequestProxyOptions::getModeName(HttpClientRequestProxyOptions::Default),
                                  "default") == 0);
            SC_TEST_EXPECT(
                strcmp(HttpClientRequestProxyOptions::getModeName(HttpClientRequestProxyOptions::Http), "http") == 0);
            SC_TEST_EXPECT(strcmp(HttpClientRequestProxyOptions::getModeName(
                                      static_cast<HttpClientRequestProxyOptions::Mode>(0xFF)),
                                  "unknown") == 0);
        }
    }

    void backendPolicyPreflight()
    {
        const HttpClientCapabilities capabilities = HttpClient::getCapabilities();

        HttpClient client;
        SC_TEST_EXPECT(client.init());

        CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;
        HttpClientOperation                                    operation;
        SC_TEST_EXPECT(operation.init(client, memory.memory));

        auto expectRejected = [&](HttpClientRequest& request)
        {
            HttpClientResponse response;
            request.url                               = "https://127.0.0.1:1/preflight"_a8;
            request.options.timeouts.requestTimeoutMs = 1;
            SC_TEST_EXPECT(not operation.start(request, response));
            SC_TEST_EXPECT(not operation.isRequestInFlight());
        };

        if (not capabilities.protocolHttp11Only)
        {
            HttpClientRequest request;
            request.options.protocol.preference = HttpClientRequestProtocolOptions::Http11Only;
            expectRejected(request);
        }
        if (not capabilities.protocolHttp2Required)
        {
            HttpClientRequest request;
            request.options.protocol.preference = HttpClientRequestProtocolOptions::Http2Required;
            expectRejected(request);
        }
        if (not capabilities.tlsDisablePeerVerification)
        {
            HttpClientRequest request;
            request.options.tls.verifyPeer = false;
            expectRejected(request);
        }
        if (not capabilities.tlsCustomCaPath)
        {
            HttpClientRequest request;
            request.options.tls.caCertificatesPath = "/tmp/sc-http-client-test-ca.pem"_a8;
            expectRejected(request);
        }
        if (not capabilities.proxyNoProxy)
        {
            HttpClientRequest request;
            request.options.proxy.mode = HttpClientRequestProxyOptions::NoProxy;
            expectRejected(request);
        }
        if (not capabilities.proxyHttp)
        {
            HttpClientRequest request;
            request.options.proxy.mode = HttpClientRequestProxyOptions::Http;
            request.options.proxy.url  = "http://127.0.0.1:1"_a8;
            expectRejected(request);
        }

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(client.close());
    }

    void operationMemoryValidation()
    {
        HttpClient client;
        SC_TEST_EXPECT(client.init());

        HttpClientResponseBuffer responseBuffers[2] = {};
        HttpClientOperationEvent eventQueue[2]      = {};
        char                     responseHeaders[64];
        char                     responseMetadata[64];
        char                     tinyResponseMetadata[4];
        char                     responseMemory[2];
        char                     singleResponseBuffer[1];

        {
            HttpClientOperationMemory memory;
            HttpClientOperation       operation;
            SC_TEST_EXPECT(not operation.init(client, memory));
            SC_TEST_EXPECT(not operation.isInitialized());
        }
        {
            HttpClientOperationMemory memory;
            memory.responseBuffers  = {responseBuffers, 2};
            memory.eventQueue       = {eventQueue, 2};
            memory.responseHeaders  = {responseHeaders, sizeof(responseHeaders)};
            memory.responseMetadata = {responseMetadata, sizeof(responseMetadata)};

            HttpClientOperation operation;
            SC_TEST_EXPECT(not operation.init(client, memory));
            SC_TEST_EXPECT(not operation.isInitialized());
        }
        {
            HttpClientOperationMemory memory;
            memory.responseBuffers      = {responseBuffers, 2};
            memory.responseBufferMemory = {singleResponseBuffer, sizeof(singleResponseBuffer)};
            memory.eventQueue           = {eventQueue, 2};
            memory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
            memory.responseMetadata     = {responseMetadata, sizeof(responseMetadata)};

            HttpClientOperation operation;
            SC_TEST_EXPECT(not operation.init(client, memory));
            SC_TEST_EXPECT(not operation.isInitialized());
        }
        {
            HttpClientOperationMemory memory;
            memory.responseBuffers      = {responseBuffers, 2};
            memory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
            memory.eventQueue           = {eventQueue, 2};
            memory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
            memory.responseMetadata     = {responseMetadata, sizeof(responseMetadata)};

            HttpClientOperation operation;
            SC_TEST_EXPECT(operation.init(client, memory));
            SC_TEST_EXPECT(operation.isInitialized());
            SC_TEST_EXPECT(operation.close());
        }
        {
            HttpClientOperationMemory memory;
            memory.responseBuffers      = {responseBuffers, 2};
            memory.responseBufferMemory = {responseMemory, sizeof(responseMemory)};
            memory.eventQueue           = {eventQueue, 2};
            memory.responseHeaders      = {responseHeaders, sizeof(responseHeaders)};
            memory.responseMetadata     = {tinyResponseMetadata, sizeof(tinyResponseMetadata)};

            HttpClientRequest  request;
            HttpClientResponse response;
            request.url = "http://127.0.0.1/metadata-overflow"_a8;

            HttpClientOperation operation;
            SC_TEST_EXPECT(operation.init(client, memory));
            SC_TEST_EXPECT(not operation.start(request, response, nullptr));
            SC_TEST_EXPECT(not operation.isRequestInFlight());
            SC_TEST_EXPECT(operation.close());
        }

        SC_TEST_EXPECT(client.close());
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

    void blockingResponseBufferOverflow()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
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
                char               body[4]    = {};
                size_t             bodyLength = 0;

                request.url = server.endpoint.view();
                SC_TEST_EXPECT(not HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               memory.memory));
                SC_TEST_EXPECT(bodyLength == sizeof(body));
                SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "Hell");
                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void blockingResponseHeaderBufferOverflow()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("X-Large-Header"_a8,
                                                     "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"_a8));
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

                CoreOperationMemory<64 * 1024, 8, 16, 24, 16 * 1024> memory;

                HttpClientRequest  request;
                HttpClientResponse response;
                char               body[16]   = {};
                size_t             bodyLength = 0;

                request.url = server.endpoint.view();
                SC_TEST_EXPECT(not HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               memory.memory));
                SC_TEST_EXPECT(response.headersLength <= response.headers.sizeInBytes());
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

                request.url        = server.endpoint.view();
                request.method     = HttpClientRequest::HttpPOST;
                request.body.bytes = {"HelloBody", 9};

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
            StringView emptyHeader;
            SC_TEST_EXPECT(client.request.getHeader("X-Empty-Request"_a8, emptyHeader));
            SC_TEST_EXPECT(emptyHeader.sizeInBytes() == 0);

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

                HttpClientHeader  headers[] = {{"X-Test"_a8, "HeaderValue"_a8}, {"X-Empty-Request"_a8, ""_a8}};
                HttpClientRequest request;
                request.url     = server.endpoint.view();
                request.headers = headers;

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

    void requestHeaderValidation()
    {
        HttpClient client;
        SC_TEST_EXPECT(client.init());

        CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;
        HttpClientOperation                                    operation;
        SC_TEST_EXPECT(operation.init(client, memory.memory));

        {
            HttpClientRequest request;
            request.url = "http://127.0.0.1:1/valid-request"_a8;
            SC_TEST_EXPECT(request.validate());
        }
        {
            HttpClientRequest request;
            request.url = "HTTPS://example.test/valid-request"_a8;
            SC_TEST_EXPECT(request.validate());
        }

        auto expectUrlRejected = [&](StringSpan url)
        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url = url;
            SC_TEST_EXPECT(not request.validate());
            SC_TEST_EXPECT(not operation.start(request, response));
            SC_TEST_EXPECT(not operation.isRequestInFlight());
        };

        {
            expectUrlRejected("ftp://127.0.0.1:1/invalid-url"_a8);
        }
        {
            expectUrlRejected("http://"_a8);
        }
        {
            expectUrlRejected("https:///missing-host"_a8);
        }
        {
            expectUrlRejected("http://127.0.0.1:1/bad url"_a8);
        }
        {
            static const char BadUrl[] = {'h', 't', 't', 'p', ':', '/', '/', 'b', 'a', 'd', '\0', 'h', 'o', 's', 't'};
            expectUrlRejected({{BadUrl, sizeof(BadUrl)}, false, StringEncoding::Ascii});
        }

        auto expectRejected = [&](HttpClientHeader& header)
        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url     = "http://127.0.0.1:1/invalid-header"_a8;
            request.headers = {&header, 1};
            SC_TEST_EXPECT(not request.validate());
            SC_TEST_EXPECT(not operation.start(request, response));
        };

        {
            HttpClientHeader header = {""_a8, "value"_a8};
            expectRejected(header);
        }
        {
            HttpClientHeader header = {"Bad Header"_a8, "value"_a8};
            expectRejected(header);
        }
        {
            HttpClientHeader header = {"Bad:Header"_a8, "value"_a8};
            expectRejected(header);
        }
        {
            HttpClientHeader header = {"Bad\rHeader"_a8, "value"_a8};
            expectRejected(header);
        }
        {
            HttpClientHeader header = {"X-Test"_a8, "bad\r\nvalue"_a8};
            expectRejected(header);
        }
        {
            static const char BadValue[] = {'b', 'a', 'd', '\0', 'v', 'a', 'l', 'u', 'e'};
            HttpClientHeader  header     = {"X-Test"_a8, {{BadValue, sizeof(BadValue)}, false, StringEncoding::Ascii}};
            expectRejected(header);
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url    = "http://127.0.0.1:1/invalid-method"_a8;
            request.method = static_cast<HttpClientRequest::Method>(0xFF);
            SC_TEST_EXPECT(not request.validate());
            SC_TEST_EXPECT(not operation.start(request, response));
        }

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(client.close());
    }

    void responseHeaderHelpers()
    {
        static const char RawHeaders[] = "HTTP/1.1 200 OK\r\n"
                                         "Content-Type: text/plain\r\n"
                                         "Set-Cookie: a=1\r\n"
                                         "set-cookie: b=2\r\n"
                                         "Malformed-Header\r\n"
                                         ": missing-name\r\n"
                                         "X-Empty:\r\n"
                                         "Location: /next\r\n"
                                         "Transfer-Encoding: gzip, chunked\r\n"
                                         "Content-Length: 42\r\n"
                                         "Content-Encoding: gzip, br\r\n"
                                         "WWW-Authenticate: Basic realm=\"origin\"\r\n"
                                         "Proxy-Authenticate: Basic realm=\"proxy\"\r\n"
                                         "\r\n";

        HttpClientResponse response;
        response.headers            = {RawHeaders, sizeof(RawHeaders) - 1};
        response.headersLength      = sizeof(RawHeaders) - 1;
        response.statusCode         = 200;
        response.negotiatedProtocol = HttpClientResponse::Protocol::Http2;
        SC_TEST_EXPECT(not response.isInformationalStatus());
        SC_TEST_EXPECT(response.isSuccessfulStatus());
        SC_TEST_EXPECT(not response.isRedirectStatus());
        SC_TEST_EXPECT(not response.isClientErrorStatus());
        SC_TEST_EXPECT(not response.isServerErrorStatus());
        SC_TEST_EXPECT(not response.isErrorStatus());
        response.statusCode = 101;
        SC_TEST_EXPECT(response.isInformationalStatus());
        response.statusCode = 302;
        SC_TEST_EXPECT(response.isRedirectStatus());
        response.statusCode = 404;
        SC_TEST_EXPECT(response.isClientErrorStatus());
        SC_TEST_EXPECT(response.isErrorStatus());
        response.statusCode = 503;
        SC_TEST_EXPECT(response.isServerErrorStatus());
        SC_TEST_EXPECT(response.isErrorStatus());
        response.statusCode = 200;
        SC_TEST_EXPECT(strcmp(response.getProtocolName(), "h2") == 0);
        SC_TEST_EXPECT(strcmp(HttpClientResponse::getProtocolName(HttpClientResponse::Protocol::Http11), "http/1.1") ==
                       0);
        SC_TEST_EXPECT(strcmp(HttpClientResponse::getProtocolName(HttpClientResponse::Protocol::Unknown), "unknown") ==
                       0);
        SC_TEST_EXPECT(strcmp(HttpClientResponse::getProtocolName(static_cast<HttpClientResponse::Protocol>(0xFF)),
                              "unknown") == 0);

        HttpClientResponseHeaderIterator iterator;
        HttpClientHeader                 header;

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "Content-Type"_a8);
        SC_TEST_EXPECT(header.value == "text/plain"_a8);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "Set-Cookie"_a8);
        SC_TEST_EXPECT(header.value == "a=1"_a8);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "set-cookie"_a8);
        SC_TEST_EXPECT(header.value == "b=2"_a8);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "X-Empty"_a8);
        SC_TEST_EXPECT(header.value.sizeInBytes() == 0);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "Location"_a8);
        SC_TEST_EXPECT(header.value == "/next"_a8);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "Transfer-Encoding"_a8);
        SC_TEST_EXPECT(header.value == "gzip, chunked"_a8);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "Content-Length"_a8);
        SC_TEST_EXPECT(header.value == "42"_a8);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "Content-Encoding"_a8);
        SC_TEST_EXPECT(header.value == "gzip, br"_a8);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "WWW-Authenticate"_a8);
        SC_TEST_EXPECT(header.value == "Basic realm=\"origin\""_a8);

        SC_TEST_EXPECT(response.getNextHeader(iterator, header));
        SC_TEST_EXPECT(header.name == "Proxy-Authenticate"_a8);
        SC_TEST_EXPECT(header.value == "Basic realm=\"proxy\""_a8);

        SC_TEST_EXPECT(not response.getNextHeader(iterator, header));

        HttpClientResponseHeaderIterator cookieIterator;
        StringSpan                       value;
        SC_TEST_EXPECT(response.findNextHeader("set-cookie"_a8, cookieIterator, value));
        SC_TEST_EXPECT(value == "a=1"_a8);
        SC_TEST_EXPECT(response.findNextHeader("Set-Cookie"_a8, cookieIterator, value));
        SC_TEST_EXPECT(value == "b=2"_a8);
        SC_TEST_EXPECT(not response.findNextHeader("Set-Cookie"_a8, cookieIterator, value));

        SC_TEST_EXPECT(response.getHeader("CONTENT-TYPE"_a8, value));
        SC_TEST_EXPECT(value == "text/plain"_a8);
        SC_TEST_EXPECT(response.hasHeader("content-type"_a8));
        SC_TEST_EXPECT(response.hasHeader("x-empty"_a8));
        SC_TEST_EXPECT(not response.hasHeader("missing"_a8));
        SC_TEST_EXPECT(response.getContentType(value));
        SC_TEST_EXPECT(value == "text/plain"_a8);
        SC_TEST_EXPECT(response.getContentEncoding(value));
        SC_TEST_EXPECT(value == "gzip, br"_a8);
        SC_TEST_EXPECT(response.getTransferEncoding(value));
        SC_TEST_EXPECT(value == "gzip, chunked"_a8);
        SC_TEST_EXPECT(response.getLocation(value));
        SC_TEST_EXPECT(value == "/next"_a8);
        SC_TEST_EXPECT(response.getWwwAuthenticate(value));
        SC_TEST_EXPECT(value == "Basic realm=\"origin\""_a8);
        SC_TEST_EXPECT(response.getProxyAuthenticate(value));
        SC_TEST_EXPECT(value == "Basic realm=\"proxy\""_a8);

        HttpClientContentCodingIterator contentCodingIterator;
        HttpClientContentCoding         contentCoding;
        SC_TEST_EXPECT(response.getNextContentCoding(contentCodingIterator, contentCoding));
        SC_TEST_EXPECT(contentCoding.name == "gzip"_a8);
        SC_TEST_EXPECT(contentCoding.type == HttpClientContentCoding::GZip);
        SC_TEST_EXPECT(strcmp(HttpClientContentCoding::getName(contentCoding.type), "gzip") == 0);
        SC_TEST_EXPECT(response.getNextContentCoding(contentCodingIterator, contentCoding));
        SC_TEST_EXPECT(contentCoding.name == "br"_a8);
        SC_TEST_EXPECT(contentCoding.type == HttpClientContentCoding::Brotli);
        SC_TEST_EXPECT(strcmp(HttpClientContentCoding::getName(contentCoding.type), "br") == 0);
        SC_TEST_EXPECT(not response.getNextContentCoding(contentCodingIterator, contentCoding));
        SC_TEST_EXPECT(response.hasContentCoding(HttpClientContentCoding::GZip));
        SC_TEST_EXPECT(response.hasContentCoding(HttpClientContentCoding::Brotli));
        SC_TEST_EXPECT(not response.hasContentCoding(HttpClientContentCoding::Deflate));

        HttpClientTransferCodingIterator transferCodingIterator;
        HttpClientTransferCoding         transferCoding;
        SC_TEST_EXPECT(response.getNextTransferCoding(transferCodingIterator, transferCoding));
        SC_TEST_EXPECT(transferCoding.name == "gzip"_a8);
        SC_TEST_EXPECT(transferCoding.type == HttpClientTransferCoding::GZip);
        SC_TEST_EXPECT(strcmp(HttpClientTransferCoding::getName(transferCoding.type), "gzip") == 0);
        SC_TEST_EXPECT(response.getNextTransferCoding(transferCodingIterator, transferCoding));
        SC_TEST_EXPECT(transferCoding.name == "chunked"_a8);
        SC_TEST_EXPECT(transferCoding.isChunked());
        SC_TEST_EXPECT(strcmp(HttpClientTransferCoding::getName(transferCoding.type), "chunked") == 0);
        SC_TEST_EXPECT(not response.getNextTransferCoding(transferCodingIterator, transferCoding));
        SC_TEST_EXPECT(response.hasTransferCoding(HttpClientTransferCoding::GZip));
        SC_TEST_EXPECT(response.hasTransferCoding(HttpClientTransferCoding::Chunked));
        SC_TEST_EXPECT(not response.hasTransferCoding(HttpClientTransferCoding::Deflate));

        uint64_t contentLength = 0;
        SC_TEST_EXPECT(response.getContentLength(contentLength));
        SC_TEST_EXPECT(contentLength == 42);
        SC_TEST_EXPECT(response.isHttp2());
        SC_TEST_EXPECT(not response.isHttp11());

        static const char InvalidLengthHeaders[] = "HTTP/1.1 200 OK\r\n"
                                                   "Content-Length: 42x\r\n"
                                                   "\r\n";
        response.headers                         = {InvalidLengthHeaders, sizeof(InvalidLengthHeaders) - 1};
        response.headersLength                   = sizeof(InvalidLengthHeaders) - 1;
        contentLength                            = 7;
        SC_TEST_EXPECT(not response.getContentLength(contentLength));
        SC_TEST_EXPECT(contentLength == 7);

        static const char RepeatedContentEncodingHeaders[] = "HTTP/1.1 200 OK\r\n"
                                                             "Content-Encoding: , deflate, sc-test\r\n"
                                                             "Content-Encoding: identity\r\n"
                                                             "\r\n";
        response.headers       = {RepeatedContentEncodingHeaders, sizeof(RepeatedContentEncodingHeaders) - 1};
        response.headersLength = sizeof(RepeatedContentEncodingHeaders) - 1;
        contentCodingIterator  = {};
        SC_TEST_EXPECT(response.getNextContentCoding(contentCodingIterator, contentCoding));
        SC_TEST_EXPECT(contentCoding.name == "deflate"_a8);
        SC_TEST_EXPECT(contentCoding.type == HttpClientContentCoding::Deflate);
        SC_TEST_EXPECT(response.getNextContentCoding(contentCodingIterator, contentCoding));
        SC_TEST_EXPECT(contentCoding.name == "sc-test"_a8);
        SC_TEST_EXPECT(contentCoding.type == HttpClientContentCoding::Unknown);
        SC_TEST_EXPECT(response.getNextContentCoding(contentCodingIterator, contentCoding));
        SC_TEST_EXPECT(contentCoding.name == "identity"_a8);
        SC_TEST_EXPECT(contentCoding.isIdentity());
        SC_TEST_EXPECT(not response.getNextContentCoding(contentCodingIterator, contentCoding));
        SC_TEST_EXPECT(response.hasContentCoding(HttpClientContentCoding::Unknown));
        SC_TEST_EXPECT(response.hasContentCoding(HttpClientContentCoding::Identity));
        SC_TEST_EXPECT(HttpClientContentCoding::parseName("X-GZIP"_a8) == HttpClientContentCoding::GZip);
        SC_TEST_EXPECT(HttpClientContentCoding::parseName("x-compress"_a8) == HttpClientContentCoding::Compress);

        static const char RepeatedTransferEncodingHeaders[] = "HTTP/1.1 200 OK\r\n"
                                                              "Transfer-Encoding: , deflate, sc-test\r\n"
                                                              "Transfer-Encoding: chunked\r\n"
                                                              "\r\n";
        response.headers       = {RepeatedTransferEncodingHeaders, sizeof(RepeatedTransferEncodingHeaders) - 1};
        response.headersLength = sizeof(RepeatedTransferEncodingHeaders) - 1;
        transferCodingIterator = {};
        SC_TEST_EXPECT(response.getNextTransferCoding(transferCodingIterator, transferCoding));
        SC_TEST_EXPECT(transferCoding.name == "deflate"_a8);
        SC_TEST_EXPECT(transferCoding.type == HttpClientTransferCoding::Deflate);
        SC_TEST_EXPECT(response.getNextTransferCoding(transferCodingIterator, transferCoding));
        SC_TEST_EXPECT(transferCoding.name == "sc-test"_a8);
        SC_TEST_EXPECT(transferCoding.type == HttpClientTransferCoding::Unknown);
        SC_TEST_EXPECT(response.getNextTransferCoding(transferCodingIterator, transferCoding));
        SC_TEST_EXPECT(transferCoding.name == "chunked"_a8);
        SC_TEST_EXPECT(transferCoding.isChunked());
        SC_TEST_EXPECT(not response.getNextTransferCoding(transferCodingIterator, transferCoding));
        SC_TEST_EXPECT(response.hasTransferCoding(HttpClientTransferCoding::Unknown));
        SC_TEST_EXPECT(response.hasTransferCoding(HttpClientTransferCoding::Chunked));
        SC_TEST_EXPECT(HttpClientTransferCoding::parseName("X-GZIP"_a8) == HttpClientTransferCoding::GZip);
        SC_TEST_EXPECT(HttpClientTransferCoding::parseName("x-compress"_a8) == HttpClientTransferCoding::Compress);
        SC_TEST_EXPECT(strcmp(HttpClientTransferCoding::getName(HttpClientTransferCoding::Unknown), "unknown") == 0);

        HttpClientContentCoding::Type acceptedCodings[] = {HttpClientContentCoding::GZip,
                                                           HttpClientContentCoding::Deflate};
        char                          acceptEncodingScratch[32];
        StringSpan                    acceptEncoding;
        SC_TEST_EXPECT(HttpClientContentCoding::writeAcceptEncoding(
            acceptedCodings, {acceptEncodingScratch, sizeof(acceptEncodingScratch)}, acceptEncoding));
        SC_TEST_EXPECT(acceptEncoding == "gzip, deflate"_a8);

        HttpClientContentCoding::Type unknownCodings[] = {HttpClientContentCoding::Unknown};
        SC_TEST_EXPECT(not HttpClientContentCoding::writeAcceptEncoding(
            unknownCodings, {acceptEncodingScratch, sizeof(acceptEncodingScratch)}, acceptEncoding));
        char tinyAcceptEncodingScratch[4];
        SC_TEST_EXPECT(not HttpClientContentCoding::writeAcceptEncoding(
            acceptedCodings, {tinyAcceptEncodingScratch, sizeof(tinyAcceptEncodingScratch)}, acceptEncoding));
    }

    void contentCodingPolicy()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            StringView acceptEncoding;
            SC_TEST_EXPECT(client.request.getHeader("Accept-Encoding"_a8, acceptEncoding));
            SC_TEST_EXPECT(acceptEncoding == "sc-test"_a8);

            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Encoding"_a8, "sc-test"_a8));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "4"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.getWritableStream().write("RAW!"));
            SC_TEST_EXPECT(client.response.end());
        };

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;

                HttpClientHeader  header = {"Accept-Encoding"_a8, "sc-test"_a8};
                HttpClientRequest request;
                request.url     = server.endpoint.view();
                request.headers = {&header, 1};

                HttpClientResponse response;
                char               body[64]   = {};
                size_t             bodyLength = 0;

                SC_TEST_EXPECT(
                    HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength, memory.memory));
                SC_TEST_EXPECT(response.statusCode == 200);
                SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "RAW!");
                StringSpan contentEncoding;
                SC_TEST_EXPECT(response.getContentEncoding(contentEncoding));
                SC_TEST_EXPECT(contentEncoding == "sc-test"_a8);
                HttpClientContentCodingIterator contentCodingIterator;
                HttpClientContentCoding         contentCoding;
                SC_TEST_EXPECT(response.getNextContentCoding(contentCodingIterator, contentCoding));
                SC_TEST_EXPECT(contentCoding.name == "sc-test"_a8);
                SC_TEST_EXPECT(contentCoding.type == HttpClientContentCoding::Unknown);
                SC_TEST_EXPECT(not response.getNextContentCoding(contentCodingIterator, contentCoding));
                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void sessionLayer()
    {
        HttpClientSessionCookie         cookies[4];
        HttpClientSessionAuthCacheEntry authEntries[2];
        HttpClientHeader                requestHeaders[4];
        char                            headerScratch[256];
        char                            stateScratch[512];
        char                            basicAuthorizationScratch[64];

        HttpClientSessionMemory sessionMemory;
        sessionMemory.cookies        = {cookies, 4};
        sessionMemory.authEntries    = {authEntries, 2};
        sessionMemory.requestHeaders = {requestHeaders, 4};
        sessionMemory.headerScratch  = {headerScratch, sizeof(headerScratch)};
        sessionMemory.stateScratch   = {stateScratch, sizeof(stateScratch)};

        HttpClientSession session;
        SC_TEST_EXPECT(session.init(sessionMemory));

        static const char RawHeaders[] = "HTTP/1.1 200 OK\r\n"
                                         "Set-Cookie: sid=abc; Path=/api; HttpOnly\r\n"
                                         "Set-Cookie: global=1; Domain=.example.test; Secure\r\n"
                                         "\r\n";

        HttpClientRequest responseRequest;
        responseRequest.url = "https://example.test/api/login"_a8;

        HttpClientResponse response;
        response.headers       = {RawHeaders, sizeof(RawHeaders) - 1};
        response.headersLength = sizeof(RawHeaders) - 1;

        SC_TEST_EXPECT(session.captureResponse(responseRequest, response));
        SC_TEST_EXPECT(session.getNumCookies() == 2);
        HttpClientSessionCookie cookie;
        SC_TEST_EXPECT(session.findCookie("sid"_a8, "example.test"_a8, "/api"_a8, cookie));
        SC_TEST_EXPECT(cookie.value == "abc"_a8);
        SC_TEST_EXPECT(cookie.domain == "example.test"_a8);
        SC_TEST_EXPECT(cookie.path == "/api"_a8);
        SC_TEST_EXPECT((cookie.flags & HttpClientSessionCookie::HttpOnly) != 0);
        SC_TEST_EXPECT(session.hasCookie("sid"_a8, "example.test"_a8, "/api"_a8));
        SC_TEST_EXPECT(session.findCookie("GLOBAL"_a8, "example.test"_a8, "/"_a8, cookie));
        SC_TEST_EXPECT(cookie.value == "1"_a8);
        SC_TEST_EXPECT((cookie.flags & HttpClientSessionCookie::Secure) != 0);
        SC_TEST_EXPECT((cookie.flags & HttpClientSessionCookie::DomainCookie) != 0);
        SC_TEST_EXPECT(not session.findCookie("missing"_a8, "example.test"_a8, "/"_a8, cookie));
        SC_TEST_EXPECT(not session.hasCookie("missing"_a8, "example.test"_a8, "/"_a8));

        StringSpan basicAuthorization;
        SC_TEST_EXPECT(HttpClientSession::makeBasicAuthorization(
            "test"_a8, "secret"_a8, {basicAuthorizationScratch, sizeof(basicAuthorizationScratch)},
            basicAuthorization));
        SC_TEST_EXPECT(basicAuthorization == "Basic dGVzdDpzZWNyZXQ="_a8);
        char       tinyAuthorizationScratch[8];
        StringSpan tinyAuthorization;
        SC_TEST_EXPECT(not HttpClientSession::makeBasicAuthorization(
            "test"_a8, "secret"_a8, {tinyAuthorizationScratch, sizeof(tinyAuthorizationScratch)}, tinyAuthorization));
        SC_TEST_EXPECT(session.addAuthorization("https://example.test"_a8, basicAuthorization));
        SC_TEST_EXPECT(session.getNumAuthorizations() == 1);
        StringSpan cachedAuthorization;
        SC_TEST_EXPECT(session.findAuthorization("HTTPS://EXAMPLE.TEST"_a8, cachedAuthorization));
        SC_TEST_EXPECT(cachedAuthorization == "Basic dGVzdDpzZWNyZXQ="_a8);
        SC_TEST_EXPECT(session.hasAuthorization("https://example.test"_a8));
        SC_TEST_EXPECT(not session.hasAuthorization("https://other.test"_a8));

        static const char  RawOriginChallengeHeaders[] = "HTTP/1.1 401 Unauthorized\r\n"
                                                         "WWW-Authenticate: Digest realm=\"ignored\", "
                                                         "Basic charset=\"UTF-8\", realm=\"origin,zone\"\r\n"
                                                         "\r\n";
        HttpClientResponse originChallengeResponse;
        originChallengeResponse.statusCode    = 401;
        originChallengeResponse.headers       = {RawOriginChallengeHeaders, sizeof(RawOriginChallengeHeaders) - 1};
        originChallengeResponse.headersLength = sizeof(RawOriginChallengeHeaders) - 1;

        HttpClientSessionAuthChallenge challenge;
        SC_TEST_EXPECT(HttpClientSession::findBasicAuthChallenge(originChallengeResponse,
                                                                 HttpClientSessionAuthChallenge::Origin, challenge));
        SC_TEST_EXPECT(challenge.target == HttpClientSessionAuthChallenge::Origin);
        SC_TEST_EXPECT(challenge.scheme == HttpClientSessionAuthChallenge::Basic);
        SC_TEST_EXPECT(strcmp(challenge.getTargetName(), "origin") == 0);
        SC_TEST_EXPECT(strcmp(challenge.getSchemeName(), "basic") == 0);
        SC_TEST_EXPECT(challenge.realm == "origin,zone"_a8);

        StringSpan challengeAuthorization;
        SC_TEST_EXPECT(HttpClientSession::makeBasicAuthorizationForChallenge(
            originChallengeResponse, HttpClientSessionAuthChallenge::Origin, "test"_a8, "secret"_a8,
            {basicAuthorizationScratch, sizeof(basicAuthorizationScratch)}, challengeAuthorization));
        SC_TEST_EXPECT(challengeAuthorization == "Basic dGVzdDpzZWNyZXQ="_a8);

        static const char  RawProxyChallengeHeaders[] = "HTTP/1.1 407 Proxy Authentication Required\r\n"
                                                        "Proxy-Authenticate: Basic realm=proxy\r\n"
                                                        "\r\n";
        HttpClientResponse proxyChallengeResponse;
        proxyChallengeResponse.statusCode    = 407;
        proxyChallengeResponse.headers       = {RawProxyChallengeHeaders, sizeof(RawProxyChallengeHeaders) - 1};
        proxyChallengeResponse.headersLength = sizeof(RawProxyChallengeHeaders) - 1;

        SC_TEST_EXPECT(HttpClientSession::findBasicAuthChallenge(proxyChallengeResponse,
                                                                 HttpClientSessionAuthChallenge::Proxy, challenge));
        SC_TEST_EXPECT(challenge.target == HttpClientSessionAuthChallenge::Proxy);
        SC_TEST_EXPECT(challenge.scheme == HttpClientSessionAuthChallenge::Basic);
        SC_TEST_EXPECT(strcmp(challenge.getTargetName(), "proxy") == 0);
        SC_TEST_EXPECT(
            strcmp(HttpClientSessionAuthChallenge::getSchemeName(HttpClientSessionAuthChallenge::Unsupported),
                   "unsupported") == 0);
        SC_TEST_EXPECT(strcmp(HttpClientSessionAuthChallenge::getTargetName(
                                  static_cast<HttpClientSessionAuthChallenge::Target>(0xFF)),
                              "unknown") == 0);
        SC_TEST_EXPECT(strcmp(HttpClientSessionAuthChallenge::getSchemeName(
                                  static_cast<HttpClientSessionAuthChallenge::Scheme>(0xFF)),
                              "unknown") == 0);
        SC_TEST_EXPECT(challenge.realm == "proxy"_a8);
        SC_TEST_EXPECT(HttpClientSession::makeBasicAuthorizationForChallenge(
            proxyChallengeResponse, HttpClientSessionAuthChallenge::Proxy, "test"_a8, "secret"_a8,
            {basicAuthorizationScratch, sizeof(basicAuthorizationScratch)}, challengeAuthorization));

        proxyChallengeResponse.statusCode = 200;
        SC_TEST_EXPECT(not HttpClientSession::makeBasicAuthorizationForChallenge(
            proxyChallengeResponse, HttpClientSessionAuthChallenge::Proxy, "test"_a8, "secret"_a8,
            {basicAuthorizationScratch, sizeof(basicAuthorizationScratch)}, challengeAuthorization));

        static const char  RawDigestOnlyChallengeHeaders[] = "HTTP/1.1 401 Unauthorized\r\n"
                                                             "WWW-Authenticate: Digest realm=\"ignored\"\r\n"
                                                             "\r\n";
        HttpClientResponse digestOnlyChallengeResponse;
        digestOnlyChallengeResponse.statusCode    = 401;
        digestOnlyChallengeResponse.headers       = {RawDigestOnlyChallengeHeaders,
                                                     sizeof(RawDigestOnlyChallengeHeaders) - 1};
        digestOnlyChallengeResponse.headersLength = sizeof(RawDigestOnlyChallengeHeaders) - 1;
        SC_TEST_EXPECT(not HttpClientSession::findBasicAuthChallenge(
            digestOnlyChallengeResponse, HttpClientSessionAuthChallenge::Origin, challenge));

        HttpClientHeader  sourceHeader = {"X-Test"_a8, "1"_a8};
        HttpClientRequest source;
        source.url     = "https://example.test/api/list"_a8;
        source.headers = {&sourceHeader, 1};

        HttpClientRequest prepared;
        SC_TEST_EXPECT(session.prepareRequest(source, prepared));
        SC_TEST_EXPECT(prepared.headers.sizeInElements() == 3);
        SC_TEST_EXPECT(prepared.headers[0].name == "X-Test"_a8);
        SC_TEST_EXPECT(prepared.headers[1].name == "Cookie"_a8);
        SC_TEST_EXPECT(prepared.headers[1].value == "sid=abc; global=1"_a8);
        SC_TEST_EXPECT(prepared.headers[2].name == "Authorization"_a8);
        SC_TEST_EXPECT(prepared.headers[2].value == "Basic dGVzdDpzZWNyZXQ="_a8);

        HttpClientRequest httpSource;
        httpSource.url = "http://sub.example.test/api/list"_a8;
        SC_TEST_EXPECT(session.prepareRequest(httpSource, prepared));
        SC_TEST_EXPECT(prepared.headers.empty());

        HttpClientHeader  callerCookie = {"Cookie"_a8, "manual=1"_a8};
        HttpClientRequest manualCookieSource;
        manualCookieSource.url     = "https://example.test/api/list"_a8;
        manualCookieSource.headers = {&callerCookie, 1};
        SC_TEST_EXPECT(session.prepareRequest(manualCookieSource, prepared));
        SC_TEST_EXPECT(prepared.headers.sizeInElements() == 2);
        SC_TEST_EXPECT(prepared.headers[0].value == "manual=1"_a8);
        SC_TEST_EXPECT(prepared.headers[1].name == "Authorization"_a8);
        SC_TEST_EXPECT(prepared.headers[1].value == "Basic dGVzdDpzZWNyZXQ="_a8);

        session.clearAuthorizations();
        SC_TEST_EXPECT(session.getNumAuthorizations() == 0);
        SC_TEST_EXPECT(not session.findAuthorization("https://example.test"_a8, cachedAuthorization));
        SC_TEST_EXPECT(session.prepareRequest(source, prepared));
        SC_TEST_EXPECT(prepared.headers.sizeInElements() == 2);
        SC_TEST_EXPECT(prepared.headers[0].name == "X-Test"_a8);
        SC_TEST_EXPECT(prepared.headers[1].name == "Cookie"_a8);
        SC_TEST_EXPECT(prepared.headers[1].value == "sid=abc; global=1"_a8);

        session.clearCookies();
        SC_TEST_EXPECT(session.getNumCookies() == 0);
        SC_TEST_EXPECT(not session.findCookie("sid"_a8, "example.test"_a8, "/api"_a8, cookie));
        SC_TEST_EXPECT(not session.hasCookie("sid"_a8, "example.test"_a8, "/api"_a8));
        SC_TEST_EXPECT(session.prepareRequest(source, prepared));
        SC_TEST_EXPECT(prepared.headers.sizeInElements() == 1);
        SC_TEST_EXPECT(prepared.headers[0].name == "X-Test"_a8);

        HttpClientSessionRetryPolicy policy;
        policy.maxAttempts = 3;
        HttpClientSessionRetryState retryState;
        SC_TEST_EXPECT(not retryState.isStarted());
        SC_TEST_EXPECT(retryState.hasAttemptsRemaining());
        SC_TEST_EXPECT(retryState.getRemainingAttempts() == 1);
        SC_TEST_EXPECT(HttpClientSession::isIdempotentMethod(HttpClientRequest::HttpGET));
        SC_TEST_EXPECT(HttpClientSession::isIdempotentMethod(HttpClientRequest::HttpOPTIONS));
        SC_TEST_EXPECT(not HttpClientSession::isIdempotentMethod(HttpClientRequest::HttpPOST));
        SC_TEST_EXPECT(not HttpClientSession::isIdempotentMethod(static_cast<HttpClientRequest::Method>(0xFF)));
        SC_TEST_EXPECT(HttpClientSession::isRetryableStatusCode(503));
        SC_TEST_EXPECT(HttpClientSession::isRetryableStatusCode(429));
        SC_TEST_EXPECT(not HttpClientSession::isRetryableStatusCode(404));
        SC_TEST_EXPECT(session.beginRetry(retryState, source, policy));
        SC_TEST_EXPECT(retryState.isStarted());
        SC_TEST_EXPECT(retryState.hasAttemptsRemaining());
        SC_TEST_EXPECT(retryState.getRemainingAttempts() == 2);
        response.statusCode = 503;
        SC_TEST_EXPECT(session.shouldRetry(retryState, Result(true), &response));
        SC_TEST_EXPECT(retryState.attemptsStarted == 2);
        SC_TEST_EXPECT(retryState.getRemainingAttempts() == 1);
        SC_TEST_EXPECT(session.shouldRetry(retryState, Result::Error("transport"), nullptr));
        SC_TEST_EXPECT(retryState.attemptsStarted == 3);
        SC_TEST_EXPECT(not retryState.hasAttemptsRemaining());
        SC_TEST_EXPECT(retryState.getRemainingAttempts() == 0);
        SC_TEST_EXPECT(not session.shouldRetry(retryState, Result(true), &response));

        HttpClientRequest postSource;
        postSource.method     = HttpClientRequest::HttpPOST;
        postSource.url        = "https://example.test/api/list"_a8;
        postSource.body.bytes = {"body", 4};
        SC_TEST_EXPECT(session.beginRetry(retryState, postSource, policy));
        SC_TEST_EXPECT(not session.shouldRetry(retryState, Result::Error("transport"), nullptr));

        policy.retryNonIdempotentReplayableBody = true;
        SC_TEST_EXPECT(session.beginRetry(retryState, postSource, policy));
        SC_TEST_EXPECT(session.shouldRetry(retryState, Result::Error("transport"), nullptr));

        session.clear();
        SC_TEST_EXPECT(session.getNumCookies() == 0);
        SC_TEST_EXPECT(session.getNumAuthorizations() == 0);
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

                request.url                               = server.endpoint.view();
                request.options.timeouts.requestTimeoutMs = 100;

                SC_TEST_EXPECT(not HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               memory.memory));
                SC_TEST_EXPECT(client.close());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void redirectPolicy()
    {
        struct DummyBodyProvider final : public HttpClientRequestBodyProvider
        {
            Result pullRequestBody(Span<char>, size_t& bytesWritten, bool& endReached) override
            {
                bytesWritten = 0;
                endReached   = true;
                return Result(true);
            }
        };
        DummyBodyProvider dummyBodyProvider;

        HttpClient client;
        SC_TEST_EXPECT(client.init());

        CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;
        HttpClientOperation                                    operation;
        SC_TEST_EXPECT(operation.init(client, memory.memory));

        {
            HttpClientRequest request;
            request.url                   = "http://127.0.0.1:1/redirect"_a8;
            request.options.redirect.mode = static_cast<HttpClientRequestRedirectOptions::Mode>(0xFF);

            HttpClientResponse response;
            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest request;
            request.url                   = "http://127.0.0.1:1/redirect"_a8;
            request.method                = HttpClientRequest::HttpPOST;
            request.options.redirect.mode = HttpClientRequestRedirectOptions::FollowGetHead;

            HttpClientResponse response;
            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest request;
            request.url                   = "http://127.0.0.1:1/redirect"_a8;
            request.method                = HttpClientRequest::HttpPOST;
            request.options.redirect.mode = HttpClientRequestRedirectOptions::FollowAll;
            request.body.provider         = &dummyBodyProvider;
            request.body.sizeInBytes      = 4;
            request.body.canReplay        = false;
            request.body.framing          = HttpClientRequestBody::SizedStream;

            HttpClientResponse response;
            SC_TEST_EXPECT(not operation.start(request, response));
        }

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(client.close());
    }

    void protocolPreference()
    {
        HttpClient client;
        SC_TEST_EXPECT(client.init());

        CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;
        HttpClientOperation                                    operation;
        SC_TEST_EXPECT(operation.init(client, memory.memory));

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                         = "http://127.0.0.1:1/protocol"_a8;
            request.options.protocol.preference = static_cast<HttpClientRequestProtocolOptions::Preference>(0xFF);

            SC_TEST_EXPECT(not operation.start(request, response));
        }

#if SC_PLATFORM_APPLE
        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                         = "http://127.0.0.1:1/protocol"_a8;
            request.options.protocol.preference = HttpClientRequestProtocolOptions::Http11Only;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                         = "http://127.0.0.1:1/protocol"_a8;
            request.options.protocol.preference = HttpClientRequestProtocolOptions::Http2Required;

            SC_TEST_EXPECT(not operation.start(request, response));
        }
#endif

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(client.close());

        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& httpConnection)
        {
            SC_TEST_EXPECT(httpConnection.response.startResponse(200));
            SC_TEST_EXPECT(httpConnection.response.addHeader("Content-Length"_a8, "2"_a8));
            SC_TEST_EXPECT(httpConnection.response.sendHeaders());
            SC_TEST_EXPECT(httpConnection.response.getWritableStream().write("OK"));
            SC_TEST_EXPECT(httpConnection.response.end());
        };

        Thread       clientThread;
        const Result clientStartResult = clientThread.start(
            [&](Thread&)
            {
                HttpClient localClient;
                SC_TEST_EXPECT(localClient.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> localMemory;

                {
                    HttpClientRequest  request;
                    HttpClientResponse response;
                    char               body[64]   = {};
                    size_t             bodyLength = 0;

                    request.url                         = server.endpoint.view();
                    request.options.protocol.preference = HttpClientRequestProtocolOptions::Http2Preferred;

                    SC_TEST_EXPECT(HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               localMemory.memory));
                    SC_TEST_EXPECT(response.statusCode == 200);
                    SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "OK");
                    SC_TEST_EXPECT(response.negotiatedProtocol == HttpClientResponse::Protocol::Http11 or
                                   response.negotiatedProtocol == HttpClientResponse::Protocol::Unknown);
                }

#if !SC_PLATFORM_APPLE
                {
                    HttpClientRequest  request;
                    HttpClientResponse response;
                    char               body[64]   = {};
                    size_t             bodyLength = 0;

                    request.url                         = server.endpoint.view();
                    request.options.protocol.preference = HttpClientRequestProtocolOptions::Http11Only;

                    SC_TEST_EXPECT(HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               localMemory.memory));
                    SC_TEST_EXPECT(response.statusCode == 200);
                    SC_TEST_EXPECT(response.negotiatedProtocol == HttpClientResponse::Protocol::Http11);
                }
#endif

                SC_TEST_EXPECT(localClient.close());
                SC_TEST_EXPECT(server.scheduleStop());
            });
        SC_TEST_EXPECT(clientStartResult);

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void proxyOptions()
    {
        HttpClient client;
        SC_TEST_EXPECT(client.init());

        CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;
        HttpClientOperation                                    operation;
        SC_TEST_EXPECT(operation.init(client, memory.memory));

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                               = "http://127.0.0.1:1/proxy"_a8;
            request.options.proxy.mode                = static_cast<HttpClientRequestProxyOptions::Mode>(0xFF);
            request.options.timeouts.requestTimeoutMs = 1;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                               = "http://127.0.0.1:1/proxy"_a8;
            request.options.proxy.url                 = "http://127.0.0.1:1"_a8;
            request.options.timeouts.requestTimeoutMs = 1;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                               = "http://127.0.0.1:1/proxy"_a8;
            request.options.proxy.authorization       = "Basic dGVzdA=="_a8;
            request.options.timeouts.requestTimeoutMs = 1;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                               = "http://127.0.0.1:1/proxy"_a8;
            request.options.proxy.mode                = HttpClientRequestProxyOptions::Http;
            request.options.proxy.url                 = "http://127.0.0.1:1"_a8;
            request.options.proxy.authorization       = "Basic bad\r\n"_a8;
            request.options.timeouts.requestTimeoutMs = 1;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                               = "http://127.0.0.1:1/proxy"_a8;
            request.options.proxy.mode                = HttpClientRequestProxyOptions::NoProxy;
            request.options.proxy.bypassList          = "127.0.0.1"_a8;
            request.options.timeouts.requestTimeoutMs = 1;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                               = "http://127.0.0.1:1/proxy"_a8;
            request.options.proxy.mode                = HttpClientRequestProxyOptions::Http;
            request.options.proxy.url                 = "https://127.0.0.1:1"_a8;
            request.options.timeouts.requestTimeoutMs = 1;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

#if SC_PLATFORM_APPLE
        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url                               = "http://127.0.0.1:1/proxy"_a8;
            request.options.proxy.mode                = HttpClientRequestProxyOptions::NoProxy;
            request.options.timeouts.requestTimeoutMs = 1;

            SC_TEST_EXPECT(not operation.start(request, response));
        }
#endif

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(client.close());

#if !SC_PLATFORM_APPLE
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));

        size_t receivedRequests = 0;
        server.server.onRequest = [this, &receivedRequests](HttpConnection& httpConnection)
        {
            receivedRequests += 1;
            if (receivedRequests == 1)
            {
                StringView proxyAuthorization;
                SC_TEST_EXPECT(httpConnection.request.getHeader("Proxy-Authorization"_a8, proxyAuthorization));
                SC_TEST_EXPECT(proxyAuthorization == "Basic dGVzdA=="_a8);
            }
            else
            {
                StringView proxyAuthorization;
                SC_TEST_EXPECT(not httpConnection.request.getHeader("Proxy-Authorization"_a8, proxyAuthorization));
            }
            SC_TEST_EXPECT(httpConnection.response.startResponse(200));
            SC_TEST_EXPECT(httpConnection.response.addHeader("Content-Length"_a8, "2"_a8));
            SC_TEST_EXPECT(httpConnection.response.sendHeaders());
            SC_TEST_EXPECT(httpConnection.response.getWritableStream().write("OK"));
            SC_TEST_EXPECT(httpConnection.response.end());
        };

        Thread       clientThread;
        const Result clientStartResult = clientThread.start(
            [&](Thread&)
            {
                HttpClient localClient;
                SC_TEST_EXPECT(localClient.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> localMemory;

                {
                    HttpClientRequest  request;
                    HttpClientResponse response;
                    char               body[64]   = {};
                    size_t             bodyLength = 0;

                    request.url                               = "http://example.invalid/proxy-target"_a8;
                    request.options.proxy.mode                = HttpClientRequestProxyOptions::Http;
                    request.options.proxy.url                 = server.endpoint.view();
                    request.options.proxy.authorization       = "Basic dGVzdA=="_a8;
                    request.options.timeouts.requestTimeoutMs = 500;

                    SC_TEST_EXPECT(HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               localMemory.memory));
                    SC_TEST_EXPECT(response.statusCode == 200);
                    SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "OK");
                }

                {
                    HttpClientRequest  request;
                    HttpClientResponse response;
                    char               body[64]   = {};
                    size_t             bodyLength = 0;

                    request.url                = server.endpoint.view();
                    request.options.proxy.mode = HttpClientRequestProxyOptions::NoProxy;

                    SC_TEST_EXPECT(HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               localMemory.memory));
                    SC_TEST_EXPECT(response.statusCode == 200);
                    SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "OK");
                }

                {
                    HttpClientRequest  request;
                    HttpClientResponse response;
                    char               body[64]   = {};
                    size_t             bodyLength = 0;

                    request.url                               = server.endpoint.view();
                    request.options.proxy.mode                = HttpClientRequestProxyOptions::Http;
                    request.options.proxy.url                 = "http://127.0.0.1:1"_a8;
                    request.options.proxy.bypassList          = "127.0.0.1,localhost"_a8;
                    request.options.timeouts.requestTimeoutMs = 500;

                    SC_TEST_EXPECT(HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                               localMemory.memory));
                    SC_TEST_EXPECT(response.statusCode == 200);
                    SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "OK");
                }

                SC_TEST_EXPECT(localClient.close());
                SC_TEST_EXPECT(server.scheduleStop());
            });
        SC_TEST_EXPECT(clientStartResult);

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(receivedRequests == 3);
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
#endif
    }

    void cancelRequest()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [](HttpConnection&) {};

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
                char               body[64] = {};

                PollResponseCollector collector;
                collector.bodyBuffer = {body, sizeof(body)};

                request.url = server.endpoint.view();
                // Keep cancellation bounded on backends where tearing down a blocked receive is not instantaneous,
                // while still preferring the explicit cancel path over a long fallback wait.
                request.options.timeouts.requestTimeoutMs = 100;
                SC_TEST_EXPECT(operation.start(request, response, &collector));
                SC_TEST_EXPECT(operation.cancel());
                while (not collector.completed)
                {
                    SC_TEST_EXPECT(operation.poll(5));
                }

                SC_TEST_EXPECT(not collector.finalRes);
                SC_TEST_EXPECT(operation.close());
                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void methodCoverage()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            const HttpParser::Method method = client.request.getParser().method;
            SC_TEST_EXPECT(method == HttpParser::Method::HttpPUT);
            const StringSpan responseBody = "PUT"_a8;

            char      contentLength[16] = {};
            const int written = snprintf(contentLength, sizeof(contentLength), "%zu", responseBody.sizeInBytes());
            SC_TEST_EXPECT(written > 0);
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(
                client.response.addHeader("Content-Length"_a8, StringSpan({contentLength, static_cast<size_t>(written)},
                                                                          false, StringEncoding::Ascii)));
            SC_TEST_EXPECT(client.response.sendHeaders());
            if (responseBody.sizeInBytes() > 0)
            {
                SC_TEST_EXPECT(client.response.getWritableStream().write(responseBody.toCharSpan()));
            }
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
                char               body[64]   = {};
                size_t             bodyLength = 0;

                request.url    = server.endpoint.view();
                request.method = HttpClientRequest::HttpPUT;

                SC_TEST_EXPECT(
                    HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength, memory.memory));
                SC_TEST_EXPECT(response.statusCode == 200);
                SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "PUT");

                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void streamedBodySizeValidation()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [](HttpConnection&) {};

        struct FixedBodyProvider final : public HttpClientRequestBodyProvider
        {
            Span<const char> payload;

            bool sent = false;

            Result pullRequestBody(Span<char> dest, size_t& bytesWritten, bool& endReached) override
            {
                bytesWritten = 0;
                endReached   = false;
                if (sent)
                {
                    endReached = true;
                    return Result(true);
                }

                const size_t toCopy =
                    payload.sizeInBytes() < dest.sizeInBytes() ? payload.sizeInBytes() : dest.sizeInBytes();
                memcpy(dest.data(), payload.data(), toCopy);
                bytesWritten = toCopy;
                sent         = true;
                endReached   = true;
                return Result(true);
            }
        };

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;

                {
                    FixedBodyProvider provider;
                    provider.payload = {"abc", 3};

                    HttpClientRequest  request;
                    HttpClientResponse response;
                    char               body[64]   = {};
                    size_t             bodyLength = 0;

                    request.url              = server.endpoint.view();
                    request.method           = HttpClientRequest::HttpPOST;
                    request.body.provider    = &provider;
                    request.body.sizeInBytes = 5;
                    request.body.framing     = HttpClientRequestBody::SizedStream;

                    SC_TEST_EXPECT(not HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                                   memory.memory));
                }

                {
                    FixedBodyProvider provider;
                    provider.payload = {"abcdef", 6};

                    HttpClientRequest  request;
                    HttpClientResponse response;
                    char               body[64]   = {};
                    size_t             bodyLength = 0;

                    request.url              = server.endpoint.view();
                    request.method           = HttpClientRequest::HttpPOST;
                    request.body.provider    = &provider;
                    request.body.sizeInBytes = 3;
                    request.body.framing     = HttpClientRequestBody::SizedStream;

                    SC_TEST_EXPECT(not HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength,
                                                                   memory.memory));
                }

                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(server.server.close());
        SC_TEST_EXPECT(loop.close());
    }

    void requestBodyFramingValidation()
    {
        struct FixedBodyProvider final : public HttpClientRequestBodyProvider
        {
            Result pullRequestBody(Span<char>, size_t& bytesWritten, bool& endReached) override
            {
                bytesWritten = 0;
                endReached   = true;
                return Result(true);
            }
        } provider;

        HttpClient client;
        SC_TEST_EXPECT(client.init());

        CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;
        HttpClientOperation                                    operation;
        SC_TEST_EXPECT(operation.init(client, memory.memory));

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url              = "http://127.0.0.1:1/body"_a8;
            request.method           = HttpClientRequest::HttpPOST;
            request.body.bytes       = {"abc", 3};
            request.body.provider    = &provider;
            request.body.sizeInBytes = 3;
            request.body.framing     = HttpClientRequestBody::SizedStream;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url              = "http://127.0.0.1:1/body"_a8;
            request.method           = HttpClientRequest::HttpPOST;
            request.body.bytes       = {"abc", 3};
            request.body.sizeInBytes = 4;
            request.body.framing     = HttpClientRequestBody::FixedSize;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;

            request.url              = "http://127.0.0.1:1/body"_a8;
            request.method           = HttpClientRequest::HttpPOST;
            request.body.provider    = &provider;
            request.body.sizeInBytes = 3;
            request.body.framing     = HttpClientRequestBody::ChunkedStream;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;
            HttpClientHeader   header = {"Content-Length"_a8, "3"_a8};

            request.url           = "http://127.0.0.1:1/body"_a8;
            request.method        = HttpClientRequest::HttpPOST;
            request.headers       = {&header, 1};
            request.body.provider = &provider;
            request.body.framing  = HttpClientRequestBody::ChunkedStream;

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        {
            HttpClientRequest  request;
            HttpClientResponse response;
            HttpClientHeader   header = {"Transfer-Encoding"_a8, "chunked"_a8};

            request.url     = "http://127.0.0.1:1/body"_a8;
            request.headers = {&header, 1};

            SC_TEST_EXPECT(not operation.start(request, response));
        }

        SC_TEST_EXPECT(operation.close());
        SC_TEST_EXPECT(client.close());
    }

    void chunkedUpload()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));

        static constexpr char   UploadBody[] = "ChunkedUploadBody";
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
                SC_ASSERT_RELEASE(receivedSize + data.sizeInBytes() <= UploadSize);
                memcpy(received + receivedSize, data.data(), data.sizeInBytes());
                receivedSize += data.sizeInBytes();
                SC_ASSERT_RELEASE(connection->request.consumeBodyBytes(data.sizeInBytes()));
            }

            void onEnd()
            {
                SC_ASSERT_RELEASE(connection != nullptr);
                SC_ASSERT_RELEASE(connection->request.getBodyFramingKind() == HttpBodyFramingKind::Chunked);
                SC_ASSERT_RELEASE(receivedSize == UploadSize);
                SC_ASSERT_RELEASE(memcmp(received, UploadBody, UploadSize) == 0);
                responseSent = true;
                SC_ASSERT_RELEASE(connection->response.startResponse(200));
                SC_ASSERT_RELEASE(connection->response.addHeader("Content-Length"_a8, "2"_a8));
                SC_ASSERT_RELEASE(connection->response.sendHeaders());
                SC_ASSERT_RELEASE(connection->response.getWritableStream().write("OK"));
                SC_ASSERT_RELEASE(connection->response.end());
            }
        } uploadCollector;

        server.server.onRequest = [this, &uploadCollector](HttpConnection& client)
        {
            uploadCollector.connection = &client;
            SC_TEST_EXPECT(client.request.getParser().method == HttpParser::Method::HttpPOST);
            SC_TEST_EXPECT(client.request.getBodyFramingKind() == HttpBodyFramingKind::Chunked);
            const bool dataListenerAdded =
                client.request.getReadableStream().eventData.addListener<UploadCollector, &UploadCollector::onData>(
                    uploadCollector);
            SC_TEST_EXPECT(dataListenerAdded);
            const bool endListenerAdded =
                client.request.getReadableStream().eventEnd.addListener<UploadCollector, &UploadCollector::onEnd>(
                    uploadCollector);
            SC_TEST_EXPECT(endListenerAdded);
        };

        struct ChunkedBodyProvider final : public HttpClientRequestBodyProvider
        {
            Span<const char> payload;
            size_t           offset       = 0;
            size_t           maxChunkSize = 5;

            Result pullRequestBody(Span<char> dest, size_t& bytesWritten, bool& endReached) override
            {
                bytesWritten = 0;
                endReached   = false;
                if (offset >= payload.sizeInBytes())
                {
                    endReached = true;
                    return Result(true);
                }

                size_t remaining = payload.sizeInBytes() - offset;
                if (remaining > maxChunkSize)
                {
                    remaining = maxChunkSize;
                }
                const size_t toCopy = remaining < dest.sizeInBytes() ? remaining : dest.sizeInBytes();
                memcpy(dest.data(), payload.data() + offset, toCopy);
                offset += toCopy;
                bytesWritten = toCopy;
                endReached   = (offset >= payload.sizeInBytes());
                return Result(true);
            }
        };

        Thread clientThread;
        SC_TEST_EXPECT(clientThread.start(
            [&](Thread&)
            {
                ChunkedBodyProvider provider;
                provider.payload = {UploadBody, UploadSize};

                HttpClient client;
                SC_TEST_EXPECT(client.init());

                CoreOperationMemory<64 * 1024, 8, 16, 4096, 16 * 1024> memory;

                HttpClientRequest  request;
                HttpClientResponse response;
                char               body[64]   = {};
                size_t             bodyLength = 0;

                request.url           = server.endpoint.view();
                request.method        = HttpClientRequest::HttpPOST;
                request.body.provider = &provider;
                request.body.framing  = HttpClientRequestBody::ChunkedStream;

                SC_TEST_EXPECT(
                    HttpClient::executeBlocking(request, response, {body, sizeof(body)}, bodyLength, memory.memory));
                SC_TEST_EXPECT(response.statusCode == 200);
                SC_TEST_EXPECT(StringView({body, bodyLength}, false, StringEncoding::Ascii) == "OK");
                SC_TEST_EXPECT(client.close());
                SC_TEST_EXPECT(server.scheduleStop());
            }));

        SC_TEST_EXPECT(loop.run());
        (void)clientThread.join();
        SC_TEST_EXPECT(uploadCollector.responseSent);
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

    void operationScheduler()
    {
        AsyncEventLoop loop;
        SC_TEST_EXPECT(loop.create());

        TestServer server(loop);
        SC_TEST_EXPECT(server.start(report));
        server.server.onRequest = [this](HttpConnection& client)
        {
            SC_TEST_EXPECT(client.response.startResponse(200));
            SC_TEST_EXPECT(client.response.addHeader("Content-Length"_a8, "5"_a8));
            SC_TEST_EXPECT(client.response.sendHeaders());
            SC_TEST_EXPECT(client.response.getWritableStream().write("READY"));
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
                HttpClientOperation*                                   operationPointers[NumClients];
                uint8_t                                                readyOperations[NumClients] = {};
                HttpClientResponse                                     responses[NumClients];
                PollResponseCollector                                  collectors[NumClients];
                char                                                   bodies[NumClients][32] = {};

                for (int idx = 0; idx < NumClients; ++idx)
                {
                    collectors[idx].bodyBuffer = {bodies[idx], sizeof(bodies[idx])};
                    SC_TEST_EXPECT(operations[idx].init(client, memories[idx].memory));
                    operationPointers[idx] = &operations[idx];
                }

                HttpClientOperationSchedulerMemory schedulerMemory;
                schedulerMemory.operations      = {operationPointers, NumClients};
                schedulerMemory.readyOperations = {readyOperations, NumClients};

                HttpClientOperationScheduler scheduler;
                SC_TEST_EXPECT(scheduler.init(schedulerMemory));
                SC_TEST_EXPECT(scheduler.getNumOperations() == NumClients);
                SC_TEST_EXPECT(scheduler.getNumReadyOperations() == NumClients);
                SC_TEST_EXPECT(scheduler.getNumRequestsInFlight() == 0);
                SC_TEST_EXPECT(scheduler.isOperationRegistered(operations[0]));

                for (int idx = 0; idx < NumClients; ++idx)
                {
                    HttpClientRequest request;
                    request.url = server.endpoint.view();
                    SC_TEST_EXPECT(operations[idx].start(request, responses[idx], &collectors[idx]));
                }
                SC_TEST_EXPECT(scheduler.getNumRequestsInFlight() == NumClients);

                while (scheduler.hasRequestsInFlight())
                {
                    size_t numPolled = 0;
                    SC_TEST_EXPECT(scheduler.pollReady(numPolled, 50));
                }

                for (int idx = 0; idx < NumClients; ++idx)
                {
                    SC_TEST_EXPECT(collectors[idx].completed);
                    SC_TEST_EXPECT(collectors[idx].finalRes);
                    SC_TEST_EXPECT(collectors[idx].headCount == 1);
                    SC_TEST_EXPECT(responses[idx].statusCode == 200);
                    SC_TEST_EXPECT(
                        StringView({bodies[idx], collectors[idx].bodyLength}, false, StringEncoding::Ascii) == "READY");
                }

                size_t numPolled = 1;
                SC_TEST_EXPECT(scheduler.pollAll(numPolled));
                SC_TEST_EXPECT(numPolled == NumClients);
                SC_TEST_EXPECT(scheduler.close());
                SC_TEST_EXPECT(scheduler.getNumOperations() == 0);
                SC_TEST_EXPECT(scheduler.getNumRequestsInFlight() == 0);
                SC_TEST_EXPECT(not scheduler.isOperationRegistered(operations[0]));

                for (int idx = 0; idx < NumClients; ++idx)
                {
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
        request.body.sizeInBytes = UploadSize;
        request.body.canReplay   = true;
        request.body.framing     = HttpClientRequestBody::SizedStream;

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
