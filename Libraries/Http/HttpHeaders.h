// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
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

//! @}
} // namespace SC
