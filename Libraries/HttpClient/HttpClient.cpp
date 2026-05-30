// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpClient.h"

#include <string.h>

#if SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
#include <errno.h>
#include <pthread.h>
#include <time.h>
#endif

#if SC_PLATFORM_APPLE
#include "Internal/HttpClientApple.inl"
#elif SC_PLATFORM_WINDOWS
#include "Internal/HttpClientWindows.inl"
#elif SC_PLATFORM_LINUX
#include "Internal/HttpClientLinux.inl"
#else
SC::HttpClient::HttpClient() {}
SC::HttpClient::~HttpClient() {}
SC::Result SC::HttpClient::platformInit() { return Result::Error("HttpClient: unsupported platform"); }
SC::Result SC::HttpClient::platformClose() { return Result(true); }

SC::HttpClientOperation::HttpClientOperation() {}
SC::HttpClientOperation::~HttpClientOperation() {}
SC::Result SC::HttpClientOperation::platformInit() { return Result::Error("HttpClient: unsupported platform"); }
SC::Result SC::HttpClientOperation::platformClose() { return Result(true); }
SC::Result SC::HttpClientOperation::platformStart() { return Result::Error("HttpClient: unsupported platform"); }
SC::Result SC::HttpClientOperation::platformCancel() { return Result(true); }
#endif

