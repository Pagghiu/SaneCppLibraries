#pragma once
#include "StringView.h"

namespace SC
{
namespace text
{

struct StringIteratorASCII
{
    StringIteratorASCII(const char_t* it, const char_t* end) : it(it), end(end) {}

    [[nodiscard]] bool advanceUntilMatches(char_t c)
    {
        while (it != end)
        {
            if (*it == c)
                return true;
            ++it;
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

    [[nodiscard]] bool skipNext()
    {
        if (it != end)
        {
            ++it;
            return true;
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

    template <typename Container>
    [[nodiscard]] bool writeBytesUntil(StringIteratorASCII other, Container& container) const
    {
        if (other.it < it)
        {
            return false;
        }
        return container.appendCopy(it, other.it - it);
    }

    template <typename Container>
    [[nodiscard]] bool insertBytesTo(Container& container, size_t idx) const
    {
        return container.insertCopy(idx, it, end - it);
    }

    [[nodiscard]] size_t bytesDistanceFrom(StringIteratorASCII other) const { return it - other.it; }

    const char_t* getStart() const { return it; }

  private:
    const char_t* it;
    const char_t* end;
};

} // namespace text
} // namespace SC
