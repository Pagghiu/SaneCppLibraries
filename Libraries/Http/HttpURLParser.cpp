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

    // Check for query parameters or fragments before parsing host
    StringView      originalHost = host;
    auto            hostIt       = originalHost.getIterator<StringIteratorASCII>();
    StringCodePoint separator;
    const bool      hasQueryOrFragment = hostIt.advanceUntilMatchesAny({'?', '#'}, separator);

    // If there is no path but the host contains query/fragment (e.g. http://example.com?x=1)
    // trim the `host` to exclude the query/fragment before parsing hostname/port.
    if (not hasPath && hasQueryOrFragment)
    {
        // hostIt currently points at the separator ('?' or '#') within originalHost.
        // Build a new host view that stops before the separator.
        host = StringView::fromIteratorFromStart(hostIt, encoding);
    }

    SC_TRY(parseHost());
    if (not hasPath)
    {
        if (hasQueryOrFragment)
        {
            // There are query params or fragments in the host string
            if (separator == '?')
            {
                // Query parameters found
                pathname = "/";
                search   = StringView::fromIteratorUntilEnd(hostIt, encoding);
                // Check for fragment after query
                auto queryIt = hostIt;
                if (queryIt.advanceUntilMatches('#'))
                {
                    hash = StringView::fromIteratorUntilEnd(queryIt, encoding);
                }
            }
            else
            {
                // Fragment found
                pathname = "/";
                hash     = StringView::fromIteratorUntilEnd(hostIt, encoding);
            }
        }
        else
        {
            path     = "/";
            pathname = "/";
        }
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
    // Save the hostname part (before port) for parsing
    StringView hostnamePart = StringView::fromIteratorUntilEnd(it, encoding);

    // Parse hostname and port from hostnamePart
    StringIteratorASCII it2    = hostnamePart.getIterator<StringIteratorASCII>();
    StringIteratorASCII start2 = it2;

    StringCodePoint firstChar;
    if (it2.advanceRead(firstChar) && firstChar == '[')
    {
        // IPv6
        if (not it2.advanceUntilMatches(']'))
            return Result(false);
        (void)it2.stepForward(); // include ]
        hostname = StringView::fromIterators(start2, it2, encoding);
        // check if next char is ':' (port separator) without consuming it twice
        if (it2.match(':'))
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
            // Update host to include hostname and port
            host = StringView::fromIterators(start, it2, encoding);
        }
        else
        {
            // No port for IPv6
            host = StringView::fromIterators(start, it2, encoding);
        }
    }
    else
    {
        // Regular hostname
        it2 = start2;
        if (it2.advanceUntilMatches(':'))
        {
            hostname = StringView::fromIterators(start2, it2, encoding);
            // stepForward moves past ':' so port iterator starts at first digit
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
            // Advance it2 to end of port string for host calculation
            while (not it2.isAtEnd())
                (void)it2.stepForward();
            // Update host to include hostname and port
            host = StringView::fromIterators(start, it2, encoding);
        }
        else
        {
            // No port found
            hostname = hostnamePart;
            host     = hostnamePart;
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
    auto func = [this, userPassword](auto it) -> Result
    {
        auto start = it;
        if (it.advanceUntilMatches(':'))
        {
            username = StringView::fromIterators(start, it, encoding);
            (void)it.stepForward();
            password = StringView::fromIteratorUntilEnd(it, encoding);
        }
        else
        {
            username = userPassword;
            password = StringView();
        }
        return Result(true);
    };
    return userPassword.withIterator(func);
}
