// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// Linux backend for HttpClient using libcurl via dlopen.
#include "HttpClientLinuxAPI.h"

#include "../../Common/PlacementNew.h"

#include <limits.h>

struct SC::HttpClient::Internal
{
    HttpClientLinuxLibCurlLoader curl;
};

struct SC::HttpClientOperation::Internal
{
    struct curl_slist* requestHeaders = nullptr;

    pthread_t workerThread        = 0;
    CURL*     curlHandle          = nullptr;
    bool      workerRunning       = false;
    bool      workerThreadStarted = false;
    bool      cancelRequested     = false;
    bool      responseHeadSeen    = false;
    Result    callbackError       = Result(true);
};

namespace
{
static SC::Result appendResponseHeaderLine(SC::HttpClientResponse& response, const char* data, size_t size)
{
    const size_t remaining = response.headers.sizeInBytes() - response.headersLength;
    if (remaining < size)
        return SC::Result::Error(SC::HttpClientResultCategory, SC::HttpClientError::ResponseHeadersTooSmall);
    memcpy(const_cast<char*>(response.headers.data()) + response.headersLength, data, size);
    response.headersLength += size;
    return SC::Result(true);
}

static const char* getCustomMethod(SC::HttpClientRequest::Method method)
{
    switch (method)
    {
    case SC::HttpClientRequest::HttpGET: return "GET";
    case SC::HttpClientRequest::HttpPOST: return "POST";
    case SC::HttpClientRequest::HttpPUT: return "PUT";
    case SC::HttpClientRequest::HttpHEAD: return "HEAD";
    case SC::HttpClientRequest::HttpDELETE: return "DELETE";
    case SC::HttpClientRequest::HttpPATCH: return "PATCH";
    case SC::HttpClientRequest::HttpOPTIONS: return "OPTIONS";
    }
    return "GET";
}

static SC::Result httpClientLinuxAppendHeader(HttpClientLinuxLibCurlLoader& curl, struct curl_slist*& headers,
                                              const char* headerLine)
{
    struct curl_slist* newHeaders = curl.curl_slist_append(headers, headerLine);
    if (newHeaders == nullptr)
        return SC::Result::Error(SC::HttpClientResultCategory, SC::HttpClientError::RequestHeaderPreparationFailed);
    headers = newHeaders;
    return SC::Result(true);
}

static SC::Result httpClientLinuxAppendProxyAuthorization(HttpClientLinuxLibCurlLoader& curl,
                                                          struct curl_slist*& headers, SC::StringSpan authorization,
                                                          SC::Span<char> scratch)
{
    static const char   ProxyAuthorizationPrefix[]    = "Proxy-Authorization: ";
    static const size_t ProxyAuthorizationPrefixBytes = sizeof(ProxyAuthorizationPrefix) - 1;

    const SC::Span<const char> authorizationBytes = authorization.toCharSpan();
    if (scratch.sizeInBytes() <= ProxyAuthorizationPrefixBytes + authorizationBytes.sizeInBytes())
        return SC::Result::Error(SC::HttpClientResultCategory, SC::HttpClientError::BackendScratchTooSmall);
    memcpy(scratch.data(), ProxyAuthorizationPrefix, ProxyAuthorizationPrefixBytes);
    memcpy(scratch.data() + ProxyAuthorizationPrefixBytes, authorizationBytes.data(), authorizationBytes.sizeInBytes());
    scratch[ProxyAuthorizationPrefixBytes + authorizationBytes.sizeInBytes()] = '\0';
    return httpClientLinuxAppendHeader(curl, headers, scratch.data());
}

static long getCurlHttpVersionOption(SC::HttpClientRequestProtocolOptions::Preference preference)
{
    switch (preference)
    {
    case SC::HttpClientRequestProtocolOptions::Default: return CURL_HTTP_VERSION_NONE;
    case SC::HttpClientRequestProtocolOptions::Http11Only: return CURL_HTTP_VERSION_1_1;
    case SC::HttpClientRequestProtocolOptions::Http2Preferred: return CURL_HTTP_VERSION_2TLS;
    case SC::HttpClientRequestProtocolOptions::Http2Required: return CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE;
    }
    return CURL_HTTP_VERSION_NONE;
}

static SC::Result getCurlStringPointer(SC::StringSpan source, SC::Span<char> storage, const char*& destination)
{
    if (source.isNullTerminated())
    {
        destination = source.bytesIncludingTerminator();
        return SC::Result(true);
    }

    const SC::Span<const char> sourceBytes = source.toCharSpan();
    if (storage.sizeInBytes() <= sourceBytes.sizeInBytes())
        return SC::Result::Error(SC::HttpClientResultCategory, SC::HttpClientError::BackendScratchTooSmall);
    memcpy(storage.data(), sourceBytes.data(), sourceBytes.sizeInBytes());
    storage[sourceBytes.sizeInBytes()] = '\0';
    destination                        = storage.data();
    return SC::Result(true);
}

struct HttpClientLinuxRuntimeFeatures
{
    bool tls   = false;
    bool http2 = false;
};

static const HttpClientLinuxRuntimeFeatures& getHttpClientLinuxRuntimeFeatures()
{
    static const HttpClientLinuxRuntimeFeatures features = []()
    {
        HttpClientLinuxRuntimeFeatures result;
        HttpClientLinuxLibCurlLoader   curl;
        if (not curl.init())
            return result;

        result.tls   = curl.supportsFeature(CURL_VERSION_SSL);
        result.http2 = curl.supportsFeature(CURL_VERSION_HTTP2);
        curl.close();
        return result;
    }();
    return features;
}
} // namespace

