// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpHeaders.h"

namespace SC
{
struct HttpHeaderInternal
{
    static StringSpan trimOptionalWhitespace(StringSpan value)
    {
        const char* data  = value.bytesWithoutTerminator();
        size_t      start = 0;
        size_t      end   = value.sizeInBytes();
        while (start < end and (data[start] == ' ' or data[start] == '\t'))
        {
            start++;
        }
        while (end > start and (data[end - 1] == ' ' or data[end - 1] == '\t'))
        {
            end--;
        }
        return {{data + start, end - start}, false, value.getEncoding()};
    }

    static bool equalsIgnoreCase(StringSpan left, StringSpan right)
    {
        if (left.sizeInBytes() != right.sizeInBytes())
        {
            return false;
        }
        const char* leftData  = left.bytesWithoutTerminator();
        const char* rightData = right.bytesWithoutTerminator();
        for (size_t idx = 0; idx < left.sizeInBytes(); ++idx)
        {
            char a = leftData[idx];
            char b = rightData[idx];
            if (a >= 'A' and a <= 'Z')
            {
                a = static_cast<char>(a - 'A' + 'a');
            }
            if (b >= 'A' and b <= 'Z')
            {
                b = static_cast<char>(b - 'A' + 'a');
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    static int base64Value(char c)
    {
        if (c >= 'A' and c <= 'Z')
        {
            return c - 'A';
        }
        if (c >= 'a' and c <= 'z')
        {
            return c - 'a' + 26;
        }
        if (c >= '0' and c <= '9')
        {
            return c - '0' + 52;
        }
        if (c == '+')
        {
            return 62;
        }
        if (c == '/')
        {
            return 63;
        }
        return -1;
    }

    static Result decodeBase64(StringSpan input, Span<char> output, size_t& outputSize)
    {
        outputSize         = 0;
        const char*  data  = input.bytesWithoutTerminator();
        const size_t size  = input.sizeInBytes();
        size_t       index = 0;

        while (index < size)
        {
            int  values[4] = {0, 0, 0, 0};
            bool padded[4] = {false, false, false, false};
            for (size_t idx = 0; idx < 4; ++idx)
            {
                if (index >= size)
                {
                    return Result::Error("Basic authorization has incomplete base64");
                }
                const char current = data[index++];
                if (current == '=')
                {
                    values[idx] = 0;
                    padded[idx] = true;
                }
                else
                {
                    const int value = base64Value(current);
                    if (value < 0)
                    {
                        return Result::Error("Basic authorization has invalid base64");
                    }
                    values[idx] = value;
                }
            }

            if (padded[0] or padded[1] or (padded[2] and not padded[3]))
            {
                return Result::Error("Basic authorization has invalid base64 padding");
            }
            if ((padded[2] or padded[3]) and index != size)
            {
                return Result::Error("Basic authorization has trailing base64 data");
            }

            const uint32_t decoded = (static_cast<uint32_t>(values[0]) << 18) |
                                     (static_cast<uint32_t>(values[1]) << 12) |
                                     (static_cast<uint32_t>(values[2]) << 6) | static_cast<uint32_t>(values[3]);
            const size_t bytesToWrite = padded[2] ? 1 : (padded[3] ? 2 : 3);
            if (outputSize + bytesToWrite > output.sizeInBytes())
            {
                return Result::Error("Basic authorization output buffer is too small");
            }
            output.data()[outputSize++] = static_cast<char>((decoded >> 16) & 0xFF);
            if (bytesToWrite >= 2)
            {
                output.data()[outputSize++] = static_cast<char>((decoded >> 8) & 0xFF);
            }
            if (bytesToWrite == 3)
            {
                output.data()[outputSize++] = static_cast<char>(decoded & 0xFF);
            }
        }

        return Result(true);
    }

    static bool appendTo(Span<char> storage, size_t& offset, StringSpan value)
    {
        if (offset + value.sizeInBytes() > storage.sizeInBytes())
        {
            return false;
        }
        for (size_t idx = 0; idx < value.sizeInBytes(); ++idx)
        {
            storage.data()[offset + idx] = value.bytesWithoutTerminator()[idx];
        }
        offset += value.sizeInBytes();
        return true;
    }

    static bool appendUnsigned(Span<char> storage, size_t& offset, uint32_t value)
    {
        char   reversed[10];
        size_t digits = 0;
        do
        {
            reversed[digits++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        } while (value > 0 and digits < sizeof(reversed));

        if (value > 0 or offset + digits > storage.sizeInBytes())
        {
            return false;
        }
        for (size_t idx = 0; idx < digits; ++idx)
        {
            storage.data()[offset + idx] = reversed[digits - idx - 1];
        }
        offset += digits;
        return true;
    }

    static Result appendCacheDirective(Span<char> storage, size_t& offset, bool& first, StringSpan directive)
    {
        if (not first)
        {
            SC_TRY_MSG(appendTo(storage, offset, ", "), "Cache-Control output buffer is too small");
        }
        SC_TRY_MSG(appendTo(storage, offset, directive), "Cache-Control output buffer is too small");
        first = false;
        return Result(true);
    }
};

HttpCookieIterator::HttpCookieIterator(StringSpan cookieHeader) : header(cookieHeader) {}

bool HttpCookieIterator::next(HttpHeaderKeyValue& pair)
{
    pair = {};

    const char*  data   = header.bytesWithoutTerminator();
    const size_t length = header.sizeInBytes();

    while (cursor < length)
    {
        const size_t itemStart = cursor;
        while (cursor < length and data[cursor] != ';')
        {
            cursor++;
        }
        const size_t itemEnd = cursor;
        if (cursor < length and data[cursor] == ';')
        {
            cursor++;
        }

        StringSpan item = {{data + itemStart, itemEnd - itemStart}, false, header.getEncoding()};
        item            = HttpHeaderInternal::trimOptionalWhitespace(item);
        if (item.isEmpty())
        {
            continue;
        }

        size_t equals = static_cast<size_t>(-1);
        for (size_t idx = 0; idx < item.sizeInBytes(); ++idx)
        {
            if (item.bytesWithoutTerminator()[idx] == '=')
            {
                equals = idx;
                break;
            }
        }

        if (equals == static_cast<size_t>(-1))
        {
            pair.name     = HttpHeaderInternal::trimOptionalWhitespace(item);
            pair.value    = {};
            pair.hasValue = false;
            return true;
        }

        pair.name  = {{item.bytesWithoutTerminator(), equals}, false, item.getEncoding()};
        pair.value = {
            {item.bytesWithoutTerminator() + equals + 1, item.sizeInBytes() - equals - 1}, false, item.getEncoding()};
        pair.name     = HttpHeaderInternal::trimOptionalWhitespace(pair.name);
        pair.value    = HttpHeaderInternal::trimOptionalWhitespace(pair.value);
        pair.hasValue = true;
        return true;
    }

    return false;
}

HttpSetCookieAttributeIterator::HttpSetCookieAttributeIterator(StringSpan attributes) : attributes(attributes) {}

bool HttpSetCookieAttributeIterator::next(HttpHeaderKeyValue& attribute)
{
    attribute = {};

    const char*  data   = attributes.bytesWithoutTerminator();
    const size_t length = attributes.sizeInBytes();
    while (cursor < length)
    {
        while (cursor < length and data[cursor] == ';')
        {
            cursor++;
        }
        const size_t itemStart = cursor;
        while (cursor < length and data[cursor] != ';')
        {
            cursor++;
        }
        const size_t itemEnd = cursor;

        StringSpan item = {{data + itemStart, itemEnd - itemStart}, false, attributes.getEncoding()};
        item            = HttpHeaderInternal::trimOptionalWhitespace(item);
        if (item.isEmpty())
        {
            continue;
        }

        size_t equals = static_cast<size_t>(-1);
        for (size_t idx = 0; idx < item.sizeInBytes(); ++idx)
        {
            if (item.bytesWithoutTerminator()[idx] == '=')
            {
                equals = idx;
                break;
            }
        }
        if (equals == static_cast<size_t>(-1))
        {
            attribute.name     = item;
            attribute.value    = {};
            attribute.hasValue = false;
            return true;
        }
        attribute.name  = {{item.bytesWithoutTerminator(), equals}, false, item.getEncoding()};
        attribute.value = {
            {item.bytesWithoutTerminator() + equals + 1, item.sizeInBytes() - equals - 1}, false, item.getEncoding()};
        attribute.name     = HttpHeaderInternal::trimOptionalWhitespace(attribute.name);
        attribute.value    = HttpHeaderInternal::trimOptionalWhitespace(attribute.value);
        attribute.hasValue = true;
        return true;
    }
    return false;
}

Result HttpSetCookieView::parse(StringSpan setCookieHeader)
{
    *this = {};

    StringSpan header = HttpHeaderInternal::trimOptionalWhitespace(setCookieHeader);
    SC_TRY_MSG(not header.isEmpty(), "Set-Cookie header is empty");

    const char*  data           = header.bytesWithoutTerminator();
    const size_t length         = header.sizeInBytes();
    size_t       firstSemicolon = length;
    size_t       equals         = static_cast<size_t>(-1);
    for (size_t idx = 0; idx < length; ++idx)
    {
        if (data[idx] == ';')
        {
            firstSemicolon = idx;
            break;
        }
        if (data[idx] == '=' and equals == static_cast<size_t>(-1))
        {
            equals = idx;
        }
    }
    SC_TRY_MSG(equals != static_cast<size_t>(-1) and equals < firstSemicolon, "Set-Cookie missing name/value");

    name  = {{data, equals}, false, header.getEncoding()};
    value = {{data + equals + 1, firstSemicolon - equals - 1}, false, header.getEncoding()};
    name  = HttpHeaderInternal::trimOptionalWhitespace(name);
    value = HttpHeaderInternal::trimOptionalWhitespace(value);
    SC_TRY_MSG(not name.isEmpty(), "Set-Cookie cookie name is empty");

    if (firstSemicolon < length)
    {
        attributes = {{data + firstSemicolon + 1, length - firstSemicolon - 1}, false, header.getEncoding()};
    }

    HttpSetCookieAttributeIterator it(attributes);
    HttpHeaderKeyValue             attribute;
    while (it.next(attribute))
    {
        if (HttpHeaderInternal::equalsIgnoreCase(attribute.name, "Path") and attribute.hasValue)
        {
            path = attribute.value;
        }
        else if (HttpHeaderInternal::equalsIgnoreCase(attribute.name, "Domain") and attribute.hasValue)
        {
            domain = attribute.value;
        }
        else if (HttpHeaderInternal::equalsIgnoreCase(attribute.name, "Expires") and attribute.hasValue)
        {
            expires = attribute.value;
        }
        else if (HttpHeaderInternal::equalsIgnoreCase(attribute.name, "Max-Age") and attribute.hasValue)
        {
            maxAge    = attribute.value;
            hasMaxAge = true;
        }
        else if (HttpHeaderInternal::equalsIgnoreCase(attribute.name, "SameSite") and attribute.hasValue)
        {
            sameSite = attribute.value;
        }
        else if (HttpHeaderInternal::equalsIgnoreCase(attribute.name, "Secure") and not attribute.hasValue)
        {
            secure = true;
        }
        else if (HttpHeaderInternal::equalsIgnoreCase(attribute.name, "HttpOnly") and not attribute.hasValue)
        {
            httpOnly = true;
        }
    }

    return Result(true);
}

Result HttpSetCookieBuilder::writeTo(Span<char> storage, StringSpan& output) const
{
    output        = {};
    size_t offset = 0;
    SC_TRY_MSG(not name.isEmpty(), "Set-Cookie cookie name is empty");
    SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, name), "Set-Cookie output buffer is too small");
    SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "="), "Set-Cookie output buffer is too small");
    SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, value), "Set-Cookie output buffer is too small");
    if (not path.isEmpty())
    {
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "; Path="), "Set-Cookie output buffer is too small");
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, path), "Set-Cookie output buffer is too small");
    }
    if (not domain.isEmpty())
    {
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "; Domain="), "Set-Cookie output buffer is too small");
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, domain), "Set-Cookie output buffer is too small");
    }
    if (not expires.isEmpty())
    {
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "; Expires="),
                   "Set-Cookie output buffer is too small");
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, expires), "Set-Cookie output buffer is too small");
    }
    if (not maxAge.isEmpty())
    {
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "; Max-Age="),
                   "Set-Cookie output buffer is too small");
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, maxAge), "Set-Cookie output buffer is too small");
    }
    if (secure)
    {
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "; Secure"), "Set-Cookie output buffer is too small");
    }
    if (httpOnly)
    {
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "; HttpOnly"),
                   "Set-Cookie output buffer is too small");
    }
    if (not sameSite.isEmpty())
    {
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "; SameSite="),
                   "Set-Cookie output buffer is too small");
        SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, sameSite), "Set-Cookie output buffer is too small");
    }

    output = {{storage.data(), offset}, false, StringEncoding::Ascii};
    return Result(true);
}

