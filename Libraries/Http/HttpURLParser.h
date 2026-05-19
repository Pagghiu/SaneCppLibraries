// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/StringSpan.h"
#include "HttpExport.h"

namespace SC
{
//! @addtogroup group_http
//! @{

/// @brief One raw query string item parsed from a URL search component.
struct SC_HTTP_EXPORT HttpURLQueryItem
{
    StringSpan name;
    StringSpan value;
    bool       hasValue = false;
};

/// @brief Iterates raw query string items without allocation or percent-decoding.
struct SC_HTTP_EXPORT HttpURLQueryIterator
{
    explicit HttpURLQueryIterator(StringSpan search);

    bool next(HttpURLQueryItem& item);

  private:
    StringSpan search;
    size_t     cursor = 0;
};

/// @brief Parse an URL splitting it into its base components
struct SC_HTTP_EXPORT HttpURLParser
{
    StringSpan protocol; ///< Returns `http` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringSpan username; ///< Returns `user` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringSpan password; ///< Returns `pass` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringSpan hostname; ///< Returns `site.com` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    uint16_t   port;     ///< Returns `80` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringSpan host;     ///< Returns `site.com:80` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringSpan pathname; ///< Returns `/pa/th` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringSpan path;     ///< Returns `/pa/th?q=val` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringSpan search;   ///< Returns `?q=val` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringSpan hash;     ///< Returns `#hash` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)

    /// @brief Parse StringSpan representing an URL
    /// @param url The url to be parsed
    /// @return Valid Result if parse was successful
    Result parse(StringSpan url);

    /// @brief Finds first raw query value matching name in this URL search component.
    bool getQueryValue(StringSpan name, StringSpan& value) const;

    /// @brief Finds first raw query value matching name in a search component, with or without leading '?'.
    static bool getQueryValue(StringSpan search, StringSpan name, StringSpan& value);

  private:
    StringEncoding encoding;

    Result parsePath();
    Result parseHost();
    Result validateProtocol();
    Result validatePath();
    Result validateHost();
    Result parseUserPassword(StringSpan userPassword);
    struct Internal;
};
//! @}

} // namespace SC
