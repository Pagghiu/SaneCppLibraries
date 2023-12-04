// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"

namespace SC
{
namespace Http
{
struct URLParser;
}
} // namespace SC

//! @addtogroup group_http
//! @{

/// @brief Parse an URL splitting it into its base components
struct SC::Http::URLParser
{
    StringView protocol; // `http` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView username; // `user` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView password; // `pass` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView hostname; // `site.com` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    uint16_t   port;     // `80` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView host;     // `site.com:80` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView pathname; // `/pa/th` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView path;     // `/pa/th?q=val` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView search;   // `?q=val` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)
    StringView hash;     // `#hash` (from `http://user:pass@site.com:80/pa/th?q=val#hash`)

    /// @brief Parse StringView representing an URL
    /// @param url The url to be parsed
    /// @return Valid Result if parse was successfull
    [[nodiscard]] Result parse(StringView url);

  private:
    [[nodiscard]] Result parsePath();
    [[nodiscard]] Result parseHost();
    [[nodiscard]] Result validateProtocol();
    [[nodiscard]] Result validatePath();
    [[nodiscard]] Result validateHost();
    [[nodiscard]] Result parseUserPassword(StringView userPassowrd);
};

//! @}
