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

SC::HttpURLQueryIterator::HttpURLQueryIterator(StringSpan query) : search(query)
{
    if (search.sizeInBytes() > 0 and search.bytesWithoutTerminator()[0] == '?')
    {
        cursor = 1;
    }
}

bool SC::HttpURLQueryIterator::next(HttpURLQueryItem& item)
{
    item = {};
    if (cursor >= search.sizeInBytes())
    {
        return false;
    }

    const char*  data      = search.bytesWithoutTerminator();
    const size_t nameStart = cursor;
    while (cursor < search.sizeInBytes() and data[cursor] != '=' and data[cursor] != '&')
    {
        cursor++;
    }
    const size_t nameEnd = cursor;

    const bool hasValue = cursor < search.sizeInBytes() and data[cursor] == '=';
    if (hasValue)
    {
        cursor++;
    }

    const size_t valueStart = cursor;
    while (cursor < search.sizeInBytes() and data[cursor] != '&')
    {
        cursor++;
    }
    const size_t valueEnd = cursor;
    if (cursor < search.sizeInBytes() and data[cursor] == '&')
    {
        cursor++;
    }

    item.name     = {{data + nameStart, nameEnd - nameStart}, false, search.getEncoding()};
    item.value    = {{data + valueStart, valueEnd - valueStart}, false, search.getEncoding()};
    item.hasValue = hasValue;
    return true;
}

SC::HttpFormUrlEncodedIterator::HttpFormUrlEncodedIterator(StringSpan formBody) : body(formBody) {}

bool SC::HttpFormUrlEncodedIterator::next(HttpURLQueryItem& item)
{
    item = {};
    if (cursor >= body.sizeInBytes())
    {
        return false;
    }

    const char*  data      = body.bytesWithoutTerminator();
    const size_t nameStart = cursor;
    while (cursor < body.sizeInBytes() and data[cursor] != '=' and data[cursor] != '&')
    {
        cursor++;
    }
    const size_t nameEnd = cursor;

    const bool hasValue = cursor < body.sizeInBytes() and data[cursor] == '=';
    if (hasValue)
    {
        cursor++;
    }

    const size_t valueStart = cursor;
    while (cursor < body.sizeInBytes() and data[cursor] != '&')
    {
        cursor++;
    }
    const size_t valueEnd = cursor;
    if (cursor < body.sizeInBytes() and data[cursor] == '&')
    {
        cursor++;
    }

    item.name     = {{data + nameStart, nameEnd - nameStart}, false, body.getEncoding()};
    item.value    = {{data + valueStart, valueEnd - valueStart}, false, body.getEncoding()};
    item.hasValue = hasValue;
    return true;
}

bool SC::HttpFormUrlEncodedIterator::getValue(StringSpan body, StringSpan name, StringSpan& value)
{
    value = {};

    HttpFormUrlEncodedIterator it(body);
    HttpURLQueryItem           item;
    while (it.next(item))
    {
        if (item.name == name)
        {
            value = item.value;
            return true;
        }
    }
    return false;
}

static int scHttpHexValue(char value)
{
    if (value >= '0' and value <= '9')
    {
        return value - '0';
    }
    if (value >= 'A' and value <= 'F')
    {
        return value - 'A' + 10;
    }
    if (value >= 'a' and value <= 'f')
    {
        return value - 'a' + 10;
    }
    return -1;
}