StringSpan HttpContentTypeTextPlainUtf8() { return "text/plain; charset=utf-8"; }

StringSpan HttpContentTypeTextHtmlUtf8() { return "text/html; charset=utf-8"; }

StringSpan HttpContentTypeApplicationJson() { return "application/json"; }

StringSpan HttpContentTypeApplicationOctetStream() { return "application/octet-stream"; }

Result HttpCacheControlBuilder::writeTo(Span<char> storage, StringSpan& output) const
{
    output = {};
    SC_TRY_MSG(not(publicCache and privateCache), "Cache-Control cannot be both public and private");

    size_t offset = 0;
    bool   first  = true;
    if (noStore)
    {
        SC_TRY(HttpHeaderInternal::appendCacheDirective(storage, offset, first, "no-store"));
    }
    if (noCache)
    {
        SC_TRY(HttpHeaderInternal::appendCacheDirective(storage, offset, first, "no-cache"));
    }
    if (publicCache)
    {
        SC_TRY(HttpHeaderInternal::appendCacheDirective(storage, offset, first, "public"));
    }
    if (privateCache)
    {
        SC_TRY(HttpHeaderInternal::appendCacheDirective(storage, offset, first, "private"));
    }
    if (hasMaxAge)
    {
        SC_TRY(HttpHeaderInternal::appendCacheDirective(storage, offset, first, "max-age="));
        SC_TRY_MSG(HttpHeaderInternal::appendUnsigned(storage, offset, maxAgeSeconds),
                   "Cache-Control output buffer is too small");
    }
    if (mustRevalidate)
    {
        SC_TRY(HttpHeaderInternal::appendCacheDirective(storage, offset, first, "must-revalidate"));
    }
    if (immutable)
    {
        SC_TRY(HttpHeaderInternal::appendCacheDirective(storage, offset, first, "immutable"));
    }

    SC_TRY_MSG(not first, "Cache-Control builder has no directives");
    output = {{storage.data(), offset}, false, StringEncoding::Ascii};
    return Result(true);
}

