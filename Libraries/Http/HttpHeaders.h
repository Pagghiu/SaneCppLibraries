// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/StringSpan.h"
#include "HttpExport.h"

namespace SC
{
//! @addtogroup group_http
//! @{

/// @brief A zero-copy key/value view used by lightweight HTTP header helpers.
struct SC_HTTP_EXPORT HttpHeaderKeyValue
{
    StringSpan name;
    StringSpan value;
    bool       hasValue = false;
};

/// @brief Iterates a request Cookie header as semicolon-delimited name/value pairs.
struct SC_HTTP_EXPORT HttpCookieIterator
{
    explicit HttpCookieIterator(StringSpan cookieHeader);

    bool next(HttpHeaderKeyValue& pair);

  private:
    StringSpan header;
    size_t     cursor = 0;
};

/// @brief Zero-copy view of an Authorization header split into scheme and credentials.
struct SC_HTTP_EXPORT HttpAuthorizationView
{
    StringSpan scheme;
    StringSpan credentials;

    Result parse(StringSpan authorizationHeader);
    bool   isBearer() const;
    bool   isBasic() const;
};

/// @brief Parses a Bearer Authorization header and returns the raw token slice.
SC_HTTP_EXPORT Result HttpParseBearerToken(StringSpan authorizationHeader, StringSpan& token);

/// @brief Parses Basic Authorization credentials into caller-provided decoded storage.
SC_HTTP_EXPORT Result HttpParseBasicCredentials(StringSpan authorizationHeader, Span<char> storage,
                                                StringSpan& username, StringSpan& password);

//! @}
} // namespace SC
