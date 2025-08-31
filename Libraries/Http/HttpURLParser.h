// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"

namespace SC
{
struct SC_COMPILER_EXPORT HttpURLParser;
} // namespace SC

//! @addtogroup group_http
//! @{

/// @brief Parse an URL splitting it into its base components
struct SC::HttpURLParser
{
    StringView protocol; ///< Returns `http` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView username; ///< Returns `user` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView password; ///< Returns `pass` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView hostname; ///< Returns `site.com` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    uint16_t   port;     ///< Returns `80` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView host;     ///< Returns `site.com:80` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView pathname; ///< Returns `/pa/th` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView path;     ///< Returns `/pa/th?q=val` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView search;   ///< Returns `?q=val` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView hash;     ///< Returns `#hash` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)

    /// @brief Parse StringView representing an URL
    /// @param url The url to be parsed
    /// @return Valid Result if parse was successful
    Result parse(StringView url);

  private:
    Result parsePath();
    Result parseHost();
    Result validateProtocol();
    Result validatePath();
    Result validateHost();
    Result parseUserPassword(StringView userPassword);
    struct Internal;
};

//! @}
