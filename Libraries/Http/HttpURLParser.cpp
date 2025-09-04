// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpURLParser.h"
#include <ctype.h>

// Note: URL separators are always ASCII characters, allowing us to use StringIteratorASCII even with UTF8 strings
// - `:` (colon) - code point 58
// - `/` (slash) - code point 47
// - `?` (question mark) - code point 63
// - `#` (hash) - code point 35
// - `@` (at sign) - code point 64
// - `[` and `]` (IPv6 brackets) - code points 91, 93

struct SC::HttpURLParser::Internal
{
    static bool equalsIgnoreCase(SC::StringView a, SC::StringView b)
    {
        if (a.sizeInBytes() != b.sizeInBytes())
            return false;
        auto it1 = a.getIterator<SC::StringIteratorASCII>();
        auto it2 = b.getIterator<SC::StringIteratorASCII>();

        SC::StringCodePoint c1, c2;
        while (it1.advanceRead(c1) and it2.advanceRead(c2))
        {
            if (::tolower(static_cast<int>(c1)) != ::tolower(static_cast<int>(c2)))
                return false;
        }
        return true;
    }
};

SC::Result SC::HttpURLParser::parse(StringView url)
{
    encoding = url.getEncoding();

    auto it    = url.getIterator<StringIteratorASCII>();
    auto start = it;

    // Protocol
    SC_TRY(it.advanceUntilMatches(':'));
    protocol = StringView::fromIterators(start, it, encoding);
    SC_TRY(validateProtocol());
    SC_TRY(it.advanceIfMatches(':'));
    SC_TRY(it.advanceIfMatches('/'));
    SC_TRY(it.advanceIfMatches('/'));
    // hostname
    start = it;

    const bool hasPath = it.advanceUntilMatches('/');

    host = StringView::fromIterators(start, it, encoding);
    SC_TRY(parseHost());
    if (not hasPath)
    {
        path     = "/";
        pathname = "/";
        return Result(true);
    }
    // path + hash
    start              = it;
    const bool hasHref = it.advanceUntilMatches('#');
    path               = StringView::fromIterators(start, it, encoding);
    SC_TRY(parsePath());
    if (hasHref)
    {
        hash = StringView::fromIteratorUntilEnd(it, encoding);
    }
    return Result(true);
}

SC::Result SC::HttpURLParser::parsePath()
{
    StringIteratorASCII it = path.getIterator<StringIteratorASCII>();
    if (it.advanceUntilMatches('?'))
    {
        pathname = StringView::fromIteratorFromStart(it, encoding);
        search   = StringView::fromIteratorUntilEnd(it, encoding);
    }
    else
    {
        pathname = path;
    }
    return validatePath();
}

SC::Result SC::HttpURLParser::parseHost()
{
    StringIteratorASCII it = host.getIterator<StringIteratorASCII>();

    auto start = it;
    // Parse user@password if exists
    if (it.advanceUntilMatches('@'))
    {
        StringView userAndPassword = StringView::fromIterators(start, it, encoding);
        SC_TRY(parseUserPassword(userAndPassword));
        (void)it.stepForward();
    }
    else
    {
        it = start;
    }
    start = it;
    host  = StringView::fromIteratorUntilEnd(it, encoding);

    // Parse hostname and port from host
    StringIteratorASCII it2    = host.getIterator<StringIteratorASCII>();
    StringIteratorASCII start2 = it2;

    StringCodePoint firstChar;
    if (it2.advanceRead(firstChar) && firstChar == '[')
    {
        // IPv6
        if (not it2.advanceUntilMatches(']'))
            return Result(false);
        (void)it2.stepForward(); // include ]
        hostname = StringView::fromIterators(start2, it2, encoding);
        if (it2.advanceRead(firstChar) && firstChar == ':')
        {
            (void)it2.stepForward();
            StringView portString = StringView::fromIteratorUntilEnd(it2, encoding);
            if (not portString.isEmpty())
            {
                int32_t value;
                SC_TRY(portString.parseInt32(value));
                if (value < 0 || value > 65535)
                    return Result(false);
                port = static_cast<uint16_t>(value);
            }
        }
    }
    else
    {
        // Regular hostname
        it2 = start2;
        if (it2.advanceUntilMatches(':'))
        {
            hostname = StringView::fromIterators(start2, it2, encoding);
            (void)it2.stepForward();
            StringView portString = StringView::fromIteratorUntilEnd(it2, encoding);
            if (not portString.isEmpty())
            {
                int32_t value;
                SC_TRY(portString.parseInt32(value));
                if (value < 0 || value > 65535)
                    return Result(false);
                port = static_cast<uint16_t>(value);
            }
        }
        else
        {
            hostname = host;
        }
    }

    return validateHost();
}

SC::Result SC::HttpURLParser::validateProtocol()
{
    // TODO: Expand supported protocols
    if (Internal::equalsIgnoreCase(protocol, "http"))
    {
        port = 80;
        return Result(true);
    }
    else if (Internal::equalsIgnoreCase(protocol, "https"))
    {
        port = 443;
        return Result(true);
    }

    return Result(false);
}

SC::Result SC::HttpURLParser::validatePath()
{
    // TODO: Improve validatePath
    return Result(not pathname.containsCodePoint(' '));
}

SC::Result SC::HttpURLParser::validateHost()
{
    // TODO: Improve validateHost
    return Result(not host.isEmpty() and ((hostname.startsWith("[") and hostname.endsWith("]")) or
                                          host.containsCodePoint('.') or hostname == "localhost"));
}

SC::Result SC::HttpURLParser::parseUserPassword(StringView userPassword)
{
    StringViewTokenizer tokenizer(userPassword);
    SC_TRY(tokenizer.tokenizeNext({':'}, StringViewTokenizer::Options::SkipEmpty));
    username = tokenizer.component;
    SC_TRY(tokenizer.tokenizeNext({}, StringViewTokenizer::Options::SkipEmpty));
    password = tokenizer.component;
    return Result(true);
}
