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

/// @brief Iterates semicolon-delimited Set-Cookie attributes after the first name/value pair.
struct SC_HTTP_EXPORT HttpSetCookieAttributeIterator
{
    explicit HttpSetCookieAttributeIterator(StringSpan attributes);

    bool next(HttpHeaderKeyValue& attribute);

  private:
    StringSpan attributes;
    size_t     cursor = 0;
};

/// @brief Zero-copy view of one Set-Cookie header value.
struct SC_HTTP_EXPORT HttpSetCookieView
{
    StringSpan name;
    StringSpan value;
    StringSpan attributes;

    StringSpan path;
    StringSpan domain;
    StringSpan expires;
    StringSpan maxAge;
    StringSpan sameSite;

    bool hasMaxAge = false;
    bool secure    = false;
    bool httpOnly  = false;

    Result parse(StringSpan setCookieHeader);
};

/// @brief Writes a Set-Cookie header value into caller-provided storage.
struct SC_HTTP_EXPORT HttpSetCookieBuilder
{
    StringSpan name;
    StringSpan value;
    StringSpan path;
    StringSpan domain;
    StringSpan expires;
    StringSpan maxAge;
    StringSpan sameSite;

    bool secure   = false;
    bool httpOnly = false;

    Result writeTo(Span<char> storage, StringSpan& output) const;
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
