// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/StringSpan.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace SC
{
struct HttpStringIterator
{
    static bool equalsIgnoreCase(StringSpan a, StringSpan b)
    {
        if (a.sizeInBytes() != b.sizeInBytes())
            return false;
        HttpStringIterator it1 = a;
        HttpStringIterator it2 = b;

        char c1, c2;
        while (it1.advanceRead(c1) and it2.advanceRead(c2))
        {
            if (::tolower(static_cast<int>(c1)) != ::tolower(static_cast<int>(c2)))
                return false;
        }
        return true;
    }

    HttpStringIterator(StringSpan string)
    {
        start = string.bytesWithoutTerminator();
        end   = start + string.sizeInBytes();
        it    = start;
    }

    [[nodiscard]] bool advanceUntilMatches(char c)
    {
        if (it == end)
        {
            return false;
        }
        auto found = ::memchr(it, c, static_cast<size_t>(end - it));
        if (found != nullptr)
        {
            it = static_cast<const char*>(found);
            return true;
        }
        it = end;
        return false;
    }

    [[nodiscard]] bool advanceIfMatches(char c)
    {
        if (it == end)
        {
            return false;
        }
        if (*it == c)
        {
            ++it;
            return true;
        }
        else
        {
            return false;
        }
    }
    [[nodiscard]] bool match(char c)
    {
        if (it != end)
            return *it == c;
        return false;
    }

    [[nodiscard]] bool isAtEnd() const { return it == end; }

    [[nodiscard]] bool advanceUntilMatchesAny(Span<const char> items, char& matched)
    {
        while (it < end)
        {
            const auto decoded = *it;
            for (auto c : items)
            {
                if (decoded == c)
                {
                    matched = c;
                    return true;
                }
            }
            it++;
        }
        return false;
    }

    [[nodiscard]] bool stepForward()
    {
        if (it != end)
        {
            ++it;
            return true;
        }
        return false;
    }

    void setToEnd() { it = end; }

    [[nodiscard]] bool reverseAdvanceUntilMatches(char c)
    {
        if (it == start)
        {
            return false;
        }
        --it;
        while (true)
        {
            if (*it == c)
            {
                return true;
            }
            if (it == start)
            {
                it = start;
                return false;
            }
            --it;
        }
    }

    [[nodiscard]] bool advanceRead(char& c)
    {
        if (it != end)
        {
            c = *it;
            ++it;
            return true;
        }
        return false;
    }

    const char* start;
    const char* end;
    const char* it;

    static StringSpan fromIterators(HttpStringIterator it1, HttpStringIterator it2, StringEncoding encoding)
    {
        if (it2.it < it1.it)
            return {};
        return StringSpan({it1.it, static_cast<size_t>(it2.it - it1.it)}, false, encoding);
    }
    static StringSpan fromIteratorFromStart(HttpStringIterator it, StringEncoding encoding)
    {
        return StringSpan({it.start, static_cast<size_t>(it.it - it.start)}, false, encoding);
    }

    static StringSpan fromIteratorUntilEnd(HttpStringIterator it, StringEncoding encoding)
    {
        return StringSpan({it.it, static_cast<size_t>(it.end - it.it)}, false, encoding);
    }

    static bool parseInt32(StringSpan other, int32_t& value)
    {
        if (other.getNullTerminatedNative() == nullptr or other.getEncoding() == StringEncoding::Utf16)
            return false;
        char        buffer[12]; // 10 digits + sign + nullTerm
        const char* parseText;
        if (other.isNullTerminated())
        {
            parseText = other.bytesIncludingTerminator();
        }
        else
        {
            if (other.sizeInBytes() >= sizeof(buffer))
                return false;
            ::memcpy(buffer, other.bytesWithoutTerminator(), other.sizeInBytes());
            buffer[other.sizeInBytes()] = 0;
            parseText                   = buffer;
        }
        errno = 0;
        char* endText;
        auto  parsed = ::strtol(parseText, &endText, 10);
        if (errno == 0 && parseText < endText)
        {
            if (parsed >= INT32_MIN and parsed <= INT32_MAX)
            {
                value = static_cast<int32_t>(parsed);
                return true;
            }
        }
        return false;
    }

    static bool containsCodePoint(StringSpan str, char c)
    {
        if (::memchr(str.bytesWithoutTerminator(), c, str.sizeInBytes()) != nullptr)
            return true;
        return false;
    }

    static bool startsWith(StringSpan str, const char* prefix)
    {
        size_t prefixLen = ::strlen(prefix);
        if (str.sizeInBytes() < prefixLen)
            return false;
        return ::strncmp(str.bytesWithoutTerminator(), prefix, prefixLen) == 0;
    }

    static bool endsWith(StringSpan str, const char* suffix)
    {
        size_t suffixLen = ::strlen(suffix);
        if (str.sizeInBytes() < suffixLen)
            return false;
        return ::strncmp(str.bytesWithoutTerminator() + (str.sizeInBytes() - suffixLen), suffix, suffixLen) == 0;
    }

    static StringSpan sliceStart(StringSpan str, size_t numChars)
    {
        if (str.sizeInBytes() < numChars)
            return {};
        return StringSpan({str.bytesWithoutTerminator() + numChars, str.sizeInBytes() - numChars}, false,
                          str.getEncoding());
    }

    static bool parseNameExtension(const StringSpan input, StringSpan& name, StringSpan& extension)
    {
        HttpStringIterator it(input);

        auto itBackup = it;
        // Try searching for a '.' but if it's not found then just set the entire content
        // to be the name.
        it.setToEnd();
        if (it.reverseAdvanceUntilMatches('.'))
        {
            name = HttpStringIterator::fromIterators(itBackup, it, input.getEncoding()); // from 'name.ext' keep 'name'
            (void)it.stepForward();                                                      // skip the .
            extension = HttpStringIterator::fromIteratorUntilEnd(it, input.getEncoding()); // from 'name.ext' keep 'ext'
        }
        else
        {
            name      = input;
            extension = {};
        }
        return !(name.isEmpty() && extension.isEmpty());
    }
};
} // namespace SC
