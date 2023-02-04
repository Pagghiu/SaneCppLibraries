// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Assert.h"
#include "Types.h"

namespace SC
{
enum class StringEncoding : uint8_t
{
    Ascii = 0,
    Utf8  = 1,
    Utf16 = 2,
    Utf32 = 3,
#if SC_PLATFORM_WINDOWS
    Native = Utf16
#else
    Native = Utf8
#endif
};

constexpr uint32_t StringEncodingGetSize(StringEncoding encoding)
{
    switch (encoding)
    {
    case StringEncoding::Utf32: return 4;
    case StringEncoding::Utf16: return 2;
    default: return 1;
    }
}

struct StringIteratorASCII
{
    StringIteratorASCII(const char_t* it, const char_t* end) : it(it), start(it), end(end) {}

    static StringEncoding getEncoding() { return StringEncoding::Ascii; }

    void rewindToStart() { it = start; }
    void rewindToEnd() { it = end; }

    [[nodiscard]] bool advanceUntilMatches(char_t c)
    {
#if 1
        auto res = memchr(it, c, end - it); // may be faster with longer strings...
        if (res != nullptr)
            it = static_cast<const SC::char_t*>(res);
        else
            it = end;
        return it != end;
#else
        while (it != end)
        {
            if (*it == c)
                return true;
            ++it;
        }
        return false;
#endif
    }
    [[nodiscard]] bool reverseUntilMatches(char_t c)
    {
        while (it != start)
        {
            --it;
            if (*it == c)
                return true;
        }
        return false;
    }

    [[nodiscard]] bool advanceUntilMatches(char_t c1, char_t c2, char_t* matched)
    {
        while (it != end)
        {
            if (*it == c1)
            {
                *matched = c1;
                return true;
            }
            if (*it == c2)
            {
                *matched = c2;
                return true;
            }

            ++it;
        }
        return false;
    }

    [[nodiscard]] bool advanceUntilMatchesAfter(char_t c)
    {
        if (advanceUntilMatches(c))
        {
            ++it;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool isEmpty() const { return it == end; }

    [[nodiscard]] bool matches(char_t c) const { return *it == c; }

    [[nodiscard]] bool matchesAny(std::initializer_list<char> items) const
    {
        for (auto i : items)
        {
            if (*it == i)
                return true;
        }
        return false;
    }

    [[nodiscard]] bool skipNext()
    {
        if (it != end)
        {
            ++it;
            return it != end;
        }
        return false;
    }

    [[nodiscard]] bool skipPrev()
    {
        if (it != start)
        {
            --it;
            return it != start;
        }
        return false;
    }

    [[nodiscard]] bool advanceCodePoints(size_t numCodePoints)
    {
        while (numCodePoints-- > 0)
        {
            if (it == end)
            {
                return false;
            }
            ++it;
        }
        return true;
    }

    [[nodiscard]] bool isFollowedBy(char_t c)
    {
        if (it != end)
        {
            return it[1] == c;
        }
        return false;
    }

    StringIteratorASCII untilBefore(StringIteratorASCII otherPoint) const
    {
        SC_RELEASE_ASSERT(it + 1 <= otherPoint.it);
        return StringIteratorASCII(it, otherPoint.it - 1);
    }

    [[nodiscard]] size_t bytesDistanceFrom(StringIteratorASCII other) const { return it - other.it; }

    Span<const char> sliceUntil(StringIteratorASCII other) const
    {
        if (other.it >= start && other.it <= end)
        {
            return Span<const char>(it, other.it - it);
        }
        return {};
    }
    Span<const char> sliceUntilEnd() const { return Span<const char>(it, end - it); }
    Span<const char> sliceFromStart() const { return Span<const char>(start, it - start); }

    const char_t* getStart() const { return start; }
    const char_t* getIt() const { return it; }
    const char_t* getEnd() const { return end; }

  private:
    const char_t* it;
    const char_t* start;
    const char_t* end;
};

} // namespace SC
