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