namespace SC
{
struct HttpClientLinuxCallbacks
{
    static HttpClientResponse::Protocol mapCurlHttpVersion(long version)
    {
        switch (version)
        {
        case CURL_HTTP_VERSION_1_0:
        case CURL_HTTP_VERSION_1_1: return HttpClientResponse::Protocol::Http11;
        case CURL_HTTP_VERSION_2_0:
        case CURL_HTTP_VERSION_2TLS:
        case CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE: return HttpClientResponse::Protocol::Http2;
        default: return HttpClientResponse::Protocol::Unknown;
        }
    }

    static bool isHttp2Required(HttpClientOperation& operation)
    {
        return operation.currentRequest.options.protocol.preference == HttpClientRequestProtocolOptions::Http2Required;
    }

    static void updateNegotiatedProtocol(HttpClientOperation& operation)
    {
        if (operation.currentResponse == nullptr)
        {
            return;
        }

        auto& session  = *reinterpret_cast<HttpClient::Internal*>(operation.client->storage);
        auto& internal = *reinterpret_cast<HttpClientOperation::Internal*>(operation.storage);

        long version = CURL_HTTP_VERSION_NONE;
        if (session.curl.curl_easy_getinfo(internal.curlHandle, CURLINFO_HTTP_VERSION, &version) == CURLE_OK)
        {
            const HttpClientResponse::Protocol protocol = mapCurlHttpVersion(version);
            if (protocol != HttpClientResponse::Protocol::Unknown)
            {
                operation.currentResponse->negotiatedProtocol = protocol;
            }
        }
    }

