// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "HttpURLParser.h"

SC::Result SC::HttpURLParser::parse(StringView url)
{
    StringIteratorASCII it = url.getIterator<StringIteratorASCII>();

    auto start = it;
    // Protocol
    SC_TRY(it.advanceUntilMatches(':'));
    protocol = StringView::fromIterators(start, it);
    SC_TRY(validateProtocol());
    SC_TRY(it.advanceIfMatches(':'));
    SC_TRY(it.advanceIfMatches('/'));
    SC_TRY(it.advanceIfMatches('/'));
    // hostname
    start              = it;
    const bool hasPath = it.advanceUntilMatches('/');
    host               = StringView::fromIterators(start, it);
    SC_TRY(parseHost());
    if (not hasPath)
    {
        return Result(true);
    }
    // path + hash
    start              = it;
    const bool hasHref = it.advanceUntilMatches('#');
    path               = StringView::fromIterators(start, it);
    SC_TRY(parsePath());
    if (hasHref)
    {
        hash = StringView::fromIteratorUntilEnd(it);
    }
    return Result(true);
}

SC::Result SC::HttpURLParser::parsePath()
{
    StringIteratorASCII it = path.getIterator<StringIteratorASCII>();
    if (it.advanceUntilMatches('?'))
    {
        pathname = StringView::fromIteratorFromStart(it);
        search   = StringView::fromIteratorUntilEnd(it);
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
        StringView userAndPassword = StringView::fromIterators(start, it);
        SC_TRY(parseUserPassword(userAndPassword));
        (void)it.stepForward();
    }
    else
    {
        it = start;
    }
    start = it;
    host  = StringView::fromIteratorUntilEnd(it);
    if (it.advanceUntilMatches(':'))
    {
        hostname = StringView::fromIterators(start, it);
        (void)it.stepForward();
        StringView portString = StringView::fromIteratorUntilEnd(it);

        int32_t value;
        SC_TRY(portString.parseInt32(value));
        port = static_cast<uint16_t>(value);
    }
    else
    {
        hostname = host; // default port is filled by validateProtocol
    }

    return validateHost();
}

SC::Result SC::HttpURLParser::validateProtocol()
{
    // TODO: Expand supported protocols
    if (protocol == "http")
    {
        port = 80;
        return Result(true);
    }
    else if (protocol == "https")
    {
        port = 443;
        return Result(true);
    }

    return Result(false);
}

SC::Result SC::HttpURLParser::validatePath()
{
    // TODO: Improve validatePath
    return Result(not pathname.containsChar(' '));
}

SC::Result SC::HttpURLParser::validateHost()
{
    // TODO: Improve validateHost
    return Result(not host.isEmpty() and (host.containsChar('.') or hostname == "localhost"));
}

SC::Result SC::HttpURLParser::parseUserPassword(StringView userPassowrd)
{
    StringViewTokenizer tokenizer(userPassowrd);
    SC_TRY(tokenizer.tokenizeNext({':'}, StringViewTokenizer::Options::SkipEmpty));
    username = tokenizer.component;
    SC_TRY(tokenizer.tokenizeNext({}, StringViewTokenizer::Options::SkipEmpty));
    password = tokenizer.component;
    return Result(true);
}
