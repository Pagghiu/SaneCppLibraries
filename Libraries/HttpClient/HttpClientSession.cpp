// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpClientSession.h"

#include <string.h>

namespace
{
struct ParsedUrl
{
    SC::StringSpan origin;
    SC::StringSpan host;
    SC::StringSpan path;
    bool           isHttps = false;
};

static char sessionAsciiLower(char c)
{
    if (c >= 'A' and c <= 'Z')
    {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

static bool sessionAsciiEqualsIgnoreCase(SC::StringSpan left, SC::StringSpan right)
{
    if (left.sizeInBytes() != right.sizeInBytes())
    {
        return false;
    }
    const SC::Span<const char> leftBytes  = left.toCharSpan();
    const SC::Span<const char> rightBytes = right.toCharSpan();
    for (size_t idx = 0; idx < leftBytes.sizeInBytes(); ++idx)
    {
        if (sessionAsciiLower(leftBytes[idx]) != sessionAsciiLower(rightBytes[idx]))
        {
            return false;
        }
    }
    return true;
}

static bool isHttpTokenCharacter(char c)
{
    return (c >= 'A' and c <= 'Z') or (c >= 'a' and c <= 'z') or (c >= '0' and c <= '9') or c == '!' or c == '#' or
           c == '$' or c == '%' or c == '&' or c == '\'' or c == '*' or c == '+' or c == '-' or c == '.' or c == '^' or
           c == '_' or c == '`' or c == '|' or c == '~';
}

static size_t skipAuthSpaces(SC::Span<const char> bytes, size_t offset, size_t end)
{
    while (offset < end and (bytes[offset] == ' ' or bytes[offset] == '\t'))
    {
        offset += 1;
    }
    return offset;
}

static size_t readAuthToken(SC::Span<const char> bytes, size_t offset, size_t end)
{
    while (offset < end and isHttpTokenCharacter(bytes[offset]))
    {
        offset += 1;
    }
    return offset;
}

static bool asciiStartsWith(SC::StringSpan text, SC::StringSpan prefix)
{
    if (text.sizeInBytes() < prefix.sizeInBytes())
    {
        return false;
    }
    const SC::Span<const char> textBytes   = text.toCharSpan();
    const SC::Span<const char> prefixBytes = prefix.toCharSpan();
    return ::memcmp(textBytes.data(), prefixBytes.data(), prefixBytes.sizeInBytes()) == 0;
}

static bool sessionIsAsciiWhiteSpace(char c) { return c == ' ' or c == '\t' or c == '\r' or c == '\n'; }

static SC::StringSpan trimAsciiWhiteSpace(SC::StringSpan text)
{
    const SC::Span<const char> bytes = text.toCharSpan();
    size_t                     start = 0;
    size_t                     end   = bytes.sizeInBytes();
    while (start < end and sessionIsAsciiWhiteSpace(bytes[start]))
    {
        start += 1;
    }
    while (end > start and sessionIsAsciiWhiteSpace(bytes[end - 1]))
    {
        end -= 1;
    }
    return {{bytes.data() + start, end - start}, false, SC::StringEncoding::Ascii};
}

static SC::StringSpan sliceString(SC::StringSpan text, size_t start, size_t end)
{
    const SC::Span<const char> bytes = text.toCharSpan();
    if (start > end or end > bytes.sizeInBytes())
    {
        return SC::StringSpan(text.getEncoding());
    }
    return {{bytes.data() + start, end - start}, false, text.getEncoding()};
}

static size_t findAuthChallengeEnd(SC::StringSpan value, size_t start)
{
    const SC::Span<const char> bytes   = value.toCharSpan();
    const size_t               length  = bytes.sizeInBytes();
    bool                       quoted  = false;
    bool                       escaped = false;

    for (size_t idx = start; idx < length; ++idx)
    {
        if (quoted)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (bytes[idx] == '\\')
            {
                escaped = true;
            }
            else if (bytes[idx] == '"')
            {
                quoted = false;
            }
            continue;
        }

        if (bytes[idx] == '"')
        {
            quoted = true;
            continue;
        }

        if (bytes[idx] != ',')
        {
            continue;
        }

        const size_t tokenStart = skipAuthSpaces(bytes, idx + 1, length);
        const size_t tokenEnd   = readAuthToken(bytes, tokenStart, length);
        if (tokenStart == tokenEnd)
        {
            return idx;
        }

        const size_t afterToken = skipAuthSpaces(bytes, tokenEnd, length);
        if (afterToken >= length or bytes[afterToken] != '=')
        {
            return idx;
        }
    }

    return length;
}

static bool parseAuthRealm(SC::StringSpan value, size_t start, size_t end, SC::StringSpan& realm)
{
    const SC::Span<const char> bytes = value.toCharSpan();
    size_t                     idx   = start;
    while (idx < end)
    {
        idx = skipAuthSpaces(bytes, idx, end);
        if (idx < end and bytes[idx] == ',')
        {
            idx += 1;
            continue;
        }

        const size_t nameStart = idx;
        const size_t nameEnd   = readAuthToken(bytes, nameStart, end);
        if (nameStart == nameEnd)
        {
            idx += 1;
            continue;
        }

        idx = skipAuthSpaces(bytes, nameEnd, end);
        if (idx >= end or bytes[idx] != '=')
        {
            idx = nameEnd + 1;
            continue;
        }

        idx = skipAuthSpaces(bytes, idx + 1, end);
        if (idx >= end)
        {
            break;
        }

        const bool isRealm =
            sessionAsciiEqualsIgnoreCase(sliceString(value, nameStart, nameEnd), SC::StringSpan("realm"));
        if (bytes[idx] == '"')
        {
            const size_t valueStart = idx + 1;
            bool         escaped    = false;
            idx += 1;
            while (idx < end)
            {
                if (escaped)
                {
                    escaped = false;
                }
                else if (bytes[idx] == '\\')
                {
                    escaped = true;
                }
                else if (bytes[idx] == '"')
                {
                    break;
                }
                idx += 1;
            }

            if (isRealm)
            {
                realm = sliceString(value, valueStart, idx);
                return true;
            }
            if (idx < end)
            {
                idx += 1;
            }
        }
        else
        {
            const size_t valueStart = idx;
            while (idx < end and bytes[idx] != ',' and bytes[idx] != ' ' and bytes[idx] != '\t')
            {
                idx += 1;
            }
            if (isRealm)
            {
                realm = sliceString(value, valueStart, idx);
                return true;
            }
        }
    }

    return false;
}

static bool findBasicChallengeInValue(SC::StringSpan value, SC::HttpClientSessionAuthChallenge::Target target,
                                      SC::HttpClientSessionAuthChallenge& challenge)
{
    const SC::Span<const char> bytes  = value.toCharSpan();
    const size_t               length = bytes.sizeInBytes();
    size_t                     idx    = 0;

    while (idx < length)
    {
        idx = skipAuthSpaces(bytes, idx, length);
        if (idx < length and bytes[idx] == ',')
        {
            idx += 1;
            continue;
        }

        const size_t schemeStart = idx;
        const size_t schemeEnd   = readAuthToken(bytes, schemeStart, length);
        if (schemeStart == schemeEnd)
        {
            break;
        }

        const size_t challengeEnd = findAuthChallengeEnd(value, schemeEnd);
        if (sessionAsciiEqualsIgnoreCase(sliceString(value, schemeStart, schemeEnd), SC::StringSpan("Basic")))
        {
            challenge.target = target;
            challenge.scheme = SC::HttpClientSessionAuthChallenge::Basic;
            challenge.realm  = SC::StringSpan(value.getEncoding());
            (void)parseAuthRealm(value, schemeEnd, challengeEnd, challenge.realm);
            return true;
        }

        idx = challengeEnd < length ? challengeEnd + 1 : length;
    }

    return false;
}

static bool splitOnce(SC::StringSpan text, char separator, SC::StringSpan& left, SC::StringSpan& right)
{
    const SC::Span<const char> bytes = text.toCharSpan();
    for (size_t idx = 0; idx < bytes.sizeInBytes(); ++idx)
    {
        if (bytes[idx] == separator)
        {
            left  = sliceString(text, 0, idx);
            right = sliceString(text, idx + 1, bytes.sizeInBytes());
            return true;
        }
    }
    return false;
}

static bool sessionRequestHasHeader(const SC::HttpClientRequest& request, SC::StringSpan name)
{
    for (size_t idx = 0; idx < request.headers.sizeInElements(); ++idx)
    {
        if (sessionAsciiEqualsIgnoreCase(request.headers[idx].name, name))
        {
            return true;
        }
    }
    return false;
}

static bool parseUrl(SC::StringSpan url, ParsedUrl& parsed)
{
    const SC::Span<const char> bytes = url.toCharSpan();

    size_t schemeEnd = 0;
    while (schemeEnd + 2 < bytes.sizeInBytes())
    {
        if (bytes[schemeEnd] == ':' and bytes[schemeEnd + 1] == '/' and bytes[schemeEnd + 2] == '/')
        {
            break;
        }
        schemeEnd += 1;
    }
    if (schemeEnd + 2 >= bytes.sizeInBytes())
    {
        return false;
    }

    const SC::StringSpan scheme = sliceString(url, 0, schemeEnd);
    parsed.isHttps              = sessionAsciiEqualsIgnoreCase(scheme, SC::StringSpan("https"));

    const size_t hostStart = schemeEnd + 3;
    size_t       hostEnd   = hostStart;
    while (hostEnd < bytes.sizeInBytes() and bytes[hostEnd] != '/' and bytes[hostEnd] != '?' and bytes[hostEnd] != '#')
    {
        hostEnd += 1;
    }
    if (hostEnd == hostStart)
    {
        return false;
    }

    parsed.origin = sliceString(url, 0, hostEnd);
    parsed.host   = sliceString(url, hostStart, hostEnd);

    if (hostEnd < bytes.sizeInBytes() and bytes[hostEnd] == '/')
    {
        size_t pathEnd = hostEnd;
        while (pathEnd < bytes.sizeInBytes() and bytes[pathEnd] != '?' and bytes[pathEnd] != '#')
        {
            pathEnd += 1;
        }
        parsed.path = sliceString(url, hostEnd, pathEnd);
    }
    else
    {
        parsed.path = SC::StringSpan("/");
    }
    return true;
}

static bool cookieDomainMatches(const SC::HttpClientSessionCookie& cookie, SC::StringSpan host)
{
    if ((cookie.flags & SC::HttpClientSessionCookie::DomainCookie) == 0)
    {
        return sessionAsciiEqualsIgnoreCase(cookie.domain, host);
    }

    if (sessionAsciiEqualsIgnoreCase(cookie.domain, host))
    {
        return true;
    }

    if (host.sizeInBytes() <= cookie.domain.sizeInBytes() + 1)
    {
        return false;
    }
    const SC::Span<const char> hostBytes = host.toCharSpan();
    const size_t               offset    = host.sizeInBytes() - cookie.domain.sizeInBytes();
    if (hostBytes[offset - 1] != '.')
    {
        return false;
    }
    return sessionAsciiEqualsIgnoreCase(
        {{hostBytes.data() + offset, cookie.domain.sizeInBytes()}, false, SC::StringEncoding::Ascii}, cookie.domain);
}

static bool cookiePathMatches(const SC::HttpClientSessionCookie& cookie, SC::StringSpan path)
{
    if (cookie.path.isEmpty())
    {
        return true;
    }
    return asciiStartsWith(path, cookie.path);
}

static bool sessionIsIdempotentMethod(SC::HttpClientRequest::Method method)
{
    switch (method)
    {
    case SC::HttpClientRequest::HttpGET:
    case SC::HttpClientRequest::HttpHEAD:
    case SC::HttpClientRequest::HttpPUT:
    case SC::HttpClientRequest::HttpDELETE:
    case SC::HttpClientRequest::HttpOPTIONS: return true;
    case SC::HttpClientRequest::HttpPOST:
    case SC::HttpClientRequest::HttpPATCH: return false;
    }
    return false;
}

static bool sessionIsRetryableStatusCode(int statusCode)
{
    return statusCode == 408 or statusCode == 425 or statusCode == 429 or statusCode == 500 or statusCode == 502 or
           statusCode == 503 or statusCode == 504;
}

static bool canReplayRequestBody(const SC::HttpClientRequestBody& body)
{
    return not body.isStreamed() or body.canReplay;
}

static char basicAuthorizationSourceByte(SC::Span<const char> username, SC::Span<const char> password, size_t index)
{
    if (index < username.sizeInBytes())
    {
        return username[index];
    }
    if (index == username.sizeInBytes())
    {
        return ':';
    }
    return password[index - username.sizeInBytes() - 1];
}
} // namespace

SC::Result SC::HttpClientSession::init(const HttpClientSessionMemory& memory)
{
    SC_TRY_MSG(not initialized, "HttpClientSession: already initialized");
    SC_TRY_MSG(not memory.requestHeaders.empty(), "HttpClientSession: request header workspace missing");
    sessionMemory = memory;
    clear();
    initialized = true;
    return Result(true);
}

void SC::HttpClientSession::clear()
{
    clearCookies();
    clearAuthorizations();
    stateScratchUsed  = 0;
    headerScratchUsed = 0;
}

void SC::HttpClientSession::clearCookies()
{
    for (size_t idx = 0; idx < sessionMemory.cookies.sizeInElements(); ++idx)
    {
        sessionMemory.cookies[idx] = HttpClientSessionCookie();
    }
}

void SC::HttpClientSession::clearAuthorizations()
{
    for (size_t idx = 0; idx < sessionMemory.authEntries.sizeInElements(); ++idx)
    {
        sessionMemory.authEntries[idx] = HttpClientSessionAuthCacheEntry();
    }
}

const char* SC::HttpClientSessionAuthChallenge::getTargetName(Target target)
{
    switch (target)
    {
    case Origin: return "origin";
    case Proxy: return "proxy";
    }
    return "unknown";
}

const char* SC::HttpClientSessionAuthChallenge::getSchemeName(Scheme scheme)
{
    switch (scheme)
    {
    case Unsupported: return "unsupported";
    case Basic: return "basic";
    }
    return "unknown";
}

SC::Result SC::HttpClientSession::copyStateString(StringSpan source, StringSpan& destination)
{
    SC_TRY_MSG(initialized, "HttpClientSession: not initialized");
    SC_TRY_MSG(stateScratchUsed <= sessionMemory.stateScratch.sizeInBytes(), "HttpClientSession: invalid state");

    if (source.isEmpty())
    {
        destination = StringSpan(source.getEncoding());
        return Result(true);
    }

    SC_TRY_MSG(sessionMemory.stateScratch.sizeInBytes() - stateScratchUsed >= source.sizeInBytes(),
               "HttpClientSession: state scratch exhausted");
    char* const destinationBytes = sessionMemory.stateScratch.data() + stateScratchUsed;
    ::memcpy(destinationBytes, source.bytesWithoutTerminator(), source.sizeInBytes());
    destination = StringSpan({destinationBytes, source.sizeInBytes()}, false, source.getEncoding());
    stateScratchUsed += source.sizeInBytes();
    return Result(true);
}

SC::Result SC::HttpClientSession::appendPreparedHeader(StringSpan name, StringSpan value, size_t& numHeaders)
{
    SC_TRY_MSG(numHeaders < sessionMemory.requestHeaders.sizeInElements(),
               "HttpClientSession: request header capacity exhausted");
    sessionMemory.requestHeaders[numHeaders] = {name, value};
    numHeaders += 1;
    return Result(true);
}

SC::Result SC::HttpClientSession::appendScratch(StringSpan text, bool addSeparator)
{
    const size_t separatorBytes = addSeparator ? 2 : 0;
    SC_TRY_MSG(sessionMemory.headerScratch.sizeInBytes() - headerScratchUsed >= separatorBytes + text.sizeInBytes(),
               "HttpClientSession: header scratch exhausted");
    if (addSeparator)
    {
        sessionMemory.headerScratch[headerScratchUsed++] = ';';
        sessionMemory.headerScratch[headerScratchUsed++] = ' ';
    }
    if (text.sizeInBytes() > 0)
    {
        ::memcpy(sessionMemory.headerScratch.data() + headerScratchUsed, text.bytesWithoutTerminator(),
                 text.sizeInBytes());
        headerScratchUsed += text.sizeInBytes();
    }
    return Result(true);
}

SC::Result SC::HttpClientSession::addAuthorization(StringSpan origin, StringSpan authorizationHeader)
{
    SC_TRY_MSG(initialized, "HttpClientSession: not initialized");
    SC_TRY_MSG(not origin.isEmpty(), "HttpClientSession: auth origin missing");
    SC_TRY_MSG(not authorizationHeader.isEmpty(), "HttpClientSession: authorization header missing");

    HttpClientSessionAuthCacheEntry* target = nullptr;
    for (size_t idx = 0; idx < sessionMemory.authEntries.sizeInElements(); ++idx)
    {
        HttpClientSessionAuthCacheEntry& entry = sessionMemory.authEntries[idx];
        if (entry.isInUse() and sessionAsciiEqualsIgnoreCase(entry.origin, origin))
        {
            target = &entry;
            break;
        }
        if (target == nullptr and not entry.isInUse())
        {
            target = &entry;
        }
    }

    SC_TRY_MSG(target != nullptr, "HttpClientSession: auth cache capacity exhausted");
    SC_TRY(copyStateString(origin, target->origin));
    SC_TRY(copyStateString(authorizationHeader, target->authorizationHeader));
    return Result(true);
}

bool SC::HttpClientSession::findAuthorization(StringSpan origin, StringSpan& authorizationHeader) const
{
    authorizationHeader = {};
    if (not initialized)
    {
        return false;
    }

    for (size_t idx = 0; idx < sessionMemory.authEntries.sizeInElements(); ++idx)
    {
        const HttpClientSessionAuthCacheEntry& entry = sessionMemory.authEntries[idx];
        if (entry.isInUse() and sessionAsciiEqualsIgnoreCase(entry.origin, origin))
        {
            authorizationHeader = entry.authorizationHeader;
            return true;
        }
    }
    return false;
}

bool SC::HttpClientSession::hasAuthorization(StringSpan origin) const
{
    StringSpan authorizationHeader;
    return findAuthorization(origin, authorizationHeader);
}

bool SC::HttpClientSession::findCookie(StringSpan name, StringSpan domain, StringSpan path,
                                       HttpClientSessionCookie& cookie) const
{
    cookie = HttpClientSessionCookie();
    if (not initialized)
    {
        return false;
    }

    for (size_t idx = 0; idx < sessionMemory.cookies.sizeInElements(); ++idx)
    {
        const HttpClientSessionCookie& candidate = sessionMemory.cookies[idx];
        if (candidate.isInUse() and sessionAsciiEqualsIgnoreCase(candidate.name, name) and
            sessionAsciiEqualsIgnoreCase(candidate.domain, domain) and candidate.path == path)
        {
            cookie = candidate;
            return true;
        }
    }
    return false;
}

bool SC::HttpClientSession::hasCookie(StringSpan name, StringSpan domain, StringSpan path) const
{
    HttpClientSessionCookie cookie;
    return findCookie(name, domain, path, cookie);
}

SC::Result SC::HttpClientSession::makeBasicAuthorization(StringSpan username, StringSpan password,
                                                         Span<char> destination, StringSpan& authorizationHeader)
{
    static const char   Prefix[]    = "Basic ";
    static const size_t PrefixBytes = sizeof(Prefix) - 1;
    static const char   Base64[]    = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    const Span<const char> usernameBytes = username.toCharSpan();
    const Span<const char> passwordBytes = password.toCharSpan();
    const size_t           sourceBytes   = usernameBytes.sizeInBytes() + 1 + passwordBytes.sizeInBytes();
    const size_t           encodedBytes  = ((sourceBytes + 2) / 3) * 4;

    SC_TRY_MSG(not username.isEmpty(), "HttpClientSession: basic auth username missing");
    SC_TRY_MSG(destination.sizeInBytes() >= PrefixBytes + encodedBytes,
               "HttpClientSession: basic auth destination too small");

    memcpy(destination.data(), Prefix, PrefixBytes);
    char* output = destination.data() + PrefixBytes;

    for (size_t inputOffset = 0, outputOffset = 0; inputOffset < sourceBytes; inputOffset += 3, outputOffset += 4)
    {
        const size_t        available = sourceBytes - inputOffset;
        const unsigned char b0 =
            static_cast<unsigned char>(basicAuthorizationSourceByte(usernameBytes, passwordBytes, inputOffset));
        const unsigned char b1 = available > 1 ? static_cast<unsigned char>(basicAuthorizationSourceByte(
                                                     usernameBytes, passwordBytes, inputOffset + 1))
                                               : 0;
        const unsigned char b2 = available > 2 ? static_cast<unsigned char>(basicAuthorizationSourceByte(
                                                     usernameBytes, passwordBytes, inputOffset + 2))
                                               : 0;

        output[outputOffset + 0] = Base64[(b0 >> 2) & 0x3F];
        output[outputOffset + 1] = Base64[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)];
        output[outputOffset + 2] = available > 1 ? Base64[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)] : '=';
        output[outputOffset + 3] = available > 2 ? Base64[b2 & 0x3F] : '=';
    }

    authorizationHeader = {{destination.data(), PrefixBytes + encodedBytes}, false, StringEncoding::Ascii};
    return Result(true);
}

