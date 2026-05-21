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

} // namespace SC
