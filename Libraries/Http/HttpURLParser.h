// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/StringSpan.h"

namespace SC
{
//! @addtogroup group_http
//! @{

/// @brief Parse an URL splitting it into its base components
struct SC_COMPILER_EXPORT HttpURLParser
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
