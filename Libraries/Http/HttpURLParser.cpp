// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpURLParser.h"
#include "Internal/HttpStringIterator.h"
// Note: URL separators are always ASCII characters, allowing us to use HttpStringIterator even with UTF8 strings
// - `:` (colon) - code point 58
// - `/` (slash) - code point 47
// - `?` (question mark) - code point 63
// - `#` (hash) - code point 35
// - `@` (at sign) - code point 64
// - `[` and `]` (IPv6 brackets) - code points 91, 93

SC::Result SC::HttpURLParser::parse(StringSpan url)
{
    encoding = url.getEncoding();

    HttpStringIterator it    = url;
    HttpStringIterator start = it;

    // Protocol
    SC_TRY(it.advanceUntilMatches(':'));
    protocol = HttpStringIterator::fromIterators(start, it, encoding);
    SC_TRY(validateProtocol());
    SC_TRY(it.advanceIfMatches(':'));
    SC_TRY(it.advanceIfMatches('/'));
    SC_TRY(it.advanceIfMatches('/'));
    // hostname
    start = it;

    const bool hasPath = it.advanceUntilMatches('/');

    host = HttpStringIterator::fromIterators(start, it, encoding);

    // Check for query parameters or fragments before parsing host
    StringSpan originalHost = host;

    HttpStringIterator hostIt = originalHost;

    char       separator;
    const bool hasQueryOrFragment = hostIt.advanceUntilMatchesAny({'?', '#'}, separator);

    // If there is no path but the host contains query/fragment (e.g. http://example.com?x=1)
    // trim the `host` to exclude the query/fragment before parsing hostname/port.
    if (not hasPath && hasQueryOrFragment)
    {
        // hostIt currently points at the separator ('?' or '#') within originalHost.
        // Build a new host view that stops before the separator.
        host = HttpStringIterator::fromIteratorFromStart(hostIt, encoding);
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
                search   = HttpStringIterator::fromIteratorUntilEnd(hostIt, encoding);
                // Check for fragment after query
                auto queryIt = hostIt;
                if (queryIt.advanceUntilMatches('#'))
                {
                    hash = HttpStringIterator::fromIteratorUntilEnd(queryIt, encoding);
                }
            }
            else
            {
                // Fragment found
                pathname = "/";
                hash     = HttpStringIterator::fromIteratorUntilEnd(hostIt, encoding);
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
    path               = HttpStringIterator::fromIterators(start, it, encoding);
    SC_TRY(parsePath());
    if (hasHref)
    {
        hash = HttpStringIterator::fromIteratorUntilEnd(it, encoding);
    }
    return Result(true);
}

SC::Result SC::HttpURLParser::parsePath()
{
    HttpStringIterator it = path;
    if (it.advanceUntilMatches('?'))
    {
        pathname = HttpStringIterator::fromIteratorFromStart(it, encoding);
        search   = HttpStringIterator::fromIteratorUntilEnd(it, encoding);
    }
    else
    {
        pathname = path;
    }
    return validatePath();
}

SC::Result SC::HttpURLParser::parseHost()
{
    HttpStringIterator it = host;

    auto start = it;
    // Parse user@password if exists
    if (it.advanceUntilMatches('@'))
    {
        StringSpan userAndPassword = HttpStringIterator::fromIterators(start, it, encoding);
        SC_TRY(parseUserPassword(userAndPassword));
        (void)it.stepForward();
    }
    else
    {
        it = start;
    }
    start = it;
    // Save the hostname part (before port) for parsing
    StringSpan hostnamePart = HttpStringIterator::fromIteratorUntilEnd(it, encoding);

    // Parse hostname and port from hostnamePart
    HttpStringIterator it2    = hostnamePart;
    HttpStringIterator start2 = it2;

    char firstChar;
    if (it2.advanceRead(firstChar) && firstChar == '[')
    {
        // IPv6
        if (not it2.advanceUntilMatches(']'))
            return Result(false);
        (void)it2.stepForward(); // include ]
        hostname = HttpStringIterator::fromIterators(start2, it2, encoding);
        // check if next char is ':' (port separator) without consuming it twice
        if (it2.match(':'))
        {
            (void)it2.stepForward();
            StringSpan portString = HttpStringIterator::fromIteratorUntilEnd(it2, encoding);
            if (not portString.isEmpty())
            {
                int32_t value;
                SC_TRY(HttpStringIterator::parseInt32(portString, value));
                if (value < 0 || value > 65535)
                    return Result(false);
                port = static_cast<uint16_t>(value);
            }
            // Update host to include hostname and port
            host = HttpStringIterator::fromIterators(start, it2, encoding);
        }
        else
        {
            // No port for IPv6
            host = HttpStringIterator::fromIterators(start, it2, encoding);
        }
    }
    else
    {
        // Regular hostname
        it2 = start2;
        if (it2.advanceUntilMatches(':'))
        {
            hostname = HttpStringIterator::fromIterators(start2, it2, encoding);
            // stepForward moves past ':' so port iterator starts at first digit
            (void)it2.stepForward();
            StringSpan portString = HttpStringIterator::fromIteratorUntilEnd(it2, encoding);
            if (not portString.isEmpty())
            {
                int32_t value;
                SC_TRY(HttpStringIterator::parseInt32(portString, value));
                if (value < 0 || value > 65535)
                    return Result(false);
                port = static_cast<uint16_t>(value);
            }
            // Advance it2 to end of port string for host calculation
            while (not it2.isAtEnd())
                (void)it2.stepForward();
            // Update host to include hostname and port
            host = HttpStringIterator::fromIterators(start, it2, encoding);
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
    if (HttpStringIterator::equalsIgnoreCase(protocol, "http"))
    {
        port = 80;
        return Result(true);
    }
    else if (HttpStringIterator::equalsIgnoreCase(protocol, "https"))
    {
        port = 443;
        return Result(true);
    }

    return Result(false);
}

SC::Result SC::HttpURLParser::validatePath()
{
    // TODO: Improve validatePath
    return Result(not HttpStringIterator::containsCodePoint(pathname, ' '));
}

SC::Result SC::HttpURLParser::validateHost()
{
    // TODO: Improve validateHost
    return Result(not host.isEmpty() and
                  ((HttpStringIterator::startsWith(hostname, "[") and HttpStringIterator::endsWith(hostname, "]")) or
                   HttpStringIterator::containsCodePoint(host, '.') or hostname == "localhost"));
}

SC::Result SC::HttpURLParser::parseUserPassword(StringSpan userPassword)
{
    HttpStringIterator it    = userPassword;
    auto               start = it;
    if (it.advanceUntilMatches(':'))
    {
        username = HttpStringIterator::fromIterators(start, it, encoding);
        (void)it.stepForward();
        password = HttpStringIterator::fromIteratorUntilEnd(it, encoding);
    }
    else
    {
        username = userPassword;
        password = StringSpan();
    }
    return Result(true);
}
