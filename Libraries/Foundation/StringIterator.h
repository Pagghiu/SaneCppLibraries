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

constexpr bool StringEncodingAreBinaryCompatible(StringEncoding encoding1, StringEncoding encoding2)
{
    return (encoding1 == encoding2) or (encoding2 == StringEncoding::Ascii && encoding1 == StringEncoding::Utf8) or
           (encoding2 == StringEncoding::Utf8 && encoding1 == StringEncoding::Ascii);
}

constexpr uint32_t StringEncodingGetSize(StringEncoding encoding)
{
    switch (encoding)
    {
    case StringEncoding::Utf32: return 4;
    case StringEncoding::Utf16: return 2;
    default: return 1;
    }
}

// Invariants: start <= end and it >= start and it <= end
struct StringIteratorASCII
{
    static constexpr StringEncoding getEncoding() { return StringEncoding::Ascii; }

    [[nodiscard]] constexpr bool isEmpty() const { return it == end; }

    constexpr void setToStart() { it = start; }

    constexpr void setToEnd() { it = end; }

    [[nodiscard]] constexpr bool advanceUntilMatches(char c)
    {
        if (__builtin_is_constant_evaluated())
        {
            while (it != end)
            {
                if (*it == c)
                    return true;
                ++it;
            }
            return false;
        }
        else
        {
            auto res = memchr(it, c, static_cast<size_t>(end - it));
            if (res != nullptr)
                it = static_cast<const char*>(res);
            else
                it = end;
            return it != end;
        }
    }

    [[nodiscard]] constexpr bool reverseAdvanceUntilMatches(char c)
    {
        while (it != start)
        {
            --it;
            if (*it == c)
                return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceUntilMatches(char c1, char c2, char* matched)
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

    constexpr void advanceUntilDifferentFrom(char c)
    {
        while (it != end)
        {
            if (*it != c)
            {
                break;
            }
            ++it;
        }
    }

    [[nodiscard]] constexpr bool advanceIfMatches(char c)
    {
        if (it != end && *it == c)
        {
            ++it;
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceIfMatchesAny(std::initializer_list<char> items)
    {
        if (it != end)
        {
            for (auto c : items)
            {
                if (*it == c)
                {
                    ++it;
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceIfMatchesRange(char first, char last)
    {
        SC_RELEASE_ASSERT(first <= last);
        if (it != end)
        {
            if (*it >= first and *it <= last)
            {
                ++it;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] constexpr bool match(char c) { return it != end and *it == c; }

    [[nodiscard]] constexpr bool advanceRead(char& c)
    {
        if (it != end)
        {
            c = *it;
            ++it;
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool stepForward()
    {
        if (it != end)
        {
            ++it;
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool stepBackward()
    {
        if (it != start)
        {
            --it;
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceCodePoints(size_t numCodePoints)
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

    [[nodiscard]] constexpr bool isFollowedBy(char c)
    {
        if (it != end)
        {
            return it[1] == c;
        }
        return false;
    }

    [[nodiscard]] constexpr bool isPrecededBy(char c) const
    {
        if (it != start)
        {
            return it[-1] == c;
        }
        return false;
    }

    constexpr StringIteratorASCII sliceFromStartUntil(StringIteratorASCII otherPoint) const
    {
        SC_RELEASE_ASSERT(it <= otherPoint.it);
        return StringIteratorASCII(it, otherPoint.it);
    }

    [[nodiscard]] constexpr ssize_t bytesDistanceFrom(StringIteratorASCII other) const { return it - other.it; }

  private:
    constexpr StringIteratorASCII(const char* it, const char* end) : it(it), start(it), end(end) {}
    constexpr const char* getCurrentIt() const { return it; }
    friend struct StringView;
    const char* it;
    const char* start;
    const char* end;
};

} // namespace SC