    static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata)
    {
        HttpClientOperation* operation = reinterpret_cast<HttpClientOperation*>(userdata);

        const size_t totalSize = size * nitems;
        if (operation == nullptr or operation->currentResponse == nullptr)
        {
            return 0;
        }

        HttpClientResponse& response    = *operation->currentResponse;
        const Result        appendError = appendResponseHeaderLine(response, buffer, totalSize);
        if (not appendError)
        {
            HttpClientOperation::Internal& internal =
                *reinterpret_cast<HttpClientOperation::Internal*>(operation->storage);
            internal.callbackError = appendError;
            return 0;
        }

        if (totalSize >= 5 and memcmp(buffer, "HTTP/", 5) == 0)
        {
            int code = 0;
            for (size_t idx = 0; idx + 3 < totalSize; ++idx)
            {
                if (buffer[idx] == ' ' and buffer[idx + 1] >= '0' and buffer[idx + 1] <= '9')
                {
                    for (size_t j = idx + 1; j < totalSize and buffer[j] >= '0' and buffer[j] <= '9'; ++j)
                    {
                        code = code * 10 + (buffer[j] - '0');
                    }
                    response.statusCode = code;
                    break;
                }
            }
            if (totalSize >= 6 and buffer[5] == '2')
            {
                response.negotiatedProtocol = HttpClientResponse::Protocol::Http2;
            }
            else if (totalSize >= 6 and buffer[5] == '1')
            {
                response.negotiatedProtocol = HttpClientResponse::Protocol::Http11;
            }
        }

        HttpClientOperation::Internal& internal = *reinterpret_cast<HttpClientOperation::Internal*>(operation->storage);
        if (totalSize == 2 and buffer[0] == '\r' and buffer[1] == '\n' and not internal.responseHeadSeen)
        {
            updateNegotiatedProtocol(*operation);
            if (isHttp2Required(*operation) and response.negotiatedProtocol != HttpClientResponse::Protocol::Http2)
            {
                internal.callbackError =
                    Result::Error(HttpClientResultCategory, HttpClientError::Http2RequiredNotNegotiated);
                return 0;
            }
            internal.responseHeadSeen = true;
            operation->enqueueResponseHead();
        }
        return totalSize;
    }

    static size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
    {
        HttpClientOperation* operation = reinterpret_cast<HttpClientOperation*>(userdata);
        const size_t         totalSize = size * nmemb;
        if (operation == nullptr or totalSize == 0)
        {
            return 0;
        }

        const Result enqueueRes = operation->enqueueResponseDataCopy({ptr, totalSize});
        if (not enqueueRes)
        {
            HttpClientOperation::Internal& internal =
                *reinterpret_cast<HttpClientOperation::Internal*>(operation->storage);
            internal.callbackError = enqueueRes;
            return 0;
        }
        return totalSize;
    }

    static size_t curlReadCallback(char* buffer, size_t size, size_t nitems, void* userdata)
    {
        HttpClientOperation* operation = reinterpret_cast<HttpClientOperation*>(userdata);
        if (operation == nullptr)
        {
            return CURL_READFUNC_ABORT;
        }

        Result error(true);
        bool   endReached = false;

        const size_t readBytes = operation->readRequestBodyChunk({buffer, size * nitems}, error, endReached);
        if (not error)
        {
            HttpClientOperation::Internal& internal =
                *reinterpret_cast<HttpClientOperation::Internal*>(operation->storage);
            internal.callbackError = error;
            return CURL_READFUNC_ABORT;
        }
        if (endReached)
        {
            return 0;
        }
        return readBytes;
    }

    static int curlProgressCallback(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
        HttpClientOperation* operation = reinterpret_cast<HttpClientOperation*>(clientp);
        if (operation == nullptr)
        {
            return 1;
        }
        HttpClientOperation::Internal& internal = *reinterpret_cast<HttpClientOperation::Internal*>(operation->storage);
        return internal.cancelRequested ? 1 : 0;
    }
};
} // namespace SC

SC::HttpClient::HttpClient()
{
    static_assert(sizeof(Internal) <= sizeof(storage), "Linux HttpClient storage too small");
    static_assert(alignof(Internal) <= alignof(uint64_t), "Linux HttpClient alignment mismatch");
    SC::placementNew(*reinterpret_cast<Internal*>(storage));
}

SC::HttpClient::~HttpClient()
{
    if (initialized)
    {
        (void)close();
    }
    reinterpret_cast<Internal*>(storage)->~Internal();
}

SC::Result SC::HttpClient::platformInit()
{
    Internal& internal = *reinterpret_cast<Internal*>(storage);
    if (not internal.curl.init())
        return Result::Error(HttpClientResultCategory, HttpClientError::RequiredBackendUnavailable);
    return Result(true);
}

SC::Result SC::HttpClient::platformClose()
{
    reinterpret_cast<Internal*>(storage)->curl.close();
    return Result(true);
}