bool SC::HttpClientSession::findBasicAuthChallenge(const HttpClientResponse&              response,
                                                   HttpClientSessionAuthChallenge::Target target,
                                                   HttpClientSessionAuthChallenge&        challenge)
{
    challenge        = HttpClientSessionAuthChallenge();
    challenge.target = target;

    const StringSpan headerName = target == HttpClientSessionAuthChallenge::Proxy ? StringSpan("Proxy-Authenticate")
                                                                                  : StringSpan("WWW-Authenticate");

    HttpClientResponseHeaderIterator iterator;
    StringSpan                       value;
    while (response.findNextHeader(headerName, iterator, value))
    {
        if (findBasicChallengeInValue(value, target, challenge))
        {
            return true;
        }
    }
    return false;
}

SC::Result SC::HttpClientSession::makeBasicAuthorizationForChallenge(const HttpClientResponse&              response,
                                                                     HttpClientSessionAuthChallenge::Target target,
                                                                     StringSpan username, StringSpan password,
                                                                     Span<char>  destination,
                                                                     StringSpan& authorizationHeader)
{
    const int expectedStatusCode = target == HttpClientSessionAuthChallenge::Proxy ? 407 : 401;
    SC_TRY_MSG(response.statusCode == expectedStatusCode, "HttpClientSession: response is not an auth challenge");

    HttpClientSessionAuthChallenge challenge;
    SC_TRY_MSG(findBasicAuthChallenge(response, target, challenge), "HttpClientSession: basic auth challenge missing");
    return makeBasicAuthorization(username, password, destination, authorizationHeader);
}