namespace
{
static char asciiLower(char c)
{
    if (c >= 'A' and c <= 'Z')
    {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

static bool asciiEqualsIgnoreCase(SC::StringSpan left, SC::StringSpan right)
{
    if (left.sizeInBytes() != right.sizeInBytes())
    {
        return false;
    }
    const SC::Span<const char> leftBytes  = left.toCharSpan();
    const SC::Span<const char> rightBytes = right.toCharSpan();
    for (size_t idx = 0; idx < leftBytes.sizeInBytes(); ++idx)
    {
        if (asciiLower(leftBytes[idx]) != asciiLower(rightBytes[idx]))
        {
            return false;
        }
    }
    return true;
}

static bool asciiStartsWithIgnoreCase(SC::StringSpan text, SC::StringSpan prefix)
{
    if (text.sizeInBytes() < prefix.sizeInBytes())
    {
        return false;
    }
    const SC::Span<const char> textBytes   = text.toCharSpan();
    const SC::Span<const char> prefixBytes = prefix.toCharSpan();
    for (size_t idx = 0; idx < prefixBytes.sizeInBytes(); ++idx)
    {
        if (asciiLower(textBytes[idx]) != asciiLower(prefixBytes[idx]))
        {
            return false;
        }
    }
    return true;
}

static bool isAsciiWhiteSpace(char c) { return c == ' ' or c == '\t' or c == '\r' or c == '\n'; }

static SC::StringSpan trimAsciiWhiteSpace(SC::Span<const char> span)
{
    size_t start = 0;
    size_t end   = span.sizeInBytes();
    while (start < end and isAsciiWhiteSpace(span[start]))
    {
        start += 1;
    }
    while (end > start and isAsciiWhiteSpace(span[end - 1]))
    {
        end -= 1;
    }
    return {{span.data() + start, end - start}, false, SC::StringEncoding::Ascii};
}

static bool parseAsciiUint64(SC::StringSpan text, SC::uint64_t& value)
{
    const SC::Span<const char> bytes = text.toCharSpan();
    if (bytes.empty())
    {
        return false;
    }

    SC::uint64_t parsed = 0;
    for (size_t idx = 0; idx < bytes.sizeInBytes(); ++idx)
    {
        const char c = bytes[idx];
        if (c < '0' or c > '9')
        {
            return false;
        }
        const SC::uint64_t digit = static_cast<SC::uint64_t>(c - '0');
        if (parsed > ((~static_cast<SC::uint64_t>(0)) - digit) / 10)
        {
            return false;
        }
        parsed = parsed * 10 + digit;
    }

    value = parsed;
    return true;
}

static bool requestHasHeader(const SC::HttpClientRequest& request, SC::StringSpan name)
{
    for (size_t idx = 0; idx < request.headers.sizeInElements(); ++idx)
    {
        if (asciiEqualsIgnoreCase(request.headers[idx].name, name))
        {
            return true;
        }
    }
    return false;
}

static bool isValidRequestMethod(SC::HttpClientRequest::Method method)
{
    switch (method)
    {
    case SC::HttpClientRequest::HttpGET:
    case SC::HttpClientRequest::HttpPOST:
    case SC::HttpClientRequest::HttpPUT:
    case SC::HttpClientRequest::HttpHEAD:
    case SC::HttpClientRequest::HttpDELETE:
    case SC::HttpClientRequest::HttpPATCH:
    case SC::HttpClientRequest::HttpOPTIONS: return true;
    }
    return false;
}

static bool isHttpHeaderNameByte(char c)
{
    if ((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9'))
    {
        return true;
    }
    switch (c)
    {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~': return true;
    }
    return false;
}

static SC::Result validateRequestHeaders(SC::Span<const SC::HttpClientHeader> headers)
{
    for (size_t headerIdx = 0; headerIdx < headers.sizeInElements(); ++headerIdx)
    {
        const SC::Span<const char> name = headers[headerIdx].name.toCharSpan();
        SC_TRY_MSG(name.sizeInBytes() > 0, "HttpClientRequest: request header name is empty");
        for (size_t byteIdx = 0; byteIdx < name.sizeInBytes(); ++byteIdx)
        {
            SC_TRY_MSG(isHttpHeaderNameByte(name[byteIdx]), "HttpClientRequest: request header name is invalid");
        }

        const SC::Span<const char> value = headers[headerIdx].value.toCharSpan();
        for (size_t byteIdx = 0; byteIdx < value.sizeInBytes(); ++byteIdx)
        {
            SC_TRY_MSG(value[byteIdx] != '\r' and value[byteIdx] != '\n' and value[byteIdx] != '\0',
                       "HttpClientRequest: request header value is invalid");
        }
    }
    return SC::Result(true);
}

static SC::Result validateRequestBodyFramingHeaders(const SC::HttpClientRequest& request)
{
    SC_TRY_MSG(not requestHasHeader(request, SC::StringSpan("Transfer-Encoding")),
               "HttpClientRequest: Transfer-Encoding is controlled by request body framing");
    SC_TRY_MSG(not(request.body.isChunkedStream() and requestHasHeader(request, SC::StringSpan("Content-Length"))),
               "HttpClientRequest: chunked request body cannot use Content-Length");
    return SC::Result(true);
}

static SC::Result validateRequestBodyShape(const SC::HttpClientRequestBody& body)
{
    if (body.framing == SC::HttpClientRequestBody::FixedSize)
    {
        SC_TRY_MSG(body.provider == nullptr, "HttpClientRequest: fixed request body cannot use provider");
        SC_TRY_MSG(body.sizeInBytes == 0 or body.sizeInBytes == body.bytes.sizeInBytes(),
                   "HttpClientRequest: inline request body size mismatch");
    }
    else if (body.framing == SC::HttpClientRequestBody::SizedStream)
    {
        SC_TRY_MSG(body.bytes.sizeInBytes() == 0,
                   "HttpClientRequest: sized stream request body cannot use inline bytes");
        SC_TRY_MSG(body.provider != nullptr, "HttpClientRequest: sized stream request body requires provider");
    }
    else if (body.framing == SC::HttpClientRequestBody::ChunkedStream)
    {
        SC_TRY_MSG(body.bytes.sizeInBytes() == 0,
                   "HttpClientRequest: chunked stream request body cannot use inline bytes");
        SC_TRY_MSG(body.sizeInBytes == 0, "HttpClientRequest: chunked stream request body cannot declare a fixed size");
        SC_TRY_MSG(body.provider != nullptr, "HttpClientRequest: chunked stream request body requires provider");
    }
    else
    {
        return SC::Result::Error("HttpClientRequest: invalid request body framing");
    }
    return SC::Result(true);
}

static bool isValidProtocolPreference(SC::HttpClientRequestProtocolOptions::Preference preference)
{
    switch (preference)
    {
    case SC::HttpClientRequestProtocolOptions::Default:
    case SC::HttpClientRequestProtocolOptions::Http11Only:
    case SC::HttpClientRequestProtocolOptions::Http2Preferred:
    case SC::HttpClientRequestProtocolOptions::Http2Required: return true;
    }
    return false;
}

static bool isValidRedirectMode(SC::HttpClientRequestRedirectOptions::Mode mode)
{
    switch (mode)
    {
    case SC::HttpClientRequestRedirectOptions::NoRedirects:
    case SC::HttpClientRequestRedirectOptions::FollowGetHead:
    case SC::HttpClientRequestRedirectOptions::FollowAll: return true;
    }
    return false;
}

static bool isValidProxyMode(SC::HttpClientRequestProxyOptions::Mode mode)
{
    switch (mode)
    {
    case SC::HttpClientRequestProxyOptions::Default:
    case SC::HttpClientRequestProxyOptions::NoProxy:
    case SC::HttpClientRequestProxyOptions::Http: return true;
    }
    return false;
}

static bool hasHttpHeaderUnsafeBytes(SC::StringSpan value)
{
    const SC::Span<const char> bytes = value.toCharSpan();
    for (size_t idx = 0; idx < bytes.sizeInBytes(); ++idx)
    {
        if (bytes[idx] == '\r' or bytes[idx] == '\n' or bytes[idx] == '\0')
        {
            return true;
        }
    }
    return false;
}

static SC::Result validateProxyOptions(const SC::HttpClientRequestProxyOptions& proxy)
{
    SC_TRY_MSG(isValidProxyMode(proxy.mode), "HttpClientRequestOptions: invalid proxy mode");
    if (proxy.mode == SC::HttpClientRequestProxyOptions::Http)
    {
        static constexpr size_t HttpProxySchemeBytes = sizeof("http://") - 1;

        SC_TRY_MSG(proxy.url.sizeInBytes() > sizeof("http://") - 1,
                   "HttpClientRequestOptions: HTTP proxy URL is empty");
        SC_TRY_MSG(asciiStartsWithIgnoreCase(proxy.url, SC::StringSpan("http://")),
                   "HttpClientRequestOptions: only http:// proxy URLs are supported");
        const SC::Span<const char> proxyBytes = proxy.url.toCharSpan();
        for (size_t idx = HttpProxySchemeBytes; idx < proxyBytes.sizeInBytes(); ++idx)
        {
            SC_TRY_MSG(proxyBytes[idx] != '/' and proxyBytes[idx] != '?' and proxyBytes[idx] != '#',
                       "HttpClientRequestOptions: HTTP proxy URL must not include path, query, or fragment");
        }
        SC_TRY_MSG(not hasHttpHeaderUnsafeBytes(proxy.authorization),
                   "HttpClientRequestOptions: proxy authorization contains an invalid line break");
        SC_TRY_MSG(not hasHttpHeaderUnsafeBytes(proxy.bypassList),
                   "HttpClientRequestOptions: proxy bypass list contains an invalid line break");
    }
    else
    {
        SC_TRY_MSG(proxy.url.sizeInBytes() == 0,
                   "HttpClientRequestOptions: proxy URL is only valid with HTTP proxy mode");
        SC_TRY_MSG(proxy.authorization.sizeInBytes() == 0,
                   "HttpClientRequestOptions: proxy authorization is only valid with HTTP proxy mode");
        SC_TRY_MSG(proxy.bypassList.sizeInBytes() == 0,
                   "HttpClientRequestOptions: proxy bypass list is only valid with HTTP proxy mode");
    }
    return SC::Result(true);
}

static SC::Result validateRequestOptionsShape(const SC::HttpClientRequestOptions& options)
{
    SC_TRY_MSG(isValidRedirectMode(options.redirect.mode), "HttpClientRequestOptions: invalid redirect mode");
    SC_TRY_MSG(isValidProtocolPreference(options.protocol.preference),
               "HttpClientRequestOptions: invalid protocol preference");
    SC_TRY(validateProxyOptions(options.proxy));
    return SC::Result(true);
}

static SC::Result validateRequestShape(const SC::HttpClientRequest& request)
{
    SC_TRY_MSG(request.url.sizeInBytes() > 0, "HttpClientRequest: URL is empty");
    SC_TRY_MSG(isValidRequestMethod(request.method), "HttpClientRequest: invalid request method");
    SC_TRY(validateRequestHeaders(request.headers));
    SC_TRY(validateRequestBodyFramingHeaders(request));
    SC_TRY(validateRequestBodyShape(request.body));
    SC_TRY(validateRequestOptionsShape(request.options));

    if (request.options.redirect.mode == SC::HttpClientRequestRedirectOptions::FollowGetHead)
    {
        SC_TRY_MSG(request.method == SC::HttpClientRequest::HttpGET or
                       request.method == SC::HttpClientRequest::HttpHEAD,
                   "HttpClientRequest: FollowGetHead requires GET or HEAD");
    }
    if (request.options.redirect.mode != SC::HttpClientRequestRedirectOptions::NoRedirects)
    {
        SC_TRY_MSG((not request.body.isStreamed()) or request.body.canReplay,
                   "HttpClientRequest: automatic redirects require a replayable request body");
    }
    return SC::Result(true);
}

static SC::Result validateRequestOptionsSupported(const SC::HttpClientRequestOptions& options,
                                                  const SC::HttpClientCapabilities&   capabilities)
{
    if (options.redirect.mode != SC::HttpClientRequestRedirectOptions::NoRedirects)
    {
        SC_TRY_MSG(capabilities.redirectPolicy, "HttpClient: backend does not support redirect policy");
    }

    if (options.protocol.preference == SC::HttpClientRequestProtocolOptions::Http11Only)
    {
        SC_TRY_MSG(capabilities.protocolHttp11Only, "HttpClient: backend does not support forcing HTTP/1.1");
    }
    else if (options.protocol.preference == SC::HttpClientRequestProtocolOptions::Http2Preferred)
    {
        SC_TRY_MSG(capabilities.protocolHttp2Preferred, "HttpClient: backend does not support HTTP/2");
    }
    else if (options.protocol.preference == SC::HttpClientRequestProtocolOptions::Http2Required)
    {
        SC_TRY_MSG(capabilities.protocolHttp2Required, "HttpClient: backend does not support requiring HTTP/2");
    }

    if (not options.tls.verifyPeer)
    {
        SC_TRY_MSG(capabilities.tlsDisablePeerVerification,
                   "HttpClient: backend does not support disabling TLS peer verification");
    }
    if (options.tls.caCertificatesPath.sizeInBytes() > 0)
    {
        SC_TRY_MSG(capabilities.tlsCustomCaPath, "HttpClient: backend does not support custom TLS CA paths");
    }

    if (options.proxy.mode == SC::HttpClientRequestProxyOptions::NoProxy)
    {
        SC_TRY_MSG(capabilities.proxyNoProxy, "HttpClient: backend does not support no-proxy policy");
    }
    else if (options.proxy.mode == SC::HttpClientRequestProxyOptions::Http)
    {
        SC_TRY_MSG(capabilities.proxyHttp, "HttpClient: backend does not support HTTP proxy policy");
        if (options.proxy.authorization.sizeInBytes() > 0)
        {
            SC_TRY_MSG(capabilities.proxyAuthorization, "HttpClient: backend does not support proxy authorization");
        }
        if (options.proxy.bypassList.sizeInBytes() > 0)
        {
            SC_TRY_MSG(capabilities.proxyBypassList, "HttpClient: backend does not support proxy bypass lists");
        }
    }
    return SC::Result(true);
}

static SC::Result sliceResponseBuffers(SC::Span<SC::HttpClientResponseBuffer> buffers, SC::Span<char> memory)
{
    SC_TRY_MSG(not buffers.empty(), "HttpClientOperation: response buffers missing");
    SC_TRY_MSG(memory.sizeInBytes() >= buffers.sizeInElements(),
               "HttpClientOperation: response buffer memory too small for slicing");

    const size_t sliceSize = memory.sizeInBytes() / buffers.sizeInElements();
    SC_TRY_MSG(sliceSize > 0, "HttpClientOperation: response buffer slices would be empty");

    for (size_t idx = 0; idx < buffers.sizeInElements(); ++idx)
    {
        buffers[idx].data = {memory.data() + (idx * sliceSize),
                             idx + 1 == buffers.sizeInElements() ? memory.sizeInBytes() - idx * sliceSize : sliceSize};
    }
    return SC::Result(true);
}
} // namespace

namespace
{
#if SC_PLATFORM_WINDOWS
using HttpClientLocalMutexStorage             = CRITICAL_SECTION;
using HttpClientLocalConditionVariableStorage = CONDITION_VARIABLE;
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
using HttpClientLocalMutexStorage             = pthread_mutex_t;
using HttpClientLocalConditionVariableStorage = pthread_cond_t;

static timespec httpClientMakeAbsoluteTimeout(SC::uint32_t timeoutMilliseconds)
{
    timespec deadline = {};
    (void)clock_gettime(CLOCK_REALTIME, &deadline);

    deadline.tv_sec += static_cast<time_t>(timeoutMilliseconds / 1000);
    deadline.tv_nsec += static_cast<long>((timeoutMilliseconds % 1000) * 1000000u);
    if (deadline.tv_nsec >= 1000000000L)
    {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }
    return deadline;
}
#else
struct HttpClientLocalMutexStorage
{
    char value;
};

struct HttpClientLocalConditionVariableStorage
{
    char value;
};
#endif
} // namespace

SC::HttpClientLocalMutex::HttpClientLocalMutex()
{
    static_assert(sizeof(storage) >= sizeof(HttpClientLocalMutexStorage), "HttpClient mutex storage too small");
    static_assert(alignof(HttpClientLocalMutex) >= alignof(HttpClientLocalMutexStorage),
                  "HttpClient mutex alignment mismatch");

#if SC_PLATFORM_WINDOWS
    InitializeCriticalSection(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_mutex_init(reinterpret_cast<HttpClientLocalMutexStorage*>(storage), nullptr);
#endif
}

SC::HttpClientLocalMutex::~HttpClientLocalMutex()
{
#if SC_PLATFORM_WINDOWS
    DeleteCriticalSection(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_mutex_destroy(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#endif
}

void SC::HttpClientLocalMutex::lock()
{
#if SC_PLATFORM_WINDOWS
    EnterCriticalSection(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_mutex_lock(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#endif
}

void SC::HttpClientLocalMutex::unlock()
{
#if SC_PLATFORM_WINDOWS
    LeaveCriticalSection(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_mutex_unlock(reinterpret_cast<HttpClientLocalMutexStorage*>(storage));
#endif
}

SC::HttpClientLocalConditionVariable::HttpClientLocalConditionVariable()
{
    static_assert(sizeof(storage) >= sizeof(HttpClientLocalConditionVariableStorage),
                  "HttpClient condition variable storage too small");
    static_assert(alignof(HttpClientLocalConditionVariable) >= alignof(HttpClientLocalConditionVariableStorage),
                  "HttpClient condition variable alignment mismatch");

#if SC_PLATFORM_WINDOWS
    InitializeConditionVariable(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_init(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage), nullptr);
#endif
}

SC::HttpClientLocalConditionVariable::~HttpClientLocalConditionVariable()
{
#if SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_destroy(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#endif
}

void SC::HttpClientLocalConditionVariable::wait(HttpClientLocalMutex& mutex)
{
#if SC_PLATFORM_WINDOWS
    (void)SleepConditionVariableCS(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage),
                                   reinterpret_cast<HttpClientLocalMutexStorage*>(mutex.storage), INFINITE);
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_wait(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage),
                            reinterpret_cast<HttpClientLocalMutexStorage*>(mutex.storage));
#endif
}

bool SC::HttpClientLocalConditionVariable::wait(HttpClientLocalMutex& mutex, uint32_t timeoutMilliseconds)
{
#if SC_PLATFORM_WINDOWS
    return SleepConditionVariableCS(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage),
                                    reinterpret_cast<HttpClientLocalMutexStorage*>(mutex.storage),
                                    timeoutMilliseconds) != 0;
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    const timespec deadline = httpClientMakeAbsoluteTimeout(timeoutMilliseconds);
    return pthread_cond_timedwait(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage),
                                  reinterpret_cast<HttpClientLocalMutexStorage*>(mutex.storage),
                                  &deadline) != ETIMEDOUT;
#else
    SC_COMPILER_UNUSED(mutex);
    SC_COMPILER_UNUSED(timeoutMilliseconds);
    return true;
#endif
}

void SC::HttpClientLocalConditionVariable::signal()
{
#if SC_PLATFORM_WINDOWS
    WakeConditionVariable(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_signal(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#endif
}

void SC::HttpClientLocalConditionVariable::broadcast()
{
#if SC_PLATFORM_WINDOWS
    WakeAllConditionVariable(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#elif SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
    (void)pthread_cond_broadcast(reinterpret_cast<HttpClientLocalConditionVariableStorage*>(storage));
#endif
}

SC::HttpClientCapabilities SC::HttpClient::getCapabilities()
{
    HttpClientCapabilities capabilities;

#if SC_PLATFORM_APPLE
    capabilities.backend = HttpClientCapabilities::AppleURLSession;
#elif SC_PLATFORM_LINUX
    capabilities.backend = HttpClientCapabilities::LibCurl;
#elif SC_PLATFORM_WINDOWS
    capabilities.backend = HttpClientCapabilities::WinHttp;
#else
    capabilities.backend = HttpClientCapabilities::Unsupported;
#endif

    if (capabilities.backend == HttpClientCapabilities::Unsupported)
    {
        return capabilities;
    }

    capabilities.multipleOperationsPerClient = true;
    capabilities.fixedRequestBody            = true;
    capabilities.sizedStreamRequestBody      = true;
    capabilities.chunkedStreamRequestBody    = true;
    capabilities.redirectPolicy              = true;
    capabilities.protocolHttp2Preferred      = true;

#if SC_PLATFORM_APPLE
    capabilities.protocolHttp11Only         = false;
    capabilities.protocolHttp2Required      = false;
    capabilities.tlsDisablePeerVerification = false;
    capabilities.tlsCustomCaPath            = false;
    capabilities.proxyNoProxy               = false;
    capabilities.proxyHttp                  = false;
    capabilities.proxyAuthorization         = false;
    capabilities.proxyBypassList            = false;
#elif SC_PLATFORM_LINUX
    capabilities.protocolHttp11Only         = true;
    capabilities.protocolHttp2Required      = true;
    capabilities.tlsDisablePeerVerification = true;
    capabilities.tlsCustomCaPath            = true;
    capabilities.proxyNoProxy               = true;
    capabilities.proxyHttp                  = true;
    capabilities.proxyAuthorization         = true;
    capabilities.proxyBypassList            = true;
#elif SC_PLATFORM_WINDOWS
    capabilities.protocolHttp11Only         = true;
    capabilities.protocolHttp2Required      = true;
    capabilities.tlsDisablePeerVerification = true;
    capabilities.tlsCustomCaPath            = false;
    capabilities.proxyNoProxy               = true;
    capabilities.proxyHttp                  = true;
    capabilities.proxyAuthorization         = true;
    capabilities.proxyBypassList            = true;
#endif

    capabilities.contentCodingPolicy = false;
    return capabilities;
}

bool SC::HttpClientCapabilities::hasBackend(Backend requiredBackend) const { return backend == requiredBackend; }

bool SC::HttpClientCapabilities::supports(Feature feature) const
{
    switch (feature)
    {
    case MultipleOperationsPerClient: return multipleOperationsPerClient;
    case FixedRequestBody: return fixedRequestBody;
    case SizedStreamRequestBody: return sizedStreamRequestBody;
    case ChunkedStreamRequestBody: return chunkedStreamRequestBody;
    case RedirectPolicy: return redirectPolicy;
    case ProtocolHttp11Only: return protocolHttp11Only;
    case ProtocolHttp2Preferred: return protocolHttp2Preferred;
    case ProtocolHttp2Required: return protocolHttp2Required;
    case TlsDisablePeerVerification: return tlsDisablePeerVerification;
    case TlsCustomCaPath: return tlsCustomCaPath;
    case ProxyNoProxy: return proxyNoProxy;
    case ProxyHttp: return proxyHttp;
    case ProxyAuthorization: return proxyAuthorization;
    case ProxyBypassList: return proxyBypassList;
    case ContentCodingPolicy: return contentCodingPolicy;
    }
    return false;
}

bool SC::HttpClientCapabilities::supportsRequestOptions(const HttpClientRequestOptions& options) const
{
    return requireRequestOptions(options);
}

bool SC::HttpClientCapabilities::supportsAll(Span<const Feature> features) const
{
    for (size_t idx = 0; idx < features.sizeInElements(); ++idx)
    {
        if (not supports(features[idx]))
        {
            return false;
        }
    }
    return true;
}

SC::Result SC::HttpClientCapabilities::requireBackend(Backend requiredBackend) const
{
    SC_TRY_MSG(hasBackend(requiredBackend), "HttpClient: required backend is not active");
    return Result(true);
}

SC::Result SC::HttpClientCapabilities::requireFeatures(Span<const Feature> features) const
{
    SC_TRY_MSG(supportsAll(features), "HttpClient: required backend feature is not supported");
    return Result(true);
}

SC::Result SC::HttpClientCapabilities::requireRequestOptions(const HttpClientRequestOptions& options) const
{
    SC_TRY(validateRequestOptionsShape(options));
    SC_TRY(validateRequestOptionsSupported(options, *this));
    return Result(true);
}

const char* SC::HttpClientCapabilities::getBackendName() const { return getBackendName(backend); }

const char* SC::HttpClientCapabilities::getBackendName(Backend backend)
{
    switch (backend)
    {
    case Unsupported: return "unsupported";
    case AppleURLSession: return "apple-url-session";
    case LibCurl: return "libcurl";
    case WinHttp: return "winhttp";
    }
    return "unknown";
}

const char* SC::HttpClientCapabilities::getFeatureName(Feature feature)
{
    switch (feature)
    {
    case MultipleOperationsPerClient: return "multiple-operations-per-client";
    case FixedRequestBody: return "fixed-request-body";
    case SizedStreamRequestBody: return "sized-stream-request-body";
    case ChunkedStreamRequestBody: return "chunked-stream-request-body";
    case RedirectPolicy: return "redirect-policy";
    case ProtocolHttp11Only: return "protocol-http11-only";
    case ProtocolHttp2Preferred: return "protocol-http2-preferred";
    case ProtocolHttp2Required: return "protocol-http2-required";
    case TlsDisablePeerVerification: return "tls-disable-peer-verification";
    case TlsCustomCaPath: return "tls-custom-ca-path";
    case ProxyNoProxy: return "proxy-no-proxy";
    case ProxyHttp: return "proxy-http";
    case ProxyAuthorization: return "proxy-authorization";
    case ProxyBypassList: return "proxy-bypass-list";
    case ContentCodingPolicy: return "content-coding-policy";
    }
    return "unknown";
}

const char* SC::HttpClientRequestBody::getFramingName(Framing framing)
{
    switch (framing)
    {
    case FixedSize: return "fixed-size";
    case SizedStream: return "sized-stream";
    case ChunkedStream: return "chunked-stream";
    }
    return "unknown";
}

const char* SC::HttpClientRequestRedirectOptions::getModeName(Mode mode)
{
    switch (mode)
    {
    case NoRedirects: return "no-redirects";
    case FollowGetHead: return "follow-get-head";
    case FollowAll: return "follow-all";
    }
    return "unknown";
}

const char* SC::HttpClientRequestProtocolOptions::getPreferenceName(Preference preference)
{
    switch (preference)
    {
    case Default: return "default";
    case Http11Only: return "http/1.1-only";
    case Http2Preferred: return "h2-preferred";
    case Http2Required: return "h2-required";
    }
    return "unknown";
}

const char* SC::HttpClientRequestProxyOptions::getModeName(Mode mode)
{
    switch (mode)
    {
    case Default: return "default";
    case NoProxy: return "no-proxy";
    case Http: return "http";
    }
    return "unknown";
}

const char* SC::HttpClientRequest::getMethodName(Method method)
{
    switch (method)
    {
    case HttpGET: return "GET";
    case HttpPOST: return "POST";
    case HttpPUT: return "PUT";
    case HttpHEAD: return "HEAD";
    case HttpDELETE: return "DELETE";
    case HttpPATCH: return "PATCH";
    case HttpOPTIONS: return "OPTIONS";
    }
    return "UNKNOWN";
}

SC::Result SC::HttpClientRequest::validate() const { return validateRequestShape(*this); }

SC::HttpClientContentCoding::Type SC::HttpClientContentCoding::parseName(StringSpan name)
{
    if (asciiEqualsIgnoreCase(name, StringSpan("identity")))
    {
        return Identity;
    }
    if (asciiEqualsIgnoreCase(name, StringSpan("gzip")) or asciiEqualsIgnoreCase(name, StringSpan("x-gzip")))
    {
        return GZip;
    }
    if (asciiEqualsIgnoreCase(name, StringSpan("deflate")))
    {
        return Deflate;
    }
    if (asciiEqualsIgnoreCase(name, StringSpan("compress")) or asciiEqualsIgnoreCase(name, StringSpan("x-compress")))
    {
        return Compress;
    }
    if (asciiEqualsIgnoreCase(name, StringSpan("br")))
    {
        return Brotli;
    }
    return Unknown;
}

const char* SC::HttpClientContentCoding::getName(Type type)
{
    switch (type)
    {
    case Unknown: return "unknown";
    case Identity: return "identity";
    case GZip: return "gzip";
    case Deflate: return "deflate";
    case Compress: return "compress";
    case Brotli: return "br";
    }
    return "unknown";
}

SC::HttpClientTransferCoding::Type SC::HttpClientTransferCoding::parseName(StringSpan name)
{
    if (asciiEqualsIgnoreCase(name, StringSpan("chunked")))
    {
        return Chunked;
    }
    if (asciiEqualsIgnoreCase(name, StringSpan("compress")) or asciiEqualsIgnoreCase(name, StringSpan("x-compress")))
    {
        return Compress;
    }
    if (asciiEqualsIgnoreCase(name, StringSpan("deflate")))
    {
        return Deflate;
    }
    if (asciiEqualsIgnoreCase(name, StringSpan("gzip")) or asciiEqualsIgnoreCase(name, StringSpan("x-gzip")))
    {
        return GZip;
    }
    return Unknown;
}

const char* SC::HttpClientTransferCoding::getName(Type type)
{
    switch (type)
    {
    case Unknown: return "unknown";
    case Chunked: return "chunked";
    case Compress: return "compress";
    case Deflate: return "deflate";
    case GZip: return "gzip";
    }
    return "unknown";
}

SC::Result SC::HttpClientContentCoding::writeAcceptEncoding(Span<const Type> types, Span<char> destination,
                                                            StringSpan& value)
{
    SC_TRY_MSG(not types.empty(), "HttpClientContentCoding: content coding list is empty");

    size_t usedBytes = 0;
    for (size_t idx = 0; idx < types.sizeInElements(); ++idx)
    {
        SC_TRY_MSG(types[idx] != Unknown, "HttpClientContentCoding: cannot write unknown content coding");

        const char* const name      = getName(types[idx]);
        const size_t      nameBytes = strlen(name);
        const size_t      sepBytes  = idx == 0 ? 0 : 2;
        SC_TRY_MSG(destination.sizeInBytes() - usedBytes >= sepBytes + nameBytes,
                   "HttpClientContentCoding: destination too small");

        if (sepBytes > 0)
        {
            destination[usedBytes++] = ',';
            destination[usedBytes++] = ' ';
        }
        memcpy(destination.data() + usedBytes, name, nameBytes);
        usedBytes += nameBytes;
    }

    value = {{destination.data(), usedBytes}, false, StringEncoding::Ascii};
    return Result(true);
}

bool SC::HttpClientResponse::getHeader(StringSpan name, StringSpan& value) const
{
    HttpClientResponseHeaderIterator iterator;
    return findNextHeader(name, iterator, value);
}

bool SC::HttpClientResponse::hasHeader(StringSpan name) const
{
    StringSpan value;
    return getHeader(name, value);
}

bool SC::HttpClientResponse::findNextHeader(StringSpan name, HttpClientResponseHeaderIterator& iterator,
                                            StringSpan& value) const
{
    value = {};

    HttpClientHeader header;
    while (getNextHeader(iterator, header))
    {
        if (asciiEqualsIgnoreCase(header.name, name))
        {
            value = header.value;
            return true;
        }
    }
    return false;
}

bool SC::HttpClientResponse::getNextHeader(HttpClientResponseHeaderIterator& iterator, HttpClientHeader& header) const
{
    const Span<const char> allHeaders = {headers.data(), headersLength};

    header = {};

    while (iterator.offset < allHeaders.sizeInBytes())
    {
        size_t lineEnd = iterator.offset;
        while (lineEnd < allHeaders.sizeInBytes())
        {
            if (allHeaders[lineEnd] == '\r' and (lineEnd + 1) < allHeaders.sizeInBytes() and
                allHeaders[lineEnd + 1] == '\n')
            {
                break;
            }
            lineEnd += 1;
        }

        const Span<const char> line = {allHeaders.data() + iterator.offset, lineEnd - iterator.offset};
        iterator.offset             = lineEnd;
        if ((iterator.offset + 1) < allHeaders.sizeInBytes() and allHeaders[iterator.offset] == '\r' and
            allHeaders[iterator.offset + 1] == '\n')
        {
            iterator.offset += 2;
        }

        if (line.sizeInBytes() == 0)
        {
            continue;
        }

        size_t separator = 0;
        while (separator < line.sizeInBytes() and line[separator] != ':')
        {
            separator += 1;
        }
        if (separator < line.sizeInBytes())
        {
            const StringSpan headerName = trimAsciiWhiteSpace({line.data(), separator});
            const StringSpan headerValue =
                trimAsciiWhiteSpace({line.data() + separator + 1, line.sizeInBytes() - separator - 1});
            if (headerName.sizeInBytes() > 0)
            {
                header.name  = headerName;
                header.value = headerValue;
                return true;
            }
        }
    }
    return false;
}

bool SC::HttpClientResponse::getContentLength(uint64_t& value) const
{
    StringSpan text;
    return getHeader("Content-Length", text) and parseAsciiUint64(text, value);
}

bool SC::HttpClientResponse::getContentType(StringSpan& value) const { return getHeader("Content-Type", value); }

bool SC::HttpClientResponse::getContentEncoding(StringSpan& value) const
{
    return getHeader("Content-Encoding", value);
}

bool SC::HttpClientResponse::getTransferEncoding(StringSpan& value) const
{
    return getHeader("Transfer-Encoding", value);
}

bool SC::HttpClientResponse::getLocation(StringSpan& value) const { return getHeader("Location", value); }

bool SC::HttpClientResponse::getWwwAuthenticate(StringSpan& value) const
{
    return getHeader("WWW-Authenticate", value);
}

bool SC::HttpClientResponse::getProxyAuthenticate(StringSpan& value) const
{
    return getHeader("Proxy-Authenticate", value);
}

bool SC::HttpClientResponse::getNextContentCoding(HttpClientContentCodingIterator& iterator,
                                                  HttpClientContentCoding&         contentCoding) const
{
    contentCoding = {};

    for (;;)
    {
        if (not iterator.hasHeaderValue or iterator.valueOffset >= iterator.headerValue.sizeInBytes())
        {
            if (not findNextHeader("Content-Encoding", iterator.headerIterator, iterator.headerValue))
            {
                iterator.hasHeaderValue = false;
                return false;
            }
            iterator.valueOffset    = 0;
            iterator.hasHeaderValue = true;
        }

        const Span<const char> bytes = iterator.headerValue.toCharSpan();
        while (iterator.valueOffset < bytes.sizeInBytes() and
               (bytes[iterator.valueOffset] == ',' or bytes[iterator.valueOffset] == ' ' or
                bytes[iterator.valueOffset] == '\t'))
        {
            iterator.valueOffset += 1;
        }

        const size_t tokenStart = iterator.valueOffset;
        while (iterator.valueOffset < bytes.sizeInBytes() and bytes[iterator.valueOffset] != ',')
        {
            iterator.valueOffset += 1;
        }

        const StringSpan token = trimAsciiWhiteSpace({bytes.data() + tokenStart, iterator.valueOffset - tokenStart});
        if (iterator.valueOffset < bytes.sizeInBytes() and bytes[iterator.valueOffset] == ',')
        {
            iterator.valueOffset += 1;
        }

        if (token.sizeInBytes() == 0)
        {
            continue;
        }

        contentCoding.name = token;
        contentCoding.type = HttpClientContentCoding::parseName(token);
        return true;
    }
}

bool SC::HttpClientResponse::getNextTransferCoding(HttpClientTransferCodingIterator& iterator,
                                                   HttpClientTransferCoding&         transferCoding) const
{
    transferCoding = {};

    for (;;)
    {
        if (not iterator.hasHeaderValue or iterator.valueOffset >= iterator.headerValue.sizeInBytes())
        {
            if (not findNextHeader("Transfer-Encoding", iterator.headerIterator, iterator.headerValue))
            {
                iterator.hasHeaderValue = false;
                return false;
            }
            iterator.valueOffset    = 0;
            iterator.hasHeaderValue = true;
        }

        const Span<const char> bytes = iterator.headerValue.toCharSpan();
        while (iterator.valueOffset < bytes.sizeInBytes() and
               (bytes[iterator.valueOffset] == ',' or bytes[iterator.valueOffset] == ' ' or
                bytes[iterator.valueOffset] == '\t'))
        {
            iterator.valueOffset += 1;
        }

        const size_t tokenStart = iterator.valueOffset;
        while (iterator.valueOffset < bytes.sizeInBytes() and bytes[iterator.valueOffset] != ',')
        {
            iterator.valueOffset += 1;
        }

        const StringSpan token = trimAsciiWhiteSpace({bytes.data() + tokenStart, iterator.valueOffset - tokenStart});
        if (iterator.valueOffset < bytes.sizeInBytes() and bytes[iterator.valueOffset] == ',')
        {
            iterator.valueOffset += 1;
        }

        if (token.sizeInBytes() == 0)
        {
            continue;
        }

        transferCoding.name = token;
        transferCoding.type = HttpClientTransferCoding::parseName(token);
        return true;
    }
}

bool SC::HttpClientResponse::hasContentCoding(HttpClientContentCoding::Type type) const
{
    HttpClientContentCodingIterator iterator;
    HttpClientContentCoding         contentCoding;
    while (getNextContentCoding(iterator, contentCoding))
    {
        if (contentCoding.type == type)
        {
            return true;
        }
    }
    return false;
}

bool SC::HttpClientResponse::hasTransferCoding(HttpClientTransferCoding::Type type) const
{
    HttpClientTransferCodingIterator iterator;
    HttpClientTransferCoding         transferCoding;
    while (getNextTransferCoding(iterator, transferCoding))
    {
        if (transferCoding.type == type)
        {
            return true;
        }
    }
    return false;
}

const char* SC::HttpClientResponse::getProtocolName(Protocol protocol)
{
    switch (protocol)
    {
    case Protocol::Unknown: return "unknown";
    case Protocol::Http11: return "http/1.1";
    case Protocol::Http2: return "h2";
    }
    return "unknown";
}

SC::Result SC::HttpClient::init()
{
    SC_TRY_MSG(not initialized, "HttpClient: already initialized");
    SC_TRY(platformInit());
    initialized = true;
    return Result(true);
}

SC::Result SC::HttpClient::init(HttpClientCapabilities::Backend requiredBackend)
{
    SC_TRY_MSG(not initialized, "HttpClient: already initialized");
    SC_TRY(getCapabilities().requireBackend(requiredBackend));
    SC_TRY(platformInit());
    initialized = true;
    return Result(true);
}

SC::Result SC::HttpClient::init(Span<const HttpClientCapabilities::Feature> requiredFeatures)
{
    SC_TRY_MSG(not initialized, "HttpClient: already initialized");
    SC_TRY(getCapabilities().requireFeatures(requiredFeatures));
    SC_TRY(platformInit());
    initialized = true;
    return Result(true);
}

SC::Result SC::HttpClient::init(HttpClientCapabilities::Backend             requiredBackend,
                                Span<const HttpClientCapabilities::Feature> requiredFeatures)
{
    SC_TRY_MSG(not initialized, "HttpClient: already initialized");
    const HttpClientCapabilities capabilities = getCapabilities();
    SC_TRY(capabilities.requireBackend(requiredBackend));
    SC_TRY(capabilities.requireFeatures(requiredFeatures));
    SC_TRY(platformInit());
    initialized = true;
    return Result(true);
}

SC::Result SC::HttpClient::close()
{
    if (not initialized)
    {
        return Result(true);
    }
    SC_TRY(platformClose());
    initialized = false;
    return Result(true);
}

SC::Result SC::HttpClientOperation::init(HttpClient& clientValue, const HttpClientOperationMemory& memory)
{
    SC_TRY_MSG(clientValue.isInitialized(), "HttpClientOperation: client not initialized");
    SC_TRY_MSG(not initialized, "HttpClientOperation: already initialized");
    SC_TRY_MSG(memory.responseBuffers.sizeInElements() > 0, "HttpClientOperation: response buffers missing");
    SC_TRY_MSG(memory.eventQueue.sizeInElements() > 0, "HttpClientOperation: event queue missing");
    SC_TRY_MSG(memory.responseHeaders.sizeInBytes() > 0, "HttpClientOperation: response headers buffer missing");
    SC_TRY_MSG(memory.responseMetadata.sizeInBytes() > 0, "HttpClientOperation: response metadata buffer missing");

    client           = &clientValue;
    responseBuffers  = memory.responseBuffers;
    eventQueue       = memory.eventQueue;
    responseHeaders  = memory.responseHeaders;
    responseMetadata = memory.responseMetadata;
    backendScratch   = memory.backendScratch;

    if (memory.responseBufferMemory.sizeInBytes() > 0)
    {
        SC_TRY(sliceResponseBuffers(responseBuffers, memory.responseBufferMemory));
    }
    else
    {
        for (HttpClientResponseBuffer& buffer : responseBuffers)
        {
            SC_TRY_MSG(buffer.data.sizeInBytes() > 0, "HttpClientOperation: response buffer is empty");
            buffer.inUse = false;
        }
    }

    eventHead            = 0;
    eventTail            = 0;
    eventCount           = 0;
    requestBodyError     = Result(true);
    requestBodyBytesRead = 0;
    requestBodyFinished  = false;

    SC_TRY(platformInit());
    initialized = true;
    return Result(true);
}

SC::Result SC::HttpClientOperation::close()
{
    if (not initialized)
    {
        return Result(true);
    }

    if (requestInFlight)
    {
        (void)platformCancel();
    }

    SC_TRY(platformClose());

    client          = nullptr;
    currentResponse = nullptr;
    currentListener = nullptr;
    currentRequest  = {};
    notifier        = nullptr;

    eventMutex.lock();
    eventHead  = 0;
    eventTail  = 0;
    eventCount = 0;
    for (HttpClientResponseBuffer& buffer : responseBuffers)
    {
        buffer.inUse = false;
    }
    eventMutex.unlock();

    requestInFlight      = false;
    requestBodyFinished  = false;
    requestBodyError     = Result(true);
    requestBodyBytesRead = 0;
    initialized          = false;
    return Result(true);
}

SC::Result SC::HttpClientOperation::cancel()
{
    SC_TRY_MSG(initialized, "HttpClientOperation: not initialized");
    if (not requestInFlight)
    {
        return Result(true);
    }
    return platformCancel();
}

SC::Result SC::HttpClientOperation::start(const HttpClientRequest& request, HttpClientResponse& response,
                                          HttpClientOperationListener* listener)
{
    SC_TRY_MSG(initialized, "HttpClientOperation: not initialized");
    SC_TRY_MSG(not requestInFlight, "HttpClientOperation: request already in flight");
    SC_TRY(request.validate());
    SC_TRY(validateRequestOptionsSupported(request.options, HttpClient::getCapabilities()));

    resetResponseState(response);
    resetRequestBodyState();

    currentRequest  = request;
    currentResponse = &response;
    currentListener = listener;
    requestInFlight = true;

    const Result effectiveUrlResult = copyResponseEffectiveUrl(currentRequest.url);
    if (not effectiveUrlResult)
    {
        requestInFlight = false;
        return effectiveUrlResult;
    }

    const Result startResult = platformStart();
    if (not startResult)
    {
        requestInFlight = false;
    }
    return startResult;
}

SC::Result SC::HttpClientOperation::enqueueEvent(const HttpClientOperationEvent& event)
{
    eventMutex.lock();
    while (eventCount >= eventQueue.sizeInElements())
    {
        eventCV.wait(eventMutex);
    }
    eventQueue[eventTail] = event;
    eventTail             = (eventTail + 1) % eventQueue.sizeInElements();
    eventCount += 1;
    eventCV.signal();
    eventMutex.unlock();

    if (notifier != nullptr)
    {
        notifier->notifyHttpClientOperation(*this);
    }
    return Result(true);
}

bool SC::HttpClientOperation::dequeueEvent(HttpClientOperationEvent& event)
{
    eventMutex.lock();
    if (eventCount == 0)
    {
        eventMutex.unlock();
        return false;
    }
    event     = eventQueue[eventHead];
    eventHead = (eventHead + 1) % eventQueue.sizeInElements();
    eventCount -= 1;
    eventCV.signal();
    eventMutex.unlock();
    return true;
}

bool SC::HttpClientOperation::hasPendingEvents() const
{
    eventMutex.lock();
    const bool pending = eventCount > 0;
    eventMutex.unlock();
    return pending;
}

SC::Result SC::HttpClientOperation::allocateResponseBuffer(size_t minimumSizeInBytes, size_t& bufferIndex,
                                                           Span<char>& data)
{
    eventMutex.lock();
    bool supportsSize = false;
    for (const HttpClientResponseBuffer& buffer : responseBuffers)
    {
        if (buffer.data.sizeInBytes() >= minimumSizeInBytes)
        {
            supportsSize = true;
            break;
        }
    }
    if (not supportsSize)
    {
        eventMutex.unlock();
        return Result::Error("HttpClient: response buffer exhausted");
    }

    while (requestInFlight)
    {
        for (size_t idx = 0; idx < responseBuffers.sizeInElements(); ++idx)
        {
            HttpClientResponseBuffer& buffer = responseBuffers[idx];
            if (not buffer.inUse and buffer.data.sizeInBytes() >= minimumSizeInBytes)
            {
                buffer.inUse = true;
                bufferIndex  = idx;
                data         = buffer.data;
                eventMutex.unlock();
                return Result(true);
            }
        }
        eventCV.wait(eventMutex);
    }

    eventMutex.unlock();
    return Result::Error("HttpClient: request cancelled");
}

void SC::HttpClientOperation::releaseResponseBuffer(size_t bufferIndex)
{
    eventMutex.lock();
    if (bufferIndex < responseBuffers.sizeInElements())
    {
        responseBuffers[bufferIndex].inUse = false;
        eventCV.signal();
    }
    eventMutex.unlock();
}

SC::Result SC::HttpClientOperation::enqueueResponseDataCopy(Span<const char> data)
{
    size_t offset = 0;
    while (offset < data.sizeInBytes())
    {
        size_t     bufferIndex = 0;
        Span<char> writable;
        SC_TRY(allocateResponseBuffer(1, bufferIndex, writable));

        const size_t remaining = data.sizeInBytes() - offset;
        const size_t toCopy    = writable.sizeInBytes() < remaining ? writable.sizeInBytes() : remaining;
        memcpy(writable.data(), data.data() + offset, toCopy);
        enqueueResponseBuffer(bufferIndex, toCopy);
        offset += toCopy;
    }
    return Result(true);
}

void SC::HttpClientOperation::enqueueResponseHead()
{
    HttpClientOperationEvent event;
    event.type = HttpClientOperationEvent::Type::ResponseHead;
    (void)enqueueEvent(event);
}

void SC::HttpClientOperation::enqueueResponseBuffer(size_t bufferIndex, size_t size)
{
    HttpClientOperationEvent event;
    event.type        = HttpClientOperationEvent::Type::ResponseData;
    event.bufferIndex = bufferIndex;
    event.size        = size;
    (void)enqueueEvent(event);
}

void SC::HttpClientOperation::enqueueResponseComplete()
{
    HttpClientOperationEvent event;
    event.type = HttpClientOperationEvent::Type::ResponseComplete;
    (void)enqueueEvent(event);
}

void SC::HttpClientOperation::enqueueError(Result error)
{
    HttpClientOperationEvent event;
    event.type  = HttpClientOperationEvent::Type::Error;
    event.error = error;
    (void)enqueueEvent(event);
}

SC::Result SC::HttpClientOperation::processPendingEvents()
{
    HttpClientOperationEvent event;
    while (dequeueEvent(event))
    {
        switch (event.type)
        {
        case HttpClientOperationEvent::Type::ResponseHead:
            if (currentResponse != nullptr and currentListener != nullptr)
            {
                currentListener->onResponseHead(*currentResponse);
            }
            break;
        case HttpClientOperationEvent::Type::ResponseData: {
            if (event.bufferIndex >= responseBuffers.sizeInElements())
            {
                return Result::Error("HttpClient: invalid response buffer index");
            }
            const Span<char>       writable = responseBuffers[event.bufferIndex].data;
            const Span<const char> readable = {writable.data(), event.size};
            if (currentListener != nullptr)
            {
                currentListener->onResponseBody(readable);
            }
            releaseResponseBuffer(event.bufferIndex);
            break;
        }
        case HttpClientOperationEvent::Type::ResponseComplete:
            requestInFlight = false;
            eventMutex.lock();
            eventCV.signal();
            eventMutex.unlock();
            if (currentListener != nullptr)
            {
                currentListener->onResponseComplete();
            }
            resetRequestBodyState();
            break;
        case HttpClientOperationEvent::Type::Error:
            requestInFlight = false;
            eventMutex.lock();
            eventCV.signal();
            eventMutex.unlock();
            if (currentListener != nullptr)
            {
                currentListener->onError(event.error);
            }
            resetRequestBodyState();
            break;
        }
    }
    return Result(true);
}

SC::Result SC::HttpClientOperation::poll(uint32_t timeoutMilliseconds)
{
    SC_TRY_MSG(initialized, "HttpClientOperation: not initialized");

    SC_TRY(processPendingEvents());
    if (timeoutMilliseconds == 0 or not requestInFlight)
    {
        return Result(true);
    }

    eventMutex.lock();
    while (requestInFlight and eventCount == 0)
    {
        if (not eventCV.wait(eventMutex, timeoutMilliseconds))
        {
            eventMutex.unlock();
            return Result(true);
        }
    }
    eventMutex.unlock();

    return processPendingEvents();
}

void SC::HttpClientOperation::resetResponseState(HttpClientResponse& response)
{
    response.statusCode         = 0;
    response.headers            = responseHeaders;
    response.headersLength      = 0;
    response.negotiatedProtocol = HttpClientResponse::Protocol::Unknown;
    response.effectiveUrl       = {};
    response.redirectCount      = 0;
}

void SC::HttpClientOperation::resetRequestBodyState()
{
    requestBodyFinished  = false;
    requestBodyError     = Result(true);
    requestBodyBytesRead = 0;
}

bool SC::HttpClientOperation::isAutomaticRedirectEnabled() const
{
    return currentRequest.options.redirect.mode != HttpClientRequestRedirectOptions::NoRedirects;
}

bool SC::HttpClientOperation::canAutomaticRedirectRequestReplay() const
{
    if (currentRequest.body.isStreamed())
    {
        return currentRequest.body.canReplay;
    }
    return true;
}

SC::Result SC::HttpClientOperation::copyResponseEffectiveUrl(StringSpan url)
{
    SC_TRY_MSG(currentResponse != nullptr, "HttpClientOperation: missing response");
    SC_TRY_MSG(responseMetadata.sizeInBytes() >= url.sizeInBytes(), "HttpClient: response metadata buffer too small");

    if (url.sizeInBytes() > 0)
    {
        memcpy(responseMetadata.data(), url.toCharSpan().data(), url.sizeInBytes());
    }
    currentResponse->effectiveUrl = StringSpan({responseMetadata.data(), url.sizeInBytes()}, false, url.getEncoding());
    return Result(true);
}

size_t SC::HttpClientOperation::readRequestBodyChunk(Span<char> dest, Result& outError, bool& outEnd)
{
    outError = Result(true);
    outEnd   = false;

    if (not currentRequest.body.isStreamed())
    {
        outEnd = true;
        return 0;
    }

    if (requestBodyFinished)
    {
        outEnd = true;
        return 0;
    }

    if (currentRequest.body.provider == nullptr)
    {
        outError = Result::Error("HttpClientOperation: missing request body provider");
        return 0;
    }

    size_t bytesWritten = 0;
    bool   endReached   = false;

    SC_TRY_MSG(dest.sizeInBytes() > 0, "HttpClientOperation: request body destination is empty");

    outError = currentRequest.body.provider->pullRequestBody(dest, bytesWritten, endReached);
    if (not outError)
    {
        return 0;
    }

    if (bytesWritten > dest.sizeInBytes())
    {
        outError = Result::Error("HttpClientOperation: request body provider overflowed destination span");
        return 0;
    }

    if (bytesWritten == 0 and not endReached)
    {
        outError = Result::Error("HttpClientOperation: request body provider returned zero bytes without ending");
        return 0;
    }

    requestBodyBytesRead += bytesWritten;
    if (currentRequest.body.framing == HttpClientRequestBody::SizedStream and
        requestBodyBytesRead > currentRequest.body.sizeInBytes)
    {
        outError = Result::Error("HttpClientOperation: streamed body exceeded declared size");
        return 0;
    }

    if (endReached)
    {
        requestBodyFinished = true;
        if (currentRequest.body.framing == HttpClientRequestBody::SizedStream and
            requestBodyBytesRead != currentRequest.body.sizeInBytes)
        {
            outError = Result::Error("HttpClientOperation: streamed body ended before expected size");
            return 0;
        }
        if (bytesWritten == 0)
        {
            outEnd = true;
        }
    }

    return bytesWritten;
}

SC::Result SC::HttpClient::executeBlocking(const HttpClientRequest& request, HttpClientResponse& response,
                                           Span<char> bodyBuffer, size_t& bodyLength,
                                           const HttpClientOperationMemory& memory)
{
    bodyLength = 0;

    HttpClient client;
    SC_TRY(client.init());

    struct BlockingListener final : public HttpClientOperationListener
    {
        Span<char>           bodyBuffer;
        size_t*              bodyLength = nullptr;
        bool*                completed  = nullptr;
        Result*              finalRes   = nullptr;
        HttpClientOperation* operation  = nullptr;

        virtual void onResponseBody(Span<const char> data) override
        {
            const size_t remaining = bodyBuffer.sizeInBytes() - *bodyLength;
            const size_t toCopy    = data.sizeInBytes() < remaining ? data.sizeInBytes() : remaining;
            if (toCopy > 0)
            {
                memcpy(bodyBuffer.data() + *bodyLength, data.data(), toCopy);
                *bodyLength += toCopy;
            }
            if (toCopy != data.sizeInBytes())
            {
                *finalRes  = Result::Error("HttpClient: blocking response body buffer too small");
                *completed = true;
                (void)operation->cancel();
            }
        }

        virtual void onResponseComplete() override { *completed = true; }

        virtual void onError(Result error) override
        {
            *completed = true;
            if (*finalRes)
            {
                *finalRes = error;
            }
        }
    };

    HttpClientOperation operation;
    SC_TRY(operation.init(client, memory));

    bool   completed = false;
    Result finalRes(true);

    BlockingListener listener;
    listener.bodyBuffer = bodyBuffer;
    listener.bodyLength = &bodyLength;
    listener.completed  = &completed;
    listener.finalRes   = &finalRes;
    listener.operation  = &operation;

    SC_TRY(operation.start(request, response, &listener));

    while (not completed and operation.isRequestInFlight())
    {
        SC_TRY(operation.poll(50));
    }

    SC_TRY(operation.poll());
    SC_TRY(operation.close());
    SC_TRY(client.close());
    return finalRes;
}
