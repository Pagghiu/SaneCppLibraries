// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "HttpClientError.h"

namespace SC
{
/// @brief Format an HttpClient-owned error into caller-provided UTF-8 storage.
/// @details Include this optional header only where canonical English text is needed.
inline ResultErrorFormat formatHttpClientError(HttpClientError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case HttpClientError::RequestHeaderNameEmpty: formatter.append("HTTP client request header name is empty"); break;
    case HttpClientError::RequestHeaderNameInvalid:
        formatter.append("HTTP client request header name is invalid");
        break;
    case HttpClientError::RequestHeaderValueInvalid:
        formatter.append("HTTP client request header value is invalid");
        break;
    case HttpClientError::RequestTransferEncodingForbidden:
        formatter.append("HTTP client request Transfer-Encoding is controlled by body framing");
        break;
    case HttpClientError::RequestChunkedContentLengthConflict:
        formatter.append("HTTP client chunked request body cannot use Content-Length");
        break;
    case HttpClientError::RequestFixedBodyProviderUnexpected:
        formatter.append("HTTP client fixed request body cannot use a provider");
        break;
    case HttpClientError::RequestFixedBodySizeMismatch:
        formatter.append("HTTP client fixed request body size does not match its bytes");
        break;
    case HttpClientError::RequestStreamInlineBytesUnexpected:
        formatter.append("HTTP client streamed request body cannot use inline bytes");
        break;
    case HttpClientError::RequestStreamProviderMissing:
        formatter.append("HTTP client streamed request body requires a provider");
        break;
    case HttpClientError::RequestChunkedDeclaredSizeInvalid:
        formatter.append("HTTP client chunked request body cannot declare a fixed size");
        break;
    case HttpClientError::RequestBodyFramingInvalid:
        formatter.append("HTTP client request body framing is invalid");
        break;
    case HttpClientError::RequestUrlEmpty: formatter.append("HTTP client request URL is empty"); break;
    case HttpClientError::RequestUrlUnsafe:
        formatter.append("HTTP client request URL contains whitespace or control bytes");
        break;
    case HttpClientError::RequestUrlSchemeUnsupported:
        formatter.append("HTTP client request URL must use HTTP or HTTPS");
        break;
    case HttpClientError::RequestUrlHostEmpty: formatter.append("HTTP client request URL host is empty"); break;
    case HttpClientError::RequestMethodInvalid: formatter.append("HTTP client request method is invalid"); break;
    case HttpClientError::RequestRedirectModeInvalid: formatter.append("HTTP client redirect mode is invalid"); break;
    case HttpClientError::RequestProtocolPreferenceInvalid:
        formatter.append("HTTP client protocol preference is invalid");
        break;
    case HttpClientError::RequestTlsCaPathInvalid:
        formatter.append("HTTP client TLS CA path contains control bytes");
        break;
    case HttpClientError::RequestRedirectMethodInvalid:
        formatter.append("HTTP client redirect policy requires GET or HEAD");
        break;
    case HttpClientError::RequestRedirectBodyNotReplayable:
        formatter.append("HTTP client automatic redirects require a replayable request body");
        break;
    case HttpClientError::ProxyModeInvalid: formatter.append("HTTP client proxy mode is invalid"); break;
    case HttpClientError::ProxyUrlEmpty: formatter.append("HTTP client proxy URL is empty"); break;
    case HttpClientError::ProxyUrlSchemeUnsupported: formatter.append("HTTP client proxy URL must use HTTP"); break;
    case HttpClientError::ProxyUrlUnsafe:
        formatter.append("HTTP client proxy URL contains whitespace or control bytes");
        break;
    case HttpClientError::ProxyUrlHostEmpty: formatter.append("HTTP client proxy URL host is empty"); break;
    case HttpClientError::ProxyUrlPathUnsupported:
        formatter.append("HTTP client proxy URL must not include a path, query, or fragment");
        break;
    case HttpClientError::ProxyAuthorizationInvalid:
        formatter.append("HTTP client proxy authorization contains invalid bytes");
        break;
    case HttpClientError::ProxyBypassListInvalid:
        formatter.append("HTTP client proxy bypass list contains invalid bytes");
        break;
    case HttpClientError::ProxyUrlWithoutHttpMode:
        formatter.append("HTTP client proxy URL requires HTTP proxy mode");
        break;
    case HttpClientError::ProxyAuthorizationWithoutHttpMode:
        formatter.append("HTTP client proxy authorization requires HTTP proxy mode");
        break;
    case HttpClientError::ProxyBypassListWithoutHttpMode:
        formatter.append("HTTP client proxy bypass list requires HTTP proxy mode");
        break;
    case HttpClientError::RedirectPolicyUnsupported:
        formatter.append("HTTP client redirect policy is unsupported");
        break;
    case HttpClientError::Http11OnlyUnsupported:
        formatter.append("HTTP client HTTP/1.1-only policy is unsupported");
        break;
    case HttpClientError::Http2PreferredUnsupported:
        formatter.append("HTTP client HTTP/2 preference is unsupported");
        break;
    case HttpClientError::Http2RequiredUnsupported:
        formatter.append("HTTP client required HTTP/2 policy is unsupported");
        break;
    case HttpClientError::TlsDisablePeerVerificationUnsupported:
        formatter.append("HTTP client disabling TLS peer verification is unsupported");
        break;
    case HttpClientError::TlsCustomCaPathUnsupported:
        formatter.append("HTTP client custom TLS CA path is unsupported");
        break;
    case HttpClientError::NoProxyPolicyUnsupported:
        formatter.append("HTTP client no-proxy policy is unsupported");
        break;
    case HttpClientError::HttpProxyPolicyUnsupported:
        formatter.append("HTTP client HTTP proxy policy is unsupported");
        break;
    case HttpClientError::ProxyAuthorizationUnsupported:
        formatter.append("HTTP client proxy authorization is unsupported");
        break;
    case HttpClientError::ProxyBypassListUnsupported:
        formatter.append("HTTP client proxy bypass list is unsupported");
        break;
    case HttpClientError::RequiredBackendUnavailable:
        formatter.append("HTTP client required backend is unavailable");
        break;
    case HttpClientError::RequiredFeatureUnsupported:
        formatter.append("HTTP client required feature is unsupported");
        break;
    case HttpClientError::ClientAlreadyInitialized: formatter.append("HTTP client is already initialized"); break;
    case HttpClientError::OperationClientNotInitialized:
        formatter.append("HTTP client operation requires an initialized client");
        break;
    case HttpClientError::OperationAlreadyInitialized:
        formatter.append("HTTP client operation is already initialized");
        break;
    case HttpClientError::ResponseBuffersMissing: formatter.append("HTTP client response buffers are missing"); break;
    case HttpClientError::ResponseBufferMemoryTooSmall:
        formatter.append("HTTP client response buffer memory is too small");
        break;
    case HttpClientError::ResponseBufferEmpty: formatter.append("HTTP client response buffer is empty"); break;
    case HttpClientError::OperationEventQueueMissing:
        formatter.append("HTTP client operation event queue is missing");
        break;
    case HttpClientError::OperationResponseHeadersMissing:
        formatter.append("HTTP client operation response header storage is missing");
        break;
    case HttpClientError::OperationResponseMetadataMissing:
        formatter.append("HTTP client operation response metadata storage is missing");
        break;
    case HttpClientError::OperationNotInitialized: formatter.append("HTTP client operation is not initialized"); break;
    case HttpClientError::OperationRequestInFlight:
        formatter.append("HTTP client operation already has a request in flight");
        break;
    case HttpClientError::RequestBodyDestinationEmpty:
        formatter.append("HTTP client request body destination is empty");
        break;
    case HttpClientError::RequestBodyProviderOverflow:
        formatter.append("HTTP client request body provider exceeded its destination");
        break;
    case HttpClientError::RequestBodyProviderStalled:
        formatter.append("HTTP client request body provider made no progress");
        break;
    case HttpClientError::RequestBodyDeclaredSizeExceeded:
        formatter.append("HTTP client request body exceeded its declared size");
        break;
    case HttpClientError::RequestBodyDeclaredSizeIncomplete:
        formatter.append("HTTP client request body ended before its declared size");
        break;
    case HttpClientError::ResponseBufferCapacityInsufficient:
        formatter.append("HTTP client response buffer cannot hold the requested data");
        break;
    case HttpClientError::RequestCancelled: formatter.append("HTTP client request was cancelled"); break;
    case HttpClientError::ResponseBufferIndexInvalid:
        formatter.append("HTTP client response buffer index is invalid");
        break;
    case HttpClientError::OperationResponseMissing:
        formatter.append("HTTP client operation response is missing");
        break;
    case HttpClientError::ResponseMetadataTooSmall:
        formatter.append("HTTP client response metadata buffer is too small");
        break;
    case HttpClientError::BlockingResponseBodyBufferTooSmall:
        formatter.append("HTTP client blocking response body buffer is too small");
        break;
    case HttpClientError::PlatformUnsupported: formatter.append("HTTP client platform is unsupported"); break;
    case HttpClientError::ContentCodingListEmpty: formatter.append("HTTP client content coding list is empty"); break;
    case HttpClientError::ContentCodingUnknown: formatter.append("HTTP client content coding is unknown"); break;
    case HttpClientError::AcceptEncodingOutputTooSmall:
        formatter.append("HTTP client Accept-Encoding output buffer is too small");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatHttpClientError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != HttpClientResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatHttpClientError(static_cast<HttpClientError>(result.errorValue()), output);
}
} // namespace SC