SC::Result SC::HttpClientSession::appendMatchingCookies(StringSpan url, size_t& numHeaders)
{
    ParsedUrl parsed;
    SC_TRY_MSG(parseUrl(url, parsed), "HttpClientSession: invalid request URL");

    const size_t cookieHeaderStart = headerScratchUsed;
    bool         wroteCookie       = false;
    for (size_t idx = 0; idx < sessionMemory.cookies.sizeInElements(); ++idx)
    {
        const HttpClientSessionCookie& cookie = sessionMemory.cookies[idx];
        if (not cookie.isInUse())
        {
            continue;
        }
        if ((cookie.flags & HttpClientSessionCookie::Secure) != 0 and not parsed.isHttps)
        {
            continue;
        }
        if (not cookieDomainMatches(cookie, parsed.host) or not cookiePathMatches(cookie, parsed.path))
        {
            continue;
        }

        SC_TRY(appendScratch(cookie.name, wroteCookie));
        SC_TRY(appendScratch(StringSpan("="), false));
        SC_TRY(appendScratch(cookie.value, false));
        wroteCookie = true;
    }

    if (wroteCookie)
    {
        const StringSpan value(
            {sessionMemory.headerScratch.data() + cookieHeaderStart, headerScratchUsed - cookieHeaderStart}, false,
            StringEncoding::Ascii);
        SC_TRY(appendPreparedHeader(StringSpan("Cookie"), value, numHeaders));
    }
    return Result(true);
}

