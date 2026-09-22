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
