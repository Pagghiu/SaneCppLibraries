// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// Linux backend for HttpClient using libcurl via dlopen.
#include "HttpClientLinuxAPI.h"

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
static void appendResponseHeaderLine(SC::HttpClientResponse& response, const char* data, size_t size)
{
    if (response.headersLength >= response.headers.sizeInBytes())
    {
        return;
    }
    const size_t remaining = response.headers.sizeInBytes() - response.headersLength;
    const size_t toCopy    = size < remaining ? size : remaining;
    memcpy(const_cast<char*>(response.headers.data()) + response.headersLength, data, toCopy);
    response.headersLength += toCopy;
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
} // namespace

namespace SC
{
struct HttpClientLinuxCallbacks
{
    static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata)
    {
        HttpClientOperation* operation = reinterpret_cast<HttpClientOperation*>(userdata);

        const size_t totalSize = size * nitems;
        if (operation == nullptr or operation->currentResponse == nullptr)
        {
            return 0;
        }

        HttpClientResponse& response = *operation->currentResponse;
        appendResponseHeaderLine(response, buffer, totalSize);

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
        }

        HttpClientOperation::Internal& internal = *reinterpret_cast<HttpClientOperation::Internal*>(operation->storage);
        if (totalSize == 2 and buffer[0] == '\r' and buffer[1] == '\n' and not internal.responseHeadSeen)
        {
            internal.responseHeadSeen   = true;
            response.negotiatedProtocol = HttpClientResponse::Protocol::Http11;
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

    static int curlProgressCallback(void* clientp, long, long, long, long)
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
    SC_TRY_MSG(internal.curl.init(), "HttpClient: failed to load libcurl");
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
    auto& session       = *reinterpret_cast<HttpClient::Internal*>(client->storage);
    auto& internal      = *reinterpret_cast<Internal*>(storage);
    internal.curlHandle = session.curl.curl_easy_init();
    SC_TRY_MSG(internal.curlHandle != nullptr, "HttpClient: curl_easy_init failed");
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
    SC_TRY_MSG(not internal.workerRunning, "HttpClient: request already in flight");

    internal.cancelRequested  = false;
    internal.responseHeadSeen = false;
    internal.callbackError    = Result(true);

    CURL* curlHandle = internal.curlHandle;
    SC_TRY_MSG(curlHandle != nullptr, "HttpClient: missing curl handle");

    session.curl.curl_easy_reset(curlHandle);

    Span<const char> urlSpan = currentRequest.url.toCharSpan();
    if (currentRequest.url.isNullTerminated())
    {
        session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_URL, currentRequest.url.bytesIncludingTerminator());
    }
    else
    {
        SC_TRY_MSG(backendScratch.sizeInBytes() > urlSpan.sizeInBytes(),
                   "HttpClient: backend scratch too small for URL");
        memcpy(backendScratch.data(), urlSpan.data(), urlSpan.sizeInBytes());
        backendScratch[urlSpan.sizeInBytes()] = '\0';
        session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_URL, backendScratch.data());
    }

    if (currentRequest.method == HttpClientRequest::HttpPOST)
    {
        session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_POST, 1L);
    }
    else if (currentRequest.method == HttpClientRequest::HttpHEAD)
    {
        session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_NOBODY, 1L);
        session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_CUSTOMREQUEST, getCustomMethod(currentRequest.method));
    }
    else if (currentRequest.method != HttpClientRequest::HttpGET)
    {
        session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_CUSTOMREQUEST, getCustomMethod(currentRequest.method));
    }

    session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_FOLLOWLOCATION, isAutomaticRedirectEnabled() ? 1L : 0L);
    session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_MAXREDIRS, currentRequest.options.redirect.maxRedirects);
    if (currentRequest.options.timeouts.requestTimeoutMs > 0)
    {
        session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_TIMEOUT_MS,
                                           static_cast<long>(currentRequest.options.timeouts.requestTimeoutMs));
    }

    if (not currentRequest.options.tls.verifyPeer)
    {
        session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_SSL_VERIFYPEER, 0L);
        session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    if (currentRequest.options.tls.caCertificatesPath.sizeInBytes() > 0)
    {
        if (currentRequest.options.tls.caCertificatesPath.isNullTerminated())
        {
            session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_CAINFO,
                                              currentRequest.options.tls.caCertificatesPath.bytesIncludingTerminator());
        }
        else
        {
            const Span<const char> caInfo = currentRequest.options.tls.caCertificatesPath.toCharSpan();
            SC_TRY_MSG(backendScratch.sizeInBytes() > caInfo.sizeInBytes(),
                       "HttpClient: backend scratch too small for CA path");
            memcpy(backendScratch.data(), caInfo.data(), caInfo.sizeInBytes());
            backendScratch[caInfo.sizeInBytes()] = '\0';
            session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_CAINFO, backendScratch.data());
        }
    }

    session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_NOPROGRESS, 0L);
    session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_XFERINFOFUNCTION,
                                      reinterpret_cast<void*>(&HttpClientLinuxCallbacks::curlProgressCallback));
    session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_XFERINFODATA, this);

    session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_HEADERFUNCTION,
                                      reinterpret_cast<void*>(&HttpClientLinuxCallbacks::curlHeaderCallback));
    session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_HEADERDATA, this);

    session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_WRITEFUNCTION,
                                      reinterpret_cast<void*>(&HttpClientLinuxCallbacks::curlWriteCallback));
    session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_WRITEDATA, this);

    if (internal.requestHeaders != nullptr)
    {
        session.curl.curl_slist_free_all(internal.requestHeaders);
        internal.requestHeaders = nullptr;
    }
    for (size_t idx = 0; idx < currentRequest.headers.sizeInElements(); ++idx)
    {
        const size_t nameLen = currentRequest.headers[idx].name.sizeInBytes();
        const size_t valLen  = currentRequest.headers[idx].value.sizeInBytes();
        SC_TRY_MSG(nameLen + valLen + 3 < backendScratch.sizeInBytes(),
                   "HttpClient: backend scratch too small for headers");
        memcpy(backendScratch.data(), currentRequest.headers[idx].name.toCharSpan().data(), nameLen);
        backendScratch[nameLen]     = ':';
        backendScratch[nameLen + 1] = ' ';
        memcpy(backendScratch.data() + nameLen + 2, currentRequest.headers[idx].value.toCharSpan().data(), valLen);
        backendScratch[nameLen + 2 + valLen] = '\0';
        internal.requestHeaders = session.curl.curl_slist_append(internal.requestHeaders, backendScratch.data());
    }
    if (internal.requestHeaders != nullptr)
    {
        session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_HTTPHEADER, internal.requestHeaders);
    }

    if (currentRequest.body.isStreamed())
    {
        SC_TRY_MSG(currentRequest.body.sizeInBytes <= static_cast<uint64_t>(LONG_MAX),
                   "HttpClient: streamed body too large for libcurl");
        session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_POSTFIELDSIZE,
                                           static_cast<long>(currentRequest.body.sizeInBytes));
        session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_READFUNCTION,
                                          reinterpret_cast<void*>(&HttpClientLinuxCallbacks::curlReadCallback));
        session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_READDATA, this);
        if (currentRequest.method == HttpClientRequest::HttpPOST)
        {
            session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_POST, 1L);
        }
    }
    else if (currentRequest.body.bytes.sizeInBytes() > 0)
    {
        session.curl.curl_easy_setopt_ptr(curlHandle, CURLOPT_POSTFIELDS, currentRequest.body.bytes.data());
        session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_POSTFIELDSIZE,
                                           static_cast<long>(currentRequest.body.bytes.sizeInBytes()));
        if (currentRequest.method == HttpClientRequest::HttpPOST)
        {
            session.curl.curl_easy_setopt_long(curlHandle, CURLOPT_POST, 1L);
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

            if (not internalRef.responseHeadSeen and operation->currentResponse != nullptr)
            {
                long httpCode = 0;
                sessionRef.curl.curl_easy_getinfo_long(internalRef.curlHandle, CURLINFO_RESPONSE_CODE, &httpCode);
                operation->currentResponse->statusCode         = static_cast<int>(httpCode);
                operation->currentResponse->negotiatedProtocol = HttpClientResponse::Protocol::Http11;
                long redirectCount                             = 0;
                (void)sessionRef.curl.curl_easy_getinfo_long(internalRef.curlHandle, CURLINFO_REDIRECT_COUNT,
                                                             &redirectCount);
                operation->currentResponse->redirectCount =
                    static_cast<uint32_t>(redirectCount < 0 ? 0 : redirectCount);
                char* effectiveUrl = nullptr;
                if (sessionRef.curl.curl_easy_getinfo_ptr(internalRef.curlHandle, CURLINFO_EFFECTIVE_URL,
                                                          &effectiveUrl) == CURLE_OK and
                    effectiveUrl != nullptr)
                {
                    (void)operation->copyResponseEffectiveUrl(
                        StringSpan::fromNullTerminated(effectiveUrl, operation->currentRequest.url.getEncoding()));
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
                    operation->enqueueError(Result::Error("HttpClient: request cancelled"));
                }
                else
                {
                    operation->enqueueError(Result::Error("HttpClient: curl_easy_perform failed"));
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
    SC_TRY_MSG(workerStartRes == 0, "HttpClient: pthread_create failed");
    internal.workerThreadStarted = true;

    return Result(true);
}