SC::Result SC::HttpClientSession::prepareRequest(const HttpClientRequest& source, HttpClientRequest& prepared)
{
    SC_TRY_MSG(initialized, "HttpClientSession: not initialized");
    SC_TRY_MSG(source.headers.sizeInElements() <= sessionMemory.requestHeaders.sizeInElements(),
               "HttpClientSession: request header capacity exhausted");

    headerScratchUsed = 0;
    size_t numHeaders = 0;
    for (size_t idx = 0; idx < source.headers.sizeInElements(); ++idx)
    {
        sessionMemory.requestHeaders[numHeaders++] = source.headers[idx];
    }

    if (not sessionRequestHasHeader(source, StringSpan("Cookie")))
    {
        SC_TRY(appendMatchingCookies(source.url, numHeaders));
    }

    if (not sessionRequestHasHeader(source, StringSpan("Authorization")))
    {
        ParsedUrl parsed;
        SC_TRY_MSG(parseUrl(source.url, parsed), "HttpClientSession: invalid request URL");
        for (size_t idx = 0; idx < sessionMemory.authEntries.sizeInElements(); ++idx)
        {
            const HttpClientSessionAuthCacheEntry& entry = sessionMemory.authEntries[idx];
            if (entry.isInUse() and sessionAsciiEqualsIgnoreCase(entry.origin, parsed.origin))
            {
                SC_TRY(appendPreparedHeader(StringSpan("Authorization"), entry.authorizationHeader, numHeaders));
                break;
            }
        }
    }

    prepared         = source;
    prepared.headers = {sessionMemory.requestHeaders.data(), numHeaders};
    return Result(true);
}

