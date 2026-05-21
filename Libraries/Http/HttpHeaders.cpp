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
};

HttpCookieIterator::HttpCookieIterator(StringSpan cookieHeader) : header(cookieHeader) {}

bool HttpCookieIterator::next(HttpHeaderKeyValue& pair)
{
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

Result HttpAuthorizationView::parse(StringSpan authorizationHeader)
{
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
    HttpAuthorizationView authorization;
    SC_TRY(authorization.parse(authorizationHeader));
    SC_TRY_MSG(authorization.isBearer(), "Authorization scheme is not Bearer");
    token = authorization.credentials;
    return Result(true);
}

Result HttpParseBasicCredentials(StringSpan authorizationHeader, Span<char> storage, StringSpan& username,
                                 StringSpan& password)
{
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

} // namespace SC