SC::HttpClientOperation::HttpClientOperation()
{
    static_assert(sizeof(Internal) <= sizeof(storage), "Linux HttpClientOperation storage too small");
    static_assert(alignof(Internal) <= alignof(uint64_t), "Linux HttpClientOperation alignment mismatch");
    SC::placementNew(*reinterpret_cast<Internal*>(storage));
}

SC::HttpClientOperation::~HttpClientOperation()
{
    if (initialized)
    {
        (void)close();
    }
    reinterpret_cast<Internal*>(storage)->~Internal();
}

SC::Result SC::HttpClientOperation::platformInit()
{
    (void)getHttpClientLinuxRuntimeFeatures();
    auto& session       = *reinterpret_cast<HttpClient::Internal*>(client->storage);
    auto& internal      = *reinterpret_cast<Internal*>(storage);
    internal.curlHandle = session.curl.curl_easy_init();
    if (internal.curlHandle == nullptr)
        return Result::Error(HttpClientResultCategory, HttpClientError::RequestTaskUnavailable);
    return Result(true);
}

SC::Result SC::HttpClientOperation::platformClose()
{
    auto& session  = *reinterpret_cast<HttpClient::Internal*>(client->storage);
    auto& internal = *reinterpret_cast<Internal*>(storage);
    if (internal.workerThreadStarted)
    {
        (void)pthread_join(internal.workerThread, nullptr);
        internal.workerThreadStarted = false;
        internal.workerRunning       = false;
    }
    if (internal.requestHeaders != nullptr)
    {
        session.curl.curl_slist_free_all(internal.requestHeaders);
        internal.requestHeaders = nullptr;
    }
    if (internal.curlHandle != nullptr)
    {
        session.curl.curl_easy_cleanup(internal.curlHandle);
        internal.curlHandle = nullptr;
    }
    internal.cancelRequested  = false;
    internal.responseHeadSeen = false;
    internal.callbackError    = Result(true);
    return Result(true);
}

SC::Result SC::HttpClientOperation::platformCancel()
{
    reinterpret_cast<Internal*>(storage)->cancelRequested = true;
    return Result(true);
}

