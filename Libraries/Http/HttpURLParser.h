// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Strings/StringView.h"

namespace SC
{
struct HttpURLParser;
} // namespace SC

struct SC::HttpURLParser
{
    // Example: http://user:pass@site.com:80/pa/th?q=val#hash
    StringView protocol; // http
    StringView username; // user
    StringView password; // pass
    StringView hostname; // site.com
    uint16_t   port;     // 80
    StringView host;     // site.com:80
    StringView pathname; // /pa/th
    StringView path;     // /pa/th?q=val
    StringView search;   // ?q=val
    StringView hash;     // #hash

    [[nodiscard]] Result parse(StringView url);

  private:
    [[nodiscard]] Result parsePath();
    [[nodiscard]] Result parseHost();
    [[nodiscard]] Result validateProtocol();
    [[nodiscard]] Result validatePath();
    [[nodiscard]] Result validateHost();
    [[nodiscard]] Result parseUserPassword(StringView userPassowrd);
};