Result HttpAuthorizationView::parse(StringSpan authorizationHeader)
{
    *this = {};

    StringSpan  header = HttpHeaderInternal::trimOptionalWhitespace(authorizationHeader);
    const char* data   = header.bytesWithoutTerminator();
    size_t      split  = static_cast<size_t>(-1);
    for (size_t idx = 0; idx < header.sizeInBytes(); ++idx)
    {
        if (data[idx] == ' ' or data[idx] == '\t')
        {
            split = idx;
            break;
        }
    }
    SC_TRY_MSG(split != static_cast<size_t>(-1), "Authorization header missing credentials");

    scheme                 = {{data, split}, false, header.getEncoding()};
    size_t credentialStart = split;
    while (credentialStart < header.sizeInBytes() and (data[credentialStart] == ' ' or data[credentialStart] == '\t'))
    {
        credentialStart++;
    }
    SC_TRY_MSG(credentialStart < header.sizeInBytes(), "Authorization header missing credentials");
    credentials = {{data + credentialStart, header.sizeInBytes() - credentialStart}, false, header.getEncoding()};
    credentials = HttpHeaderInternal::trimOptionalWhitespace(credentials);
    return Result(true);
}

bool HttpAuthorizationView::isBearer() const { return HttpHeaderInternal::equalsIgnoreCase(scheme, "Bearer"); }

