// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// Windows backend for HttpClient using WinHTTP.

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#pragma comment(lib, "winhttp.lib")
#endif

#ifndef WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL
#define WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL 133
#endif
#ifndef WINHTTP_OPTION_HTTP_PROTOCOL_USED
#define WINHTTP_OPTION_HTTP_PROTOCOL_USED 134
#endif
#ifndef WINHTTP_PROTOCOL_FLAG_HTTP2
#define WINHTTP_PROTOCOL_FLAG_HTTP2 0x1
#endif

struct SC::HttpClient::Internal
{
    HINTERNET hSession = NULL;
};

struct SC::HttpClientOperation::Internal
{
    HANDLE workerThread = NULL;

    bool          workerRunning   = false;
    volatile LONG cancelRequested = 0;

    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;

    HttpClientLocalMutex handlesMutex;
};

namespace
{
static const wchar_t* getVerb(SC::HttpClientRequest::Method method)
{
    switch (method)
    {
    case SC::HttpClientRequest::HttpGET: return L"GET";
    case SC::HttpClientRequest::HttpPOST: return L"POST";
    case SC::HttpClientRequest::HttpPUT: return L"PUT";
    case SC::HttpClientRequest::HttpHEAD: return L"HEAD";
    case SC::HttpClientRequest::HttpDELETE: return L"DELETE";
    case SC::HttpClientRequest::HttpPATCH: return L"PATCH";
    case SC::HttpClientRequest::HttpOPTIONS: return L"OPTIONS";
    }
    return L"GET";
}

static void setCancelRequested(SC::HttpClientOperation::Internal& internal, bool requested)
{
    (void)InterlockedExchange(&internal.cancelRequested, requested ? 1 : 0);
}

static bool isCancelRequested(const SC::HttpClientOperation::Internal& internal)
{
    return InterlockedCompareExchange(const_cast<volatile LONG*>(&internal.cancelRequested), 0, 0) != 0;
}

static bool isHttp2Required(const SC::HttpClientRequest& request)
{
    return request.options.protocol.preference == SC::HttpClientRequestProtocolOptions::Http2Required;
}

static size_t writeChunkHeader(size_t value, char* destination, size_t destinationSize)
{
    char   reversed[sizeof(size_t) * 2];
    size_t numDigits = 0;
    do
    {
        const size_t digit    = value & 0x0F;
        reversed[numDigits++] = static_cast<char>(digit < 10 ? ('0' + digit) : ('a' + static_cast<char>(digit - 10)));
        value >>= 4;
    } while (value != 0 and numDigits < sizeof(reversed));

    if (numDigits + 2 > destinationSize)
    {
        return 0;
    }
    for (size_t idx = 0; idx < numDigits; ++idx)
    {
        destination[idx] = reversed[numDigits - 1 - idx];
    }
    destination[numDigits]     = '\r';
    destination[numDigits + 1] = '\n';
    return numDigits + 2;
}

static bool convertStringSpanToWide(SC::StringSpan source, wchar_t* destination, size_t destinationCapacity,
                                    int& length)
{
    if (destinationCapacity == 0)
    {
        return false;
    }

    const SC::Span<const char> sourceBytes = source.toCharSpan();
    length = MultiByteToWideChar(CP_UTF8, 0, sourceBytes.data(), static_cast<int>(sourceBytes.sizeInBytes()),
                                 destination, static_cast<int>(destinationCapacity - 1));
    if (length <= 0)
    {
        return false;
    }
    destination[length] = L'\0';
    return true;
}

static void replaceWideCharacters(wchar_t* text, int length, wchar_t from, wchar_t to)
{
    for (int idx = 0; idx < length; ++idx)
    {
        if (text[idx] == from)
        {
            text[idx] = to;
        }
    }
}
} // namespace