static SC::Result scHttpDecodeComponent(SC::StringSpan input, SC::Span<char> storage, SC::StringSpan& output,
                                        bool plusAsSpace)
{
    output             = {};
    const char*  data  = input.bytesWithoutTerminator();
    const size_t size  = input.sizeInBytes();
    size_t       write = 0;
    for (size_t read = 0; read < size; ++read)
    {
        char decoded = data[read];
        if (decoded == '%')
        {
            if (size - read < 3)
                return SC::Result::Error(SC::HttpResultCategory, SC::HttpError::MalformedPercentEscape);
            const int high = scHttpHexValue(data[read + 1]);
            const int low  = scHttpHexValue(data[read + 2]);
            if (high < 0 or low < 0)
                return SC::Result::Error(SC::HttpResultCategory, SC::HttpError::MalformedPercentEscape);
            decoded = static_cast<char>((high << 4) | low);
            read += 2;
        }
        else if (plusAsSpace and decoded == '+')
        {
            decoded = ' ';
        }
        if (write >= storage.sizeInBytes())
            return SC::Result::Error(SC::HttpResultCategory, SC::HttpError::DecodedOutputTooSmall);
        storage.data()[write++] = decoded;
    }
    output = {{storage.data(), write}, false, input.getEncoding()};
    return SC::Result(true);
}

static bool scHttpUrlContainsInvalidWhitespace(SC::StringSpan value)
{
    const char* data = value.bytesWithoutTerminator();
    for (size_t idx = 0; idx < value.sizeInBytes(); ++idx)
    {
        if (static_cast<unsigned char>(data[idx]) <= static_cast<unsigned char>(' '))
        {
            return true;
        }
    }
    return false;
}

SC::Result SC::HttpPercentDecode(StringSpan input, Span<char> storage, StringSpan& output)
{
    return scHttpDecodeComponent(input, storage, output, false);
}

SC::Result SC::HttpFormUrlDecode(StringSpan input, Span<char> storage, StringSpan& output)
{
    return scHttpDecodeComponent(input, storage, output, true);
}

SC::Result SC::HttpRequestTargetView::parse(StringSpan requestTarget)
{
    *this = {};
    if (requestTarget.isEmpty())
        return Result::Error(HttpResultCategory, HttpError::RequestTargetEmpty);
    if (scHttpUrlContainsInvalidWhitespace(requestTarget))
        return Result::Error(HttpResultCategory, HttpError::RequestTargetWhitespace);

    raw = requestTarget;

    const char*  data   = requestTarget.bytesWithoutTerminator();
    const size_t length = requestTarget.sizeInBytes();
    if (length == 1 and data[0] == '*')
    {
        path = requestTarget;
        return Result(true);
    }

    if (data[0] != '/')
        return Result::Error(HttpResultCategory, HttpError::UnsupportedRequestTargetForm);

    size_t queryStart = length;
    size_t hashStart  = length;
    for (size_t idx = 0; idx < length; ++idx)
    {
        if (data[idx] == '?' and queryStart == length and hashStart == length)
        {
            queryStart = idx;
        }
        else if (data[idx] == '#' and hashStart == length)
        {
            hashStart = idx;
        }
    }

    const size_t pathEnd   = queryStart < hashStart ? queryStart : hashStart;
    const size_t searchEnd = hashStart;
    path                   = {{data, pathEnd}, false, requestTarget.getEncoding()};
    if (queryStart < length)
    {
        search = {{data + queryStart, searchEnd - queryStart}, false, requestTarget.getEncoding()};
    }
    if (hashStart < length)
    {
        hash = {{data + hashStart, length - hashStart}, false, requestTarget.getEncoding()};
    }
    return Result(true);
}

bool SC::HttpRequestTargetView::getQueryValue(StringSpan name, StringSpan& value) const
{
    return HttpURLParser::getQueryValue(search, name, value);
}

