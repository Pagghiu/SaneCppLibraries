// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "HttpError.h"

namespace SC
{
/// @brief Format an Http-owned error into caller-provided UTF-8 storage.
/// @details Include this optional header only where canonical English text is needed.
inline ResultErrorFormat formatHttpError(HttpError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case HttpError::MalformedPercentEscape: formatter.append("URL contains a malformed percent escape"); break;
    case HttpError::DecodedOutputTooSmall: formatter.append("Decoded URL output buffer is too small"); break;
    case HttpError::RequestTargetEmpty: formatter.append("HTTP request target is empty"); break;
    case HttpError::RequestTargetWhitespace: formatter.append("HTTP request target contains whitespace"); break;
    case HttpError::UnsupportedRequestTargetForm: formatter.append("HTTP request target form is unsupported"); break;
    case HttpError::MalformedURL: formatter.append("URL syntax is malformed"); break;
    case HttpError::UnsupportedProtocol: formatter.append("URL protocol is unsupported"); break;
    case HttpError::InvalidURLPath: formatter.append("URL path contains invalid whitespace"); break;
    case HttpError::InvalidURLHost: formatter.append("URL host is invalid"); break;
    case HttpError::InvalidURLUserInfo: formatter.append("URL user information is invalid"); break;
    case HttpError::InvalidIPv6Host: formatter.append("URL IPv6 host is invalid"); break;
    case HttpError::InvalidURLPort: formatter.append("URL port is invalid"); break;
    case HttpError::AllowHeaderOutputTooSmall: formatter.append("Allow header output buffer is too small"); break;
    case HttpError::MultipartDispositionEmpty: formatter.append("Multipart Content-Disposition is empty"); break;
    case HttpError::MultipartDispositionTypeEmpty: formatter.append("Multipart disposition type is empty"); break;
    case HttpError::MultipartBoundaryInvalid: formatter.append("Multipart boundary is empty"); break;
    case HttpError::MultipartBoundaryTooLong: formatter.append("Multipart boundary is too long"); break;
    case HttpError::MultipartBoundaryCandidateTooLong:
        formatter.append("Multipart boundary candidate is too long");
        break;
    case HttpError::MultipartMalformedSyntax: formatter.append("Multipart syntax is malformed"); break;
    case HttpError::MultipartSpanInvalid: formatter.append("Multipart parser span is invalid"); break;
    case HttpError::AuthorizationCredentialsMissing: formatter.append("Authorization credentials are missing"); break;
    case HttpError::AuthorizationNotBearer: formatter.append("Authorization scheme is not Bearer"); break;
    case HttpError::AuthorizationNotBasic: formatter.append("Authorization scheme is not Basic"); break;
    case HttpError::BasicBase64Incomplete: formatter.append("Basic credentials have incomplete base64"); break;
    case HttpError::BasicBase64Invalid: formatter.append("Basic credentials have invalid base64"); break;
    case HttpError::BasicBase64PaddingInvalid: formatter.append("Basic credentials have invalid base64 padding"); break;
    case HttpError::BasicBase64TrailingData: formatter.append("Basic credentials have trailing base64 data"); break;
    case HttpError::BasicDecodeOutputTooSmall: formatter.append("Basic credential output buffer is too small"); break;
    case HttpError::BasicPasswordSeparatorMissing:
        formatter.append("Basic credentials lack a password separator");
        break;
    case HttpError::BasicUsernameContainsColon: formatter.append("Basic username contains a colon"); break;
    case HttpError::AuthorizationOutputTooSmall: formatter.append("Authorization output buffer is too small"); break;
    case HttpError::BearerTokenEmpty: formatter.append("Bearer token is empty"); break;
    case HttpError::BasicCredentialsEmpty: formatter.append("Basic credentials are empty"); break;
    case HttpError::SetCookieHeaderEmpty: formatter.append("Set-Cookie header is empty"); break;
    case HttpError::SetCookieNameValueMissing: formatter.append("Set-Cookie name/value pair is missing"); break;
    case HttpError::SetCookieNameEmpty: formatter.append("Set-Cookie name is empty"); break;
    case HttpError::SetCookieOutputTooSmall: formatter.append("Set-Cookie output buffer is too small"); break;
    case HttpError::CacheControlConflictingVisibility:
        formatter.append("Cache-Control cannot be both public and private");
        break;
    case HttpError::CacheControlOutputTooSmall: formatter.append("Cache-Control output buffer is too small"); break;
    case HttpError::CacheControlNoDirectives: formatter.append("Cache-Control has no directives"); break;
    case HttpError::HeaderStorageExhausted: formatter.append("HTTP header storage is exhausted"); break;
    case HttpError::HeaderSizeLimitExceeded: formatter.append("HTTP header size limit is exceeded"); break;
    case HttpError::HeaderTokenLimitExceeded: formatter.append("HTTP header token limit is exceeded"); break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatHttpError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != HttpResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatHttpError(static_cast<HttpError>(result.errorValue()), output);
}
} // namespace SC