SC::Result SC::HttpClientSession::captureSetCookie(StringSpan requestUrl, StringSpan setCookie)
{
    ParsedUrl parsed;
    SC_TRY_MSG(parseUrl(requestUrl, parsed), "HttpClientSession: invalid request URL");

    StringSpan firstAttribute;
    StringSpan remaining;
    if (not splitOnce(setCookie, ';', firstAttribute, remaining))
    {
        firstAttribute = setCookie;
        remaining      = StringSpan(setCookie.getEncoding());
    }

    StringSpan name;
    StringSpan value;
    if (not splitOnce(firstAttribute, '=', name, value))
    {
        return Result(true);
    }

    name  = trimAsciiWhiteSpace(name);
    value = trimAsciiWhiteSpace(value);
    if (name.isEmpty())
    {
        return Result(true);
    }

    StringSpan domain         = parsed.host;
    StringSpan path           = "/";
    uint8_t    flags          = 0;
    bool       explicitDomain = false;

    while (not remaining.isEmpty())
    {
        StringSpan attribute;
        StringSpan next;
        if (splitOnce(remaining, ';', attribute, next))
        {
            remaining = next;
        }
        else
        {
            attribute = remaining;
            remaining = StringSpan(remaining.getEncoding());
        }

        attribute = trimAsciiWhiteSpace(attribute);
        StringSpan attributeName;
        StringSpan attributeValue;
        if (splitOnce(attribute, '=', attributeName, attributeValue))
        {
            attributeName  = trimAsciiWhiteSpace(attributeName);
            attributeValue = trimAsciiWhiteSpace(attributeValue);
            if (sessionAsciiEqualsIgnoreCase(attributeName, StringSpan("Domain")))
            {
                if (attributeValue.sizeInBytes() > 0 and attributeValue.toCharSpan()[0] == '.')
                {
                    attributeValue = sliceString(attributeValue, 1, attributeValue.sizeInBytes());
                }
                domain         = attributeValue;
                explicitDomain = true;
            }
            else if (sessionAsciiEqualsIgnoreCase(attributeName, StringSpan("Path")))
            {
                path = attributeValue;
            }
        }
        else if (sessionAsciiEqualsIgnoreCase(attribute, StringSpan("Secure")))
        {
            flags = static_cast<uint8_t>(flags | HttpClientSessionCookie::Secure);
        }
        else if (sessionAsciiEqualsIgnoreCase(attribute, StringSpan("HttpOnly")))
        {
            flags = static_cast<uint8_t>(flags | HttpClientSessionCookie::HttpOnly);
        }
    }

    if (explicitDomain)
    {
        flags = static_cast<uint8_t>(flags | HttpClientSessionCookie::DomainCookie);
    }

    HttpClientSessionCookie* target = nullptr;
    for (size_t idx = 0; idx < sessionMemory.cookies.sizeInElements(); ++idx)
    {
        HttpClientSessionCookie& cookie = sessionMemory.cookies[idx];
        if (cookie.isInUse() and sessionAsciiEqualsIgnoreCase(cookie.name, name) and
            sessionAsciiEqualsIgnoreCase(cookie.domain, domain) and cookie.path == path)
        {
            target = &cookie;
            break;
        }
        if (target == nullptr and not cookie.isInUse())
        {
            target = &cookie;
        }
    }

    SC_TRY_MSG(target != nullptr, "HttpClientSession: cookie capacity exhausted");
    SC_TRY(copyStateString(name, target->name));
    SC_TRY(copyStateString(value, target->value));
    SC_TRY(copyStateString(domain, target->domain));
    SC_TRY(copyStateString(path, target->path));
    target->flags = flags;
    return Result(true);
}