SC::Result SC::HttpURLParser::parse(StringSpan url)
{
    protocol = {};
    username = {};
    password = {};
    hostname = {};
    host     = {};
    pathname = {};
    path     = {};
    search   = {};
    hash     = {};
    port     = 0;
    encoding = url.getEncoding();

    HttpStringIterator it    = url;
    HttpStringIterator start = it;

    // Protocol
    if (not it.advanceUntilMatches(':'))
        return Result::Error(HttpResultCategory, HttpError::MalformedURL);
    protocol = HttpStringIterator::fromIterators(start, it, encoding);
    SC_TRY(validateProtocol());
    if (not it.advanceIfMatches(':') or not it.advanceIfMatches('/') or not it.advanceIfMatches('/'))
        return Result::Error(HttpResultCategory, HttpError::MalformedURL);
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
                path     = "/";
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
                path     = "/";
                hash     = HttpStringIterator::fromIteratorUntilEnd(hostIt, encoding);
            }
        }
        else
        {
            path     = "/";
            pathname = "/";
        }
        SC_TRY(validatePath());
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
        SC_TRY(validatePath());
    }
    return Result(true);
}

bool SC::HttpURLParser::getQueryValue(StringSpan name, StringSpan& value) const
{
    return getQueryValue(search, name, value);
}

bool SC::HttpURLParser::getQueryValue(StringSpan query, StringSpan name, StringSpan& value)
{
    value = {};

    HttpURLQueryIterator it(query);
    HttpURLQueryItem     item;
    while (it.next(item))
    {
        if (item.name == name)
        {
            value = item.value;
            return true;
        }
    }
    return false;
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
            return Result::Error(HttpResultCategory, HttpError::InvalidIPv6Host);
        (void)it2.stepForward(); // include ]
        hostname = HttpStringIterator::fromIterators(start2, it2, encoding);
        // check if next char is ':' (port separator) without consuming it twice
        if (it2.match(':'))
        {
            (void)it2.stepForward();
            StringSpan portString = HttpStringIterator::fromIteratorUntilEnd(it2, encoding);
            if (portString.isEmpty())
                return Result::Error(HttpResultCategory, HttpError::InvalidURLPort);
            int32_t value;
            if (not HttpStringIterator::parseInt32(portString, value))
                return Result::Error(HttpResultCategory, HttpError::InvalidURLPort);
            if (value < 0 || value > 65535)
                return Result::Error(HttpResultCategory, HttpError::InvalidURLPort);
            port = static_cast<uint16_t>(value);
            // Update host to include hostname and port
            host = HttpStringIterator::fromIterators(start, it2, encoding);
        }
        else if (not it2.isAtEnd())
        {
            return Result::Error(HttpResultCategory, HttpError::InvalidIPv6Host);
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
            if (portString.isEmpty())
                return Result::Error(HttpResultCategory, HttpError::InvalidURLPort);
            int32_t value;
            if (not HttpStringIterator::parseInt32(portString, value))
                return Result::Error(HttpResultCategory, HttpError::InvalidURLPort);
            if (value < 0 || value > 65535)
                return Result::Error(HttpResultCategory, HttpError::InvalidURLPort);
            port = static_cast<uint16_t>(value);
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

    return Result::Error(HttpResultCategory, HttpError::UnsupportedProtocol);
}

SC::Result SC::HttpURLParser::validatePath()
{
    // TODO: Improve validatePath
    if (scHttpUrlContainsInvalidWhitespace(pathname) or scHttpUrlContainsInvalidWhitespace(search) or
        scHttpUrlContainsInvalidWhitespace(hash))
        return Result::Error(HttpResultCategory, HttpError::InvalidURLPath);
    return Result(true);
}

SC::Result SC::HttpURLParser::validateHost()
{
    // TODO: Improve validateHost
    if (host.isEmpty() or scHttpUrlContainsInvalidWhitespace(host) or
        not((HttpStringIterator::startsWith(hostname, "[") and HttpStringIterator::endsWith(hostname, "]")) or
            HttpStringIterator::containsCodePoint(host, '.') or hostname == "localhost"))
        return Result::Error(HttpResultCategory, HttpError::InvalidURLHost);
    return Result(true);
}

SC::Result SC::HttpURLParser::parseUserPassword(StringSpan userPassword)
{
    if (scHttpUrlContainsInvalidWhitespace(userPassword))
        return Result::Error(HttpResultCategory, HttpError::InvalidURLUserInfo);

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