bool HttpAuthorizationView::isBasic() const { return HttpHeaderInternal::equalsIgnoreCase(scheme, "Basic"); }

Result HttpParseBearerToken(StringSpan authorizationHeader, StringSpan& token)
{
    token = {};

    HttpAuthorizationView authorization;
    SC_TRY(authorization.parse(authorizationHeader));
    SC_TRY_MSG(authorization.isBearer(), "Authorization scheme is not Bearer");
    token = authorization.credentials;
    return Result(true);
}

Result HttpParseBasicCredentials(StringSpan authorizationHeader, Span<char> storage, StringSpan& username,
                                 StringSpan& password)
{
    username = {};
    password = {};

    HttpAuthorizationView authorization;
    SC_TRY(authorization.parse(authorizationHeader));
    SC_TRY_MSG(authorization.isBasic(), "Authorization scheme is not Basic");

    size_t decodedSize = 0;
    SC_TRY(HttpHeaderInternal::decodeBase64(authorization.credentials, storage, decodedSize));
    size_t colonIndex = static_cast<size_t>(-1);
    for (size_t idx = 0; idx < decodedSize; ++idx)
    {
        if (storage.data()[idx] == ':')
        {
            colonIndex = idx;
            break;
        }
    }
    SC_TRY_MSG(colonIndex != static_cast<size_t>(-1), "Basic authorization missing password separator");

    username = {{storage.data(), colonIndex}, false, StringEncoding::Ascii};
    password = {{storage.data() + colonIndex + 1, decodedSize - colonIndex - 1}, false, StringEncoding::Ascii};
    return Result(true);
}

Result HttpWriteBearerAuthorization(StringSpan token, Span<char> storage, StringSpan& output)
{
    output        = {};
    size_t offset = 0;
    SC_TRY_MSG(not token.isEmpty(), "Bearer authorization token is empty");
    SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "Bearer "), "Authorization output buffer is too small");
    SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, token), "Authorization output buffer is too small");
    output = {{storage.data(), offset}, false, StringEncoding::Ascii};
    return Result(true);
}

Result HttpWriteBasicAuthorization(StringSpan base64Credentials, Span<char> storage, StringSpan& output)
{
    output        = {};
    size_t offset = 0;
    SC_TRY_MSG(not base64Credentials.isEmpty(), "Basic authorization credentials are empty");
    SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, "Basic "), "Authorization output buffer is too small");
    SC_TRY_MSG(HttpHeaderInternal::appendTo(storage, offset, base64Credentials),
               "Authorization output buffer is too small");
    output = {{storage.data(), offset}, false, StringEncoding::Ascii};
    return Result(true);
}

} // namespace SC