SC::Result SC::HttpClientSession::captureResponse(const HttpClientRequest& request, const HttpClientResponse& response)
{
    SC_TRY_MSG(initialized, "HttpClientSession: not initialized");

    HttpClientResponseHeaderIterator iterator;
    StringSpan                       setCookie;
    while (response.findNextHeader(StringSpan("Set-Cookie"), iterator, setCookie))
    {
        SC_TRY(captureSetCookie(request.url, setCookie));
    }
    return Result(true);
}

SC::Result SC::HttpClientSession::beginRetry(HttpClientSessionRetryState& state, const HttpClientRequest& request,
                                             HttpClientSessionRetryPolicy policy) const
{
    SC_TRY_MSG(initialized, "HttpClientSession: not initialized");
    SC_TRY_MSG(policy.maxAttempts > 0, "HttpClientSession: retry policy has no attempts");

    state.method                = request.method;
    state.policy                = policy;
    state.attemptsStarted       = 1;
    state.requestBodyReplayable = canReplayRequestBody(request.body);
    return Result(true);
}

bool SC::HttpClientSession::shouldRetry(HttpClientSessionRetryState& state, Result transportResult,
                                        const HttpClientResponse* response) const
{
    if (not initialized or state.attemptsStarted == 0 or state.attemptsStarted >= state.policy.maxAttempts)
    {
        return false;
    }

    const bool methodCanRetry = sessionIsIdempotentMethod(state.method) or
                                (state.policy.retryNonIdempotentReplayableBody and state.requestBodyReplayable);
    if (not methodCanRetry)
    {
        return false;
    }

    const bool transportShouldRetry = not transportResult and state.policy.retryTransportErrors;
    const bool responseShouldRetry  = response != nullptr and state.policy.retryHttpStatusCodes and
                                     sessionIsRetryableStatusCode(response->statusCode);
    if (not transportShouldRetry and not responseShouldRetry)
    {
        return false;
    }

    state.attemptsStarted += 1;
    return true;
}

bool SC::HttpClientSession::isIdempotentMethod(HttpClientRequest::Method method)
{
    return sessionIsIdempotentMethod(method);
}

bool SC::HttpClientSession::isRetryableStatusCode(int statusCode) { return sessionIsRetryableStatusCode(statusCode); }

SC::size_t SC::HttpClientSession::getNumCookies() const
{
    size_t count = 0;
    if (not initialized)
    {
        return count;
    }

    for (size_t idx = 0; idx < sessionMemory.cookies.sizeInElements(); ++idx)
    {
        if (sessionMemory.cookies[idx].isInUse())
        {
            count += 1;
        }
    }
    return count;
}

SC::size_t SC::HttpClientSession::getNumAuthorizations() const
{
    size_t count = 0;
    if (not initialized)
    {
        return count;
    }

    for (size_t idx = 0; idx < sessionMemory.authEntries.sizeInElements(); ++idx)
    {
        if (sessionMemory.authEntries[idx].isInUse())
        {
            count += 1;
        }
    }
    return count;
}