SC::HttpClient::HttpClient()
{
    static_assert(sizeof(Internal) <= sizeof(storage), "Windows HttpClient storage too small");
    static_assert(alignof(Internal) <= alignof(uint64_t), "Windows HttpClient alignment mismatch");
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
    auto& internal    = *reinterpret_cast<Internal*>(storage);
    internal.hSession = WinHttpOpen(L"SaneCppHttpClient/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    SC_TRY_MSG(internal.hSession != NULL, "HttpClient: WinHttpOpen failed");
    return Result(true);
}

SC::Result SC::HttpClient::platformClose()
{
    auto& internal = *reinterpret_cast<Internal*>(storage);
    if (internal.hSession != NULL)
    {
        WinHttpCloseHandle(internal.hSession);
        internal.hSession = NULL;
    }
    return Result(true);
}

SC::HttpClientOperation::HttpClientOperation()
{
    static_assert(sizeof(Internal) <= sizeof(storage), "Windows HttpClientOperation storage too small");
    static_assert(alignof(Internal) <= alignof(uint64_t), "Windows HttpClientOperation alignment mismatch");
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

SC::Result SC::HttpClientOperation::platformInit() { return Result(true); }

SC::Result SC::HttpClientOperation::platformClose()
{
    auto& internal = *reinterpret_cast<Internal*>(storage);
    setCancelRequested(internal, true);

    internal.handlesMutex.lock();
    if (internal.hRequest != NULL)
    {
        WinHttpCloseHandle(internal.hRequest);
        internal.hRequest = NULL;
    }
    if (internal.hConnect != NULL)
    {
        WinHttpCloseHandle(internal.hConnect);
        internal.hConnect = NULL;
    }
    internal.handlesMutex.unlock();

    if (internal.workerThread != NULL)
    {
        (void)WaitForSingleObject(internal.workerThread, INFINITE);
        (void)CloseHandle(internal.workerThread);
        internal.workerThread  = NULL;
        internal.workerRunning = false;
    }
    setCancelRequested(internal, false);
    return Result(true);
}

SC::Result SC::HttpClientOperation::platformCancel()
{
    auto& internal = *reinterpret_cast<Internal*>(storage);
    setCancelRequested(internal, true);
    if (internal.workerThread != NULL)
    {
        (void)CancelSynchronousIo(internal.workerThread);
    }
    internal.handlesMutex.lock();
    if (internal.hRequest != NULL)
    {
        WinHttpCloseHandle(internal.hRequest);
        internal.hRequest = NULL;
    }
    if (internal.hConnect != NULL)
    {
        WinHttpCloseHandle(internal.hConnect);
        internal.hConnect = NULL;
    }
    internal.handlesMutex.unlock();
    return Result(true);
}

SC::Result SC::HttpClientOperation::platformStart()
{
    auto& session  = *reinterpret_cast<HttpClient::Internal*>(client->storage);
    auto& internal = *reinterpret_cast<Internal*>(storage);

    HINTERNET sessionHandle = session.hSession;
    SC_TRY_MSG(sessionHandle != NULL, "HttpClient: missing WinHTTP session");
    SC_TRY_MSG(not internal.workerRunning, "HttpClient: request already in flight");

    setCancelRequested(internal, false);
    internal.workerRunning = true;

    internal.workerThread = CreateThread(
        NULL, 0,
        [](LPVOID parameter) -> DWORD
        {
            HttpClientOperation* operation = reinterpret_cast<HttpClientOperation*>(parameter);

            auto& internalRef = *reinterpret_cast<Internal*>(operation->storage);
            auto& session     = *reinterpret_cast<HttpClient::Internal*>(operation->client->storage);

            HINTERNET sessionHandle = session.hSession;

            auto closeHandles = [&internalRef]()
            {
                internalRef.handlesMutex.lock();
                if (internalRef.hRequest != NULL)
                {
                    WinHttpCloseHandle(internalRef.hRequest);
                    internalRef.hRequest = NULL;
                }
                if (internalRef.hConnect != NULL)
                {
                    WinHttpCloseHandle(internalRef.hConnect);
                    internalRef.hConnect = NULL;
                }
                internalRef.handlesMutex.unlock();
            };

            auto finishIfCancelled = [&]() -> bool
            {
                if (not isCancelRequested(internalRef))
                {
                    return false;
                }
                closeHandles();
                operation->enqueueError(Result::Error("HttpClient: request cancelled"));
                internalRef.workerRunning = false;
                return true;
            };

            URL_COMPONENTS urlComp   = {};
            urlComp.dwStructSize     = sizeof(urlComp);
            urlComp.dwSchemeLength   = static_cast<DWORD>(-1);
            urlComp.dwHostNameLength = static_cast<DWORD>(-1);
            urlComp.dwUrlPathLength  = static_cast<DWORD>(-1);

            wchar_t*     wideScratch = reinterpret_cast<wchar_t*>(operation->backendScratch.data());
            const size_t wideCap     = operation->backendScratch.sizeInBytes() / sizeof(wchar_t);
            if (wideCap == 0)
            {
                operation->enqueueError(Result::Error("HttpClient: backend scratch too small"));
                internalRef.workerRunning = false;
                return 0;
            }

            const int urlLen = MultiByteToWideChar(CP_UTF8, 0, operation->currentRequest.url.toCharSpan().data(),
                                                   static_cast<int>(operation->currentRequest.url.sizeInBytes()),
                                                   wideScratch, static_cast<int>(wideCap - 1));
            if (urlLen <= 0)
            {
                operation->enqueueError(Result::Error("HttpClient: URL conversion failed"));
                internalRef.workerRunning = false;
                return 0;
            }
            wideScratch[urlLen] = L'\0';

            if (not WinHttpCrackUrl(wideScratch, static_cast<DWORD>(urlLen), 0, &urlComp))
            {
                operation->enqueueError(Result::Error("HttpClient: WinHttpCrackUrl failed"));
                internalRef.workerRunning = false;
                return 0;
            }

            if (finishIfCancelled())
            {
                return 0;
            }

            const wchar_t savedHost                        = urlComp.lpszHostName[urlComp.dwHostNameLength];
            urlComp.lpszHostName[urlComp.dwHostNameLength] = L'\0';

            internalRef.handlesMutex.lock();
            internalRef.hConnect = WinHttpConnect(sessionHandle, urlComp.lpszHostName, urlComp.nPort, 0);
            internalRef.handlesMutex.unlock();
            urlComp.lpszHostName[urlComp.dwHostNameLength] = savedHost;

            if (internalRef.hConnect == NULL)
            {
                operation->enqueueError(Result::Error("HttpClient: WinHttpConnect failed"));
                internalRef.workerRunning = false;
                return 0;
            }

            if (finishIfCancelled())
            {
                return 0;
            }

            const wchar_t savedPath                      = urlComp.lpszUrlPath[urlComp.dwUrlPathLength];
            urlComp.lpszUrlPath[urlComp.dwUrlPathLength] = L'\0';

            DWORD flags = 0;
            if (urlComp.nScheme == INTERNET_SCHEME_HTTPS)
            {
                flags |= WINHTTP_FLAG_SECURE;
            }

            internalRef.handlesMutex.lock();
            internalRef.hRequest =
                WinHttpOpenRequest(internalRef.hConnect, getVerb(operation->currentRequest.method), urlComp.lpszUrlPath,
                                   NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            internalRef.handlesMutex.unlock();
            urlComp.lpszUrlPath[urlComp.dwUrlPathLength] = savedPath;

            if (internalRef.hRequest == NULL)
            {
                operation->enqueueError(Result::Error("HttpClient: WinHttpOpenRequest failed"));
                internalRef.handlesMutex.lock();
                WinHttpCloseHandle(internalRef.hConnect);
                internalRef.hConnect = NULL;
                internalRef.handlesMutex.unlock();
                internalRef.workerRunning = false;
                return 0;
            }

            if (finishIfCancelled())
            {
                return 0;
            }

            if (operation->currentRequest.options.timeouts.requestTimeoutMs > 0)
            {
                const int timeoutMs = static_cast<int>(operation->currentRequest.options.timeouts.requestTimeoutMs);
                WinHttpSetTimeouts(internalRef.hRequest, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
            }

            DWORD redirectPolicy = operation->isAutomaticRedirectEnabled() ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS
                                                                           : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            (void)WinHttpSetOption(internalRef.hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy,
                                   sizeof(redirectPolicy));
            DWORD maxRedirects = operation->currentRequest.options.redirect.maxRedirects;
            (void)WinHttpSetOption(internalRef.hRequest, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &maxRedirects,
                                   sizeof(maxRedirects));

            if (operation->currentRequest.options.proxy.mode != HttpClientRequestProxyOptions::Default)
            {
                WINHTTP_PROXY_INFO proxyInfo = {};
                proxyInfo.dwAccessType       = WINHTTP_ACCESS_TYPE_NO_PROXY;
                proxyInfo.lpszProxy          = WINHTTP_NO_PROXY_NAME;
                proxyInfo.lpszProxyBypass    = WINHTTP_NO_PROXY_BYPASS;

                if (operation->currentRequest.options.proxy.mode == HttpClientRequestProxyOptions::Http)
                {
                    static constexpr size_t HttpProxySchemeBytes = sizeof("http://") - 1;

                    const Span<const char> proxyUrl = operation->currentRequest.options.proxy.url.toCharSpan();
                    int                    proxyLen = 0;
                    if (not convertStringSpanToWide(
                            StringSpan(
                                {proxyUrl.data() + HttpProxySchemeBytes, proxyUrl.sizeInBytes() - HttpProxySchemeBytes},
                                false, operation->currentRequest.options.proxy.url.getEncoding()),
                            wideScratch, wideCap, proxyLen))
                    {
                        operation->enqueueError(Result::Error("HttpClient: proxy URL conversion failed"));
                        internalRef.workerRunning = false;
                        return 0;
                    }

                    wchar_t* bypassScratch = nullptr;
                    if (operation->currentRequest.options.proxy.bypassList.sizeInBytes() > 0)
                    {
                        const size_t bypassOffset = static_cast<size_t>(proxyLen) + 1;
                        if (bypassOffset >= wideCap)
                        {
                            operation->enqueueError(Result::Error("HttpClient: backend scratch too small for proxy "
                                                                  "bypass list"));
                            internalRef.workerRunning = false;
                            return 0;
                        }
                        bypassScratch = wideScratch + bypassOffset;
                        int bypassLen = 0;
                        if (not convertStringSpanToWide(operation->currentRequest.options.proxy.bypassList,
                                                        bypassScratch, wideCap - bypassOffset, bypassLen))
                        {
                            operation->enqueueError(Result::Error("HttpClient: proxy bypass list conversion failed"));
                            internalRef.workerRunning = false;
                            return 0;
                        }
                        replaceWideCharacters(bypassScratch, bypassLen, L',', L';');
                    }

                    proxyInfo.dwAccessType    = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
                    proxyInfo.lpszProxy       = wideScratch;
                    proxyInfo.lpszProxyBypass = bypassScratch != nullptr ? bypassScratch : WINHTTP_NO_PROXY_BYPASS;
                }

                if (not WinHttpSetOption(internalRef.hRequest, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo)))
                {
                    operation->enqueueError(Result::Error("HttpClient: WinHTTP proxy configuration not supported"));
                    internalRef.workerRunning = false;
                    return 0;
                }
            }

            if (operation->currentRequest.options.protocol.preference != HttpClientRequestProtocolOptions::Default)
            {
                DWORD enabledProtocols = 0;
                if (operation->currentRequest.options.protocol.preference ==
                        HttpClientRequestProtocolOptions::Http2Preferred or
                    operation->currentRequest.options.protocol.preference ==
                        HttpClientRequestProtocolOptions::Http2Required)
                {
                    enabledProtocols = WINHTTP_PROTOCOL_FLAG_HTTP2;
                }

                if (not WinHttpSetOption(internalRef.hRequest, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &enabledProtocols,
                                         sizeof(enabledProtocols)))
                {
                    operation->enqueueError(Result::Error("HttpClient: WinHTTP protocol preference not supported"));
                    internalRef.workerRunning = false;
                    return 0;
                }
            }

            if (operation->currentRequest.options.tls.caCertificatesPath.sizeInBytes() > 0)
            {
                operation->enqueueError(Result::Error("HttpClient: WinHTTP custom CA path not supported"));
                internalRef.workerRunning = false;
                return 0;
            }
            if (not operation->currentRequest.options.tls.verifyPeer)
            {
                DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                                      SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                (void)WinHttpSetOption(internalRef.hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &securityFlags,
                                       sizeof(securityFlags));
            }

            wchar_t*     headerLine = wideScratch;
            const size_t headerCap  = wideCap;
            if (operation->currentRequest.options.proxy.authorization.sizeInBytes() > 0)
            {
                static const wchar_t ProxyAuthorizationPrefix[] = L"Proxy-Authorization: ";
                static const size_t  ProxyAuthorizationPrefixLength =
                    (sizeof(ProxyAuthorizationPrefix) / sizeof(ProxyAuthorizationPrefix[0])) - 1;

                if (ProxyAuthorizationPrefixLength >= headerCap)
                {
                    operation->enqueueError(Result::Error("HttpClient: backend scratch too small for proxy "
                                                          "authorization"));
                    internalRef.workerRunning = false;
                    return 0;
                }
                memcpy(headerLine, ProxyAuthorizationPrefix, sizeof(ProxyAuthorizationPrefix) - sizeof(wchar_t));
                int authorizationLen = 0;
                if (not convertStringSpanToWide(operation->currentRequest.options.proxy.authorization,
                                                headerLine + ProxyAuthorizationPrefixLength,
                                                headerCap - ProxyAuthorizationPrefixLength, authorizationLen))
                {
                    operation->enqueueError(Result::Error("HttpClient: proxy authorization conversion failed"));
                    internalRef.workerRunning = false;
                    return 0;
                }
                if (not WinHttpAddRequestHeaders(internalRef.hRequest, headerLine, static_cast<DWORD>(-1L),
                                                 WINHTTP_ADDREQ_FLAG_ADD))
                {
                    operation->enqueueError(Result::Error("HttpClient: WinHttpAddRequestHeaders failed"));
                    internalRef.workerRunning = false;
                    return 0;
                }
            }
            for (size_t idx = 0; idx < operation->currentRequest.headers.sizeInElements(); ++idx)
            {
                const Span<const char> headerName = operation->currentRequest.headers[idx].name.toCharSpan();
                const int              nameLen =
                    MultiByteToWideChar(CP_UTF8, 0, headerName.data(), static_cast<int>(headerName.sizeInBytes()),
                                        headerLine, static_cast<int>(headerCap));
                if (nameLen <= 0)
                {
                    operation->enqueueError(Result::Error("HttpClient: request header name conversion failed"));
                    internalRef.workerRunning = false;
                    return 0;
                }
                if (static_cast<size_t>(nameLen + 3) >= headerCap)
                {
                    operation->enqueueError(Result::Error("HttpClient: backend scratch too small for request header"));
                    internalRef.workerRunning = false;
                    return 0;
                }
                headerLine[nameLen]     = L':';
                headerLine[nameLen + 1] = L' ';

                int                    valueLen    = 0;
                const Span<const char> headerValue = operation->currentRequest.headers[idx].value.toCharSpan();
                if (headerValue.sizeInBytes() > 0)
                {
                    const size_t valueCap = headerCap - static_cast<size_t>(nameLen) - 2;
                    if (valueCap <= 1)
                    {
                        operation->enqueueError(Result::Error("HttpClient: backend scratch too small for request "
                                                              "header"));
                        internalRef.workerRunning = false;
                        return 0;
                    }
                    valueLen =
                        MultiByteToWideChar(CP_UTF8, 0, headerValue.data(), static_cast<int>(headerValue.sizeInBytes()),
                                            headerLine + nameLen + 2, static_cast<int>(valueCap - 1));
                    if (valueLen <= 0)
                    {
                        operation->enqueueError(Result::Error("HttpClient: request header value conversion failed"));
                        internalRef.workerRunning = false;
                        return 0;
                    }
                }
                headerLine[nameLen + 2 + valueLen] = L'\0';
                if (not WinHttpAddRequestHeaders(internalRef.hRequest, headerLine, static_cast<DWORD>(-1L),
                                                 WINHTTP_ADDREQ_FLAG_ADD))
                {
                    operation->enqueueError(Result::Error("HttpClient: WinHttpAddRequestHeaders failed"));
                    internalRef.workerRunning = false;
                    return 0;
                }
            }
            if (operation->currentRequest.body.isChunkedStream())
            {
                if (not WinHttpAddRequestHeaders(internalRef.hRequest, L"Transfer-Encoding: chunked\r\n",
                                                 static_cast<DWORD>(-1L), WINHTTP_ADDREQ_FLAG_ADD))
                {
                    operation->enqueueError(Result::Error("HttpClient: WinHttpAddRequestHeaders failed"));
                    internalRef.workerRunning = false;
                    return 0;
                }
            }

            if (finishIfCancelled())
            {
                return 0;
            }

            DWORD requestBodyLength = static_cast<DWORD>(operation->currentRequest.body.bytes.sizeInBytes());
            if (operation->currentRequest.body.isChunkedStream())
            {
                requestBodyLength = WINHTTP_IGNORE_REQUEST_TOTAL_LENGTH;
            }
            else if (operation->currentRequest.body.isStreamed())
            {
                if (operation->currentRequest.body.sizeInBytes > 0xFFFFFFFFu)
                {
                    operation->enqueueError(Result::Error("HttpClient: streamed body too large for WinHTTP"));
                    internalRef.workerRunning = false;
                    return 0;
                }
                requestBodyLength = static_cast<DWORD>(operation->currentRequest.body.sizeInBytes);
            }

            const BOOL sent = WinHttpSendRequest(internalRef.hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                                 operation->currentRequest.body.bytes.sizeInBytes() > 0
                                                     ? const_cast<char*>(operation->currentRequest.body.bytes.data())
                                                     : WINHTTP_NO_REQUEST_DATA,
                                                 operation->currentRequest.body.isStreamed() ? 0 : requestBodyLength,
                                                 requestBodyLength, 0);
            if (not sent)
            {
                operation->enqueueError(Result::Error("HttpClient: WinHttpSendRequest failed"));
                internalRef.workerRunning = false;
                return 0;
            }

            if (operation->currentRequest.body.isStreamed())
            {
                static constexpr size_t ChunkHeaderBytes = 32;

                char*  chunkHeader = reinterpret_cast<char*>(operation->backendScratch.data());
                char*  chunkData   = chunkHeader;
                size_t chunkSize   = operation->backendScratch.sizeInBytes();
                if (operation->currentRequest.body.isChunkedStream())
                {
                    if (chunkSize <= ChunkHeaderBytes)
                    {
                        operation->enqueueError(Result::Error("HttpClient: backend scratch too small for upload"));
                        internalRef.workerRunning = false;
                        return 0;
                    }
                    chunkData = chunkHeader + ChunkHeaderBytes;
                    chunkSize -= ChunkHeaderBytes;
                }
                if (chunkSize == 0)
                {
                    operation->enqueueError(Result::Error("HttpClient: backend scratch too small for upload"));
                    internalRef.workerRunning = false;
                    return 0;
                }

                bool   endReached = false;
                Result bodyError(true);
                while (not endReached)
                {
                    const size_t bytesToWrite =
                        operation->readRequestBodyChunk({chunkData, chunkSize}, bodyError, endReached);
                    if (not bodyError)
                    {
                        operation->enqueueError(bodyError);
                        internalRef.workerRunning = false;
                        return 0;
                    }
                    if (bytesToWrite > 0)
                    {
                        DWORD written = 0;
                        if (operation->currentRequest.body.isChunkedStream())
                        {
                            const size_t headerSize = writeChunkHeader(bytesToWrite, chunkHeader, ChunkHeaderBytes);
                            if (headerSize == 0 or not WinHttpWriteData(internalRef.hRequest, chunkHeader,
                                                                        static_cast<DWORD>(headerSize), &written))
                            {
                                operation->enqueueError(Result::Error("HttpClient: WinHttpWriteData chunk header "
                                                                      "failed"));
                                internalRef.workerRunning = false;
                                return 0;
                            }
                        }
                        if (not WinHttpWriteData(internalRef.hRequest, chunkData, static_cast<DWORD>(bytesToWrite),
                                                 &written))
                        {
                            operation->enqueueError(Result::Error("HttpClient: WinHttpWriteData payload failed"));
                            internalRef.workerRunning = false;
                            return 0;
                        }
                        if (operation->currentRequest.body.isChunkedStream())
                        {
                            if (not WinHttpWriteData(internalRef.hRequest, "\r\n", 2, &written))
                            {
                                operation->enqueueError(Result::Error("HttpClient: WinHttpWriteData chunk terminator "
                                                                      "failed"));
                                internalRef.workerRunning = false;
                                return 0;
                            }
                        }
                    }
                }
                if (operation->currentRequest.body.isChunkedStream())
                {
                    DWORD written = 0;
                    if (not WinHttpWriteData(internalRef.hRequest, "0\r\n\r\n", 5, &written))
                    {
                        operation->enqueueError(Result::Error("HttpClient: WinHttpWriteData final chunk failed"));
                        internalRef.workerRunning = false;
                        return 0;
                    }
                }
            }

            if (not WinHttpReceiveResponse(internalRef.hRequest, NULL))
            {
                operation->enqueueError(isCancelRequested(internalRef)
                                            ? Result::Error("HttpClient: request cancelled")
                                            : Result::Error("HttpClient: WinHttpReceiveResponse failed"));
                internalRef.workerRunning = false;
                return 0;
            }

            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            (void)WinHttpQueryHeaders(internalRef.hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                      WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
            operation->currentResponse->statusCode = static_cast<int>(statusCode);

            DWORD usedProtocols     = 0;
            DWORD usedProtocolsSize = sizeof(usedProtocols);
            if (WinHttpQueryOption(internalRef.hRequest, WINHTTP_OPTION_HTTP_PROTOCOL_USED, &usedProtocols,
                                   &usedProtocolsSize))
            {
                operation->currentResponse->negotiatedProtocol = (usedProtocols & WINHTTP_PROTOCOL_FLAG_HTTP2) != 0
                                                                     ? HttpClientResponse::Protocol::Http2
                                                                     : HttpClientResponse::Protocol::Http11;
            }
            else
            {
                operation->currentResponse->negotiatedProtocol = HttpClientResponse::Protocol::Http11;
            }
            if (isHttp2Required(operation->currentRequest) and
                operation->currentResponse->negotiatedProtocol != HttpClientResponse::Protocol::Http2)
            {
                operation->enqueueError(Result::Error("HttpClient: HTTP/2 required but not negotiated"));
                internalRef.workerRunning = false;
                return 0;
            }
            operation->currentResponse->redirectCount = 0;

            DWORD rawHeaderBytes = 0;
            (void)WinHttpQueryHeaders(internalRef.hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                      WINHTTP_HEADER_NAME_BY_INDEX, NULL, &rawHeaderBytes, WINHTTP_NO_HEADER_INDEX);
            if (rawHeaderBytes > operation->backendScratch.sizeInBytes())
            {
                operation->enqueueError(Result::Error("HttpClient: backend scratch too small for response headers"));
                internalRef.workerRunning = false;
                return 0;
            }
            if (rawHeaderBytes > 0)
            {
                if (WinHttpQueryHeaders(internalRef.hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                        WINHTTP_HEADER_NAME_BY_INDEX, operation->backendScratch.data(), &rawHeaderBytes,
                                        WINHTTP_NO_HEADER_INDEX))
                {
                    const int utf8Len =
                        WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<wchar_t*>(operation->backendScratch.data()),
                                            static_cast<int>(rawHeaderBytes / sizeof(wchar_t)), NULL, 0, NULL, NULL);
                    if (utf8Len < 0 or static_cast<size_t>(utf8Len) > operation->currentResponse->headers.sizeInBytes())
                    {
                        operation->enqueueError(Result::Error("HttpClient: response headers buffer too small"));
                        internalRef.workerRunning = false;
                        return 0;
                    }
                    (void)WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<wchar_t*>(operation->backendScratch.data()),
                                              static_cast<int>(rawHeaderBytes / sizeof(wchar_t)),
                                              const_cast<char*>(operation->currentResponse->headers.data()), utf8Len,
                                              NULL, NULL);
                    operation->currentResponse->headersLength = static_cast<size_t>(utf8Len > 0 ? utf8Len : 0);
                }
            }

            wchar_t* effectiveUrl = nullptr;
            DWORD    effectiveLen = sizeof(effectiveUrl);
            if (WinHttpQueryOption(internalRef.hRequest, WINHTTP_OPTION_URL, &effectiveUrl, &effectiveLen) and
                effectiveUrl != nullptr)
            {
                const int utf8Len = WideCharToMultiByte(CP_UTF8, 0, effectiveUrl, -1, NULL, 0, NULL, NULL);
                if (utf8Len > 1)
                {
                    if (static_cast<size_t>(utf8Len - 1) > operation->responseMetadata.sizeInBytes())
                    {
                        GlobalFree(effectiveUrl);
                        operation->enqueueError(Result::Error("HttpClient: response metadata buffer too small"));
                        internalRef.workerRunning = false;
                        return 0;
                    }
                    (void)WideCharToMultiByte(CP_UTF8, 0, effectiveUrl, -1, operation->responseMetadata.data(),
                                              static_cast<int>(operation->responseMetadata.sizeInBytes()), NULL, NULL);
                    const Result effectiveUrlError = operation->copyResponseEffectiveUrl(
                        StringSpan({operation->responseMetadata.data(), static_cast<size_t>(utf8Len - 1)}, false,
                                   operation->currentRequest.url.getEncoding()));
                    if (not effectiveUrlError)
                    {
                        GlobalFree(effectiveUrl);
                        operation->enqueueError(effectiveUrlError);
                        internalRef.workerRunning = false;
                        return 0;
                    }
                }
                GlobalFree(effectiveUrl);
            }

            operation->enqueueResponseHead();

            for (;;)
            {
                DWORD available = 0;
                if (not WinHttpQueryDataAvailable(internalRef.hRequest, &available))
                {
                    break;
                }
                if (available == 0)
                {
                    break;
                }

                size_t      bufferIndex = 0;
                Span<char>  data;
                const DWORD toRead = available;
                if (not operation->allocateResponseBuffer(1, bufferIndex, data))
                {
                    operation->enqueueError(Result::Error("HttpClient: response buffer exhausted"));
                    internalRef.workerRunning = false;
                    return 0;
                }

                DWORD bytesRead = 0;
                if (not WinHttpReadData(internalRef.hRequest, data.data(),
                                        static_cast<DWORD>(data.sizeInBytes() < toRead ? data.sizeInBytes() : toRead),
                                        &bytesRead))
                {
                    operation->releaseResponseBuffer(bufferIndex);
                    operation->enqueueError(Result::Error("HttpClient: WinHttpReadData failed"));
                    internalRef.workerRunning = false;
                    return 0;
                }
                if (bytesRead == 0)
                {
                    operation->releaseResponseBuffer(bufferIndex);
                    break;
                }
                operation->enqueueResponseBuffer(bufferIndex, bytesRead);
            }

            operation->enqueueResponseComplete();

            internalRef.handlesMutex.lock();
            if (internalRef.hRequest != NULL)
            {
                WinHttpCloseHandle(internalRef.hRequest);
                internalRef.hRequest = NULL;
            }
            if (internalRef.hConnect != NULL)
            {
                WinHttpCloseHandle(internalRef.hConnect);
                internalRef.hConnect = NULL;
            }
            internalRef.handlesMutex.unlock();
            internalRef.workerRunning = false;
            return 0;
        },
        this, 0, NULL);

    if (internal.workerThread == NULL)
    {
        internal.workerRunning = false;
    }
    SC_TRY_MSG(internal.workerThread != NULL, "HttpClient: CreateThread failed");

    return Result(true);
}