SC::Result SC::HttpClientOperation::platformStart()
{
    auto& session  = *reinterpret_cast<HttpClient::Internal*>(client->storage);
    auto& internal = *reinterpret_cast<Internal*>(storage);
    if (internal.workerRunning)
        return Result::Error(HttpClientResultCategory, HttpClientError::OperationRequestInFlight);

    internal.cancelRequested  = false;
    internal.responseHeadSeen = false;
    internal.callbackError    = Result(true);

    CURL* curlHandle = internal.curlHandle;
    if (curlHandle == nullptr)
        return Result::Error(HttpClientResultCategory, HttpClientError::RequestTaskUnavailable);

    session.curl.curl_easy_reset(curlHandle);

    // libcurl recommends disabling signal handlers for Unix multi-threaded callers.
    if (session.curl.curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L) != CURLE_OK)
        return Result::Error(HttpClientResultCategory, HttpClientError::TransportConfigurationFailed);

    Span<const char> urlSpan = currentRequest.url.toCharSpan();
    if (currentRequest.url.isNullTerminated())
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_URL, currentRequest.url.bytesIncludingTerminator());
    }
    else
    {
        if (backendScratch.sizeInBytes() <= urlSpan.sizeInBytes())
            return Result::Error(HttpClientResultCategory, HttpClientError::BackendScratchTooSmall);
        memcpy(backendScratch.data(), urlSpan.data(), urlSpan.sizeInBytes());
        backendScratch[urlSpan.sizeInBytes()] = '\0';
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_URL, backendScratch.data());
    }

    if (currentRequest.method == HttpClientRequest::HttpPOST)
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_POST, 1L);
    }
    else if (currentRequest.method == HttpClientRequest::HttpHEAD)
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_NOBODY, 1L);
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, getCustomMethod(currentRequest.method));
    }
    else if (currentRequest.method != HttpClientRequest::HttpGET)
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_CUSTOMREQUEST, getCustomMethod(currentRequest.method));
    }

    session.curl.curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, isAutomaticRedirectEnabled() ? 1L : 0L);
    session.curl.curl_easy_setopt(curlHandle, CURLOPT_MAXREDIRS,
                                  static_cast<long>(currentRequest.options.redirect.maxRedirects));
    if (currentRequest.options.timeouts.requestTimeoutMs > 0)
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT_MS,
                                      static_cast<long>(currentRequest.options.timeouts.requestTimeoutMs));
    }

    const long httpVersion = getCurlHttpVersionOption(currentRequest.options.protocol.preference);
    if (httpVersion != CURL_HTTP_VERSION_NONE)
    {
        if (session.curl.curl_easy_setopt(curlHandle, CURLOPT_HTTP_VERSION, httpVersion) != CURLE_OK)
        {
            switch (currentRequest.options.protocol.preference)
            {
            case HttpClientRequestProtocolOptions::Http11Only:
                return Result::Error(HttpClientResultCategory, HttpClientError::Http11OnlyUnsupported);
            case HttpClientRequestProtocolOptions::Http2Preferred:
                return Result::Error(HttpClientResultCategory, HttpClientError::Http2PreferredUnsupported);
            case HttpClientRequestProtocolOptions::Http2Required:
                return Result::Error(HttpClientResultCategory, HttpClientError::Http2RequiredUnsupported);
            case HttpClientRequestProtocolOptions::Default: break;
            }
            return Result::Error(HttpClientResultCategory, HttpClientError::TransportConfigurationFailed);
        }
    }

    if (currentRequest.options.proxy.mode == HttpClientRequestProxyOptions::NoProxy)
    {
        if (session.curl.curl_easy_setopt(curlHandle, CURLOPT_PROXY, "") != CURLE_OK)
            return Result::Error(HttpClientResultCategory, HttpClientError::NoProxyPolicyUnsupported);
    }
    else if (currentRequest.options.proxy.mode == HttpClientRequestProxyOptions::Http)
    {
        const char* proxyUrl = nullptr;
        SC_TRY(getCurlStringPointer(currentRequest.options.proxy.url, backendScratch, proxyUrl));
        if (session.curl.curl_easy_setopt(curlHandle, CURLOPT_PROXY, proxyUrl) != CURLE_OK)
            return Result::Error(HttpClientResultCategory, HttpClientError::HttpProxyPolicyUnsupported);
        if (currentRequest.options.proxy.bypassList.sizeInBytes() > 0)
        {
            const char* noProxy = nullptr;
            SC_TRY(getCurlStringPointer(currentRequest.options.proxy.bypassList, backendScratch, noProxy));
            if (session.curl.curl_easy_setopt(curlHandle, CURLOPT_NOPROXY, noProxy) != CURLE_OK)
                return Result::Error(HttpClientResultCategory, HttpClientError::ProxyBypassListUnsupported);
        }
    }

    if (not currentRequest.options.tls.verifyPeer)
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, 0L);
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    if (currentRequest.options.tls.caCertificatesPath.sizeInBytes() > 0)
    {
        if (currentRequest.options.tls.caCertificatesPath.isNullTerminated())
        {
            session.curl.curl_easy_setopt(curlHandle, CURLOPT_CAINFO,
                                          currentRequest.options.tls.caCertificatesPath.bytesIncludingTerminator());
        }
        else
        {
            const Span<const char> caInfo = currentRequest.options.tls.caCertificatesPath.toCharSpan();
            if (backendScratch.sizeInBytes() <= caInfo.sizeInBytes())
                return Result::Error(HttpClientResultCategory, HttpClientError::BackendScratchTooSmall);
            memcpy(backendScratch.data(), caInfo.data(), caInfo.sizeInBytes());
            backendScratch[caInfo.sizeInBytes()] = '\0';
            session.curl.curl_easy_setopt(curlHandle, CURLOPT_CAINFO, backendScratch.data());
        }
    }

    session.curl.curl_easy_setopt(curlHandle, CURLOPT_NOPROGRESS, 0L);
    session.curl.curl_easy_setopt(curlHandle, CURLOPT_XFERINFOFUNCTION,
                                  &HttpClientLinuxCallbacks::curlProgressCallback);
    session.curl.curl_easy_setopt(curlHandle, CURLOPT_XFERINFODATA, this);

    session.curl.curl_easy_setopt(curlHandle, CURLOPT_HEADERFUNCTION, &HttpClientLinuxCallbacks::curlHeaderCallback);
    session.curl.curl_easy_setopt(curlHandle, CURLOPT_HEADERDATA, this);

    session.curl.curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, &HttpClientLinuxCallbacks::curlWriteCallback);
    session.curl.curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, this);

    if (internal.requestHeaders != nullptr)
    {
        session.curl.curl_slist_free_all(internal.requestHeaders);
        internal.requestHeaders = nullptr;
    }
    if (currentRequest.options.proxy.authorization.sizeInBytes() > 0)
    {
        SC_TRY(httpClientLinuxAppendProxyAuthorization(session.curl, internal.requestHeaders,
                                                       currentRequest.options.proxy.authorization, backendScratch));
    }
    for (size_t idx = 0; idx < currentRequest.headers.sizeInElements(); ++idx)
    {
        const size_t nameLen = currentRequest.headers[idx].name.sizeInBytes();
        const size_t valLen  = currentRequest.headers[idx].value.sizeInBytes();
        if (valLen == 0)
        {
            if (nameLen + 2 > backendScratch.sizeInBytes())
                return Result::Error(HttpClientResultCategory, HttpClientError::BackendScratchTooSmall);
            memcpy(backendScratch.data(), currentRequest.headers[idx].name.toCharSpan().data(), nameLen);
            backendScratch[nameLen]     = ';';
            backendScratch[nameLen + 1] = '\0';
            SC_TRY(httpClientLinuxAppendHeader(session.curl, internal.requestHeaders, backendScratch.data()));
            continue;
        }
        if (nameLen + valLen + 3 >= backendScratch.sizeInBytes())
            return Result::Error(HttpClientResultCategory, HttpClientError::BackendScratchTooSmall);
        memcpy(backendScratch.data(), currentRequest.headers[idx].name.toCharSpan().data(), nameLen);
        backendScratch[nameLen]     = ':';
        backendScratch[nameLen + 1] = ' ';
        memcpy(backendScratch.data() + nameLen + 2, currentRequest.headers[idx].value.toCharSpan().data(), valLen);
        backendScratch[nameLen + 2 + valLen] = '\0';
        SC_TRY(httpClientLinuxAppendHeader(session.curl, internal.requestHeaders, backendScratch.data()));
    }
    if (currentRequest.body.isChunkedStream())
    {
        SC_TRY(httpClientLinuxAppendHeader(session.curl, internal.requestHeaders, "Transfer-Encoding: chunked"));
    }
    if (internal.requestHeaders != nullptr)
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_HTTPHEADER, internal.requestHeaders);
    }

    if (currentRequest.body.isStreamed())
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_READFUNCTION, &HttpClientLinuxCallbacks::curlReadCallback);
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_READDATA, this);
        if (currentRequest.method == HttpClientRequest::HttpPOST)
        {
            session.curl.curl_easy_setopt(curlHandle, CURLOPT_POST, 1L);
            if (currentRequest.body.framing == HttpClientRequestBody::SizedStream)
            {
                if (currentRequest.body.sizeInBytes > static_cast<uint64_t>(LONG_MAX))
                    return Result::Error(HttpClientResultCategory, HttpClientError::RequestBodySizeUnsupported);
                session.curl.curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE,
                                              static_cast<long>(currentRequest.body.sizeInBytes));
            }
        }
        else
        {
            session.curl.curl_easy_setopt(curlHandle, CURLOPT_UPLOAD, 1L);
            if (currentRequest.body.framing == HttpClientRequestBody::SizedStream)
            {
                if (currentRequest.body.sizeInBytes > static_cast<uint64_t>(LONG_MAX))
                    return Result::Error(HttpClientResultCategory, HttpClientError::RequestBodySizeUnsupported);
                session.curl.curl_easy_setopt(curlHandle, CURLOPT_INFILESIZE,
                                              static_cast<long>(currentRequest.body.sizeInBytes));
            }
        }
    }
    else if (currentRequest.body.bytes.sizeInBytes() > 0)
    {
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, currentRequest.body.bytes.data());
        session.curl.curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDSIZE,
                                      static_cast<long>(currentRequest.body.bytes.sizeInBytes()));
        if (currentRequest.method == HttpClientRequest::HttpPOST)
        {
            session.curl.curl_easy_setopt(curlHandle, CURLOPT_POST, 1L);
        }
    }

    internal.workerRunning   = true;
    const int workerStartRes = pthread_create(
        &internal.workerThread, nullptr,
        [](void* parameter) -> void*
        {
            HttpClientOperation* operation = reinterpret_cast<HttpClientOperation*>(parameter);

            auto& sessionRef  = *reinterpret_cast<HttpClient::Internal*>(operation->client->storage);
            auto& internalRef = *reinterpret_cast<Internal*>(operation->storage);

            const int res = sessionRef.curl.curl_easy_perform(internalRef.curlHandle);

            if (res == CURLE_OK and not internalRef.responseHeadSeen and operation->currentResponse != nullptr)
            {
                long httpCode = 0;
                sessionRef.curl.curl_easy_getinfo(internalRef.curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);
                operation->currentResponse->statusCode = static_cast<int>(httpCode);
                HttpClientLinuxCallbacks::updateNegotiatedProtocol(*operation);
                long redirectCount = 0;
                (void)sessionRef.curl.curl_easy_getinfo(internalRef.curlHandle, CURLINFO_REDIRECT_COUNT,
                                                        &redirectCount);
                operation->currentResponse->redirectCount =
                    static_cast<uint32_t>(redirectCount < 0 ? 0 : redirectCount);
                char* effectiveUrl = nullptr;
                if (sessionRef.curl.curl_easy_getinfo(internalRef.curlHandle, CURLINFO_EFFECTIVE_URL, &effectiveUrl) ==
                        CURLE_OK and
                    effectiveUrl != nullptr)
                {
                    const Result effectiveUrlError = operation->copyResponseEffectiveUrl(
                        StringSpan::fromNullTerminated(effectiveUrl, operation->currentRequest.url.getEncoding()));
                    if (not effectiveUrlError)
                    {
                        operation->enqueueError(effectiveUrlError);
                        internalRef.workerRunning = false;
                        return nullptr;
                    }
                }
                if (HttpClientLinuxCallbacks::isHttp2Required(*operation) and
                    operation->currentResponse->negotiatedProtocol != HttpClientResponse::Protocol::Http2)
                {
                    operation->enqueueError(
                        Result::Error(HttpClientResultCategory, HttpClientError::Http2RequiredNotNegotiated));
                    internalRef.workerRunning = false;
                    return nullptr;
                }
                operation->enqueueResponseHead();
            }

            if (res != CURLE_OK)
            {
                if (not internalRef.callbackError)
                {
                    operation->enqueueError(internalRef.callbackError);
                }
                else if (internalRef.cancelRequested)
                {
                    operation->enqueueError(Result::Error(HttpClientResultCategory, HttpClientError::RequestCancelled));
                }
                else
                {
                    operation->enqueueError(Result::Error(HttpClientResultCategory, HttpClientError::TransportFailed));
                }
            }
            else
            {
                operation->enqueueResponseComplete();
            }
            internalRef.workerRunning = false;
            return nullptr;
        },
        this);
    if (workerStartRes != 0)
    {
        internal.workerRunning = false;
    }
    if (workerStartRes != 0)
        return Result::Error(HttpClientResultCategory, HttpClientError::RequestTaskUnavailable);
    internal.workerThreadStarted = true;

    return Result(true);
}
