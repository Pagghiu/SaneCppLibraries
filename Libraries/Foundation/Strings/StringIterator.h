// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Language/Assert.h" //SC_UNREACHABLE
#include "../Language/Types.h"
#include "../Objects/Span.h"

namespace SC
{
using StringCodePoint = uint32_t;
enum class StringEncoding : uint8_t
{
    Ascii = 0,
    Utf8  = 1,
    Utf16 = 2,
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
    case StringEncoding::Utf16: return 2;
    case StringEncoding::Ascii: return 1;
    case StringEncoding::Utf8: return 1;
    }
    SC_UNREACHABLE();
}

struct StringIteratorSkipTable
{
    bool matches[256] = {false};
    constexpr StringIteratorSkipTable(Span<const char> chars)
    {
        for (auto c : chars)
        {
            matches[static_cast<int>(c)] = true;
        }
    }
};

// Invariants: start <= end and it >= start and it <= end
template <typename TypeUnit, typename CharIterator>
struct StringIterator
{
    static constexpr StringEncoding getEncoding() { return CharIterator::getEncoding(); }

    using CodeUnit  = TypeUnit;
    using CodePoint = StringCodePoint;

    [[nodiscard]] constexpr bool isAtEnd() const { return it == end; }
    [[nodiscard]] constexpr bool isAtStart() const { return it == start; }

    constexpr StringIterator& setToStart()
    {
        it = start;
        return *this;
    }

    constexpr StringIterator& setToEnd()
    {
        it = end;
        return *this;
    }

    [[nodiscard]] constexpr bool advanceUntilMatches(CodePoint c)
    {
        while (it != end)
        {
            if (CharIterator::decode(it) == c)
                return true;
            it = getNextOf(it);
        }
        return false;
    }

    [[nodiscard]] constexpr bool reverseAdvanceUntilMatches(CodePoint c)
    {
        while (it != start)
        {
            it = getPreviousOf(it);
            if (CharIterator::decode(it) == c)
                return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceAfterFinding(StringIterator other)
    {
        const size_t thisLength  = static_cast<size_t>(end - it);
        const size_t otherLength = static_cast<size_t>(other.end - other.it);

        if (otherLength > thisLength)
        {
            return false;
        }

        if (otherLength == thisLength)
        {
            if (memcmp(it, other.it, otherLength * sizeof(CodeUnit)) == 0)
            {
                setToEnd();
                return true;
            }
            return false;
        }

        const size_t difference = thisLength - otherLength;
        for (size_t index = 0; index <= difference; index++)
        {
            size_t subIndex = 0;
            for (subIndex = 0; subIndex < otherLength; subIndex++)
            {
                if (it[index + subIndex] != other.it[subIndex])
                    break;
            }
            if (subIndex == otherLength)
            {
                it += index + subIndex;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceBeforeFinding(StringIterator other)
    {
        if (advanceAfterFinding(other))
        {
            return advanceOfBytes(other.it - other.end);
        }
        return false;
    }

  private:
    [[nodiscard]] constexpr bool advanceOfBytes(ssize_t bytesLength)
    {
        auto newIt = it + bytesLength;
        if (newIt >= start and newIt <= end)
        {
            it = newIt;
            return true;
        }
        return false;
    }

  public:
    [[nodiscard]] constexpr bool advanceByLengthOf(StringIterator other)
    {
        return advanceOfBytes(other.end - other.it);
    }

    [[nodiscard]] constexpr bool advanceUntilMatchesAny(Span<const CodePoint> items, CodePoint& matched)
    {
        while (it != end)
        {
            const auto decoded = CharIterator::decode(it);
            for (auto c : items)
            {
                if (decoded == c)
                {
                    matched = c;
                    return true;
                }
            }
            it = getNextOf(it);
        }
        return false;
    }

    /// returns true if it finds at least one character different from c, false if iterator ends before finding it
    [[nodiscard]] constexpr bool advanceUntilDifferentFrom(CodePoint c, CodePoint* optionalReadChar = nullptr)
    {
        while (it != end)
        {
            auto readChar = CharIterator::decode(it);
            if (readChar != c)
            {
                if (optionalReadChar)
                {
                    *optionalReadChar = readChar;
                }
                return true;
            }
            it = getNextOf(it);
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceIfMatches(CodePoint c)
    {
        if (it != end && CharIterator::decode(it) == c)
        {
            it = getNextOf(it);
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceBackwardIfMatches(CodePoint c)
    {
        if (it != start)
        {
            auto otherIt = getPreviousOf(it);
            if (CharIterator::decode(otherIt) == c)
            {
                it = otherIt;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceIfMatchesAny(Span<const CodePoint> items)
    {
        if (it != end)
        {
            const auto decoded = CharIterator::decode(it);
            for (auto c : items)
            {
                if (decoded == c)
                {
                    it = getNextOf(it);
                    return true;
                }
            }
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceIfMatchesRange(CodePoint first, CodePoint last)
    {
        SC_RELEASE_ASSERT(first <= last);
        if (it != end)
        {
            const auto decoded = CharIterator::decode(it);
            if (decoded >= first and decoded <= last)
            {
                it = getNextOf(it);
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] constexpr bool match(CodePoint c) { return it != end and CharIterator::decode(it) == c; }

    [[nodiscard]] constexpr bool advanceRead(CodePoint& c)
    {
        if (it != end)
        {
            c  = CharIterator::decode(it);
            it = getNextOf(it);
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool read(CodePoint& c)
    {
        if (it != end)
        {
            c = CharIterator::decode(it);
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool advanceBackwardRead(CodePoint& c)
    {
        if (it != start)
        {
            it = getPreviousOf(it);
            c  = CharIterator::decode(it);
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool stepForward()
    {
        if (it != end)
        {
            it = getNextOf(it);
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool stepBackward()
    {
        if (it != start)
        {
            it = getPreviousOf(it);
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
            it = getNextOf(it);
        }
        return true;
    }

    [[nodiscard]] constexpr bool reverseAdvanceCodePoints(size_t numCodePoints)
    {
        while (numCodePoints-- > 0)
        {
            if (it == start)
            {
                return false;
            }
            it = getPreviousOf(it);
        }
        return true;
    }

    [[nodiscard]] constexpr bool isFollowedBy(CodePoint c)
    {
        if (it != end)
        {
            return CharIterator::decode(getNextOf(it)) == c;
        }
        return false;
    }

    [[nodiscard]] constexpr bool isPrecededBy(CodePoint c)
    {
        if (it != start)
        {
            return CharIterator::decode(getPreviousOf(it)) == c;
        }
        return false;
    }

    constexpr StringIterator sliceFromStartUntil(StringIterator otherPoint) const
    {
        SC_RELEASE_ASSERT(it <= otherPoint.it);
        return StringIterator(it, otherPoint.it);
    }

    [[nodiscard]] constexpr ssize_t bytesDistanceFrom(StringIterator other) const
    {
        return (it - other.it) * static_cast<ssize_t>(sizeof(CodeUnit));
    }

    template <typename CharType>
    [[nodiscard]] constexpr bool endsWithChar(CharType character) const
    {
        if (start != end)
        {
            auto last = CharIterator::getPreviousOf(end);
            return CharIterator::decode(last) == character;
        }
        return false;
    }
    template <typename CharType>
    [[nodiscard]] constexpr bool startsWithChar(CharType character) const
    {
        if (start != end)
        {
            return CharIterator::decode(start) == character;
        }
        return false;
    }

    template <typename IteratorType>
    [[nodiscard]] constexpr bool endsWith(IteratorType other) const
    {
        StringIterator copy = *this;
        copy.setToEnd();
        other.setToEnd();
        typename IteratorType::CodePoint c;
        while (other.advanceBackwardRead(c))
        {
            if (not copy.advanceBackwardIfMatches(c))
                return false;
        }
        return other.isAtStart();
    }

    template <typename IteratorType>
    [[nodiscard]] constexpr bool startsWith(IteratorType other) const
    {
        StringIterator copy = *this;
        copy.setToStart();
        other.setToStart();
        typename IteratorType::CodePoint c;
        while (other.advanceRead(c))
        {
            if (not copy.advanceIfMatches(c))
                return false;
        }
        return other.isAtEnd();
    }

  protected:
    friend struct StringView;
    static constexpr const CodeUnit* getNextOf(const CodeUnit* src) { return CharIterator::getNextOf(src); }
    static constexpr const CodeUnit* getPreviousOf(const CodeUnit* src) { return CharIterator::getPreviousOf(src); }
    constexpr StringIterator(const CodeUnit* it, const CodeUnit* end) : it(it), start(it), end(end) {}
    constexpr auto* getCurrentIt() const { return it; }
    const CodeUnit* it;
    const CodeUnit* start;
    const CodeUnit* end;
};

struct StringIteratorASCII : public StringIterator<char, StringIteratorASCII>
{
    [[nodiscard]] constexpr bool advanceUntilMatches(CodePoint c)
    {
        if (__builtin_is_constant_evaluated())
        {
            return Parent::advanceUntilMatches(c);
        }
        else
        {
            if (not isValidASCII(c))
                SC_UNLIKELY
                {
                    it = end;
                    return false;
                }
            char charC = static_cast<char>(c);
            auto res   = memchr(it, charC, static_cast<size_t>(end - it));
            if (res != nullptr)
                it = static_cast<const char*>(res);
            else
                it = end;
            return it != end;
        }
    }

  private:
    using StringIterator::StringIterator;
    using Parent = StringIterator<char, StringIteratorASCII>;
    friend Parent;
    friend struct StringView;

    [[nodiscard]] static constexpr StringEncoding getEncoding() { return StringEncoding::Ascii; }

    [[nodiscard]] static constexpr const char* getNextOf(const char* src) { return src + 1; }
    [[nodiscard]] static constexpr const char* getPreviousOf(const char* src) { return src - 1; }
    [[nodiscard]] static constexpr CodePoint   decode(const char* src) { return static_cast<CodePoint>(*src); }
    [[nodiscard]] static constexpr bool        isValidASCII(CodePoint c) { return c <= 127; }
};

struct StringIteratorUTF16 : public StringIterator<wchar_t, StringIteratorUTF16>
{
  private:
    using StringIterator::StringIterator;
    using Parent = StringIterator<wchar_t, StringIteratorUTF16>;
    friend Parent;
    friend struct StringView;

    [[nodiscard]] static constexpr StringEncoding getEncoding() { return StringEncoding::Utf16; }

    [[nodiscard]] static constexpr const wchar_t* getNextOf(const wchar_t* src)
    {
        if ((src[0] & 0x80) == 0)
        {
            return src + 1; // Single-byte character
        }
        return src + 2; // Multi-byte character
    }

    [[nodiscard]] static constexpr const wchar_t* getPreviousOf(const wchar_t* src)
    {
        src = src - 1;
        if (src[0] >= 0xDC00 and src[0] <= 0xDFFF)
        {
            src = src - 1;
        }
        return src;
    }

    [[nodiscard]] static constexpr uint32_t decode(const wchar_t* src)
    {
        const uint32_t character = static_cast<uint32_t>(src[0]);
        if (character >= 0xD800 and character <= 0xDFFF)
        {
            const uint32_t nextCharacter = static_cast<uint32_t>(src[1]);
            if (nextCharacter >= 0xDC00)
            {
                return 0x10000 + ((nextCharacter - 0xDC00) | ((character - 0xD800) << 10));
            }
        }
        return character;
    }
};

struct StringIteratorUTF8 : public StringIterator<char, StringIteratorUTF8>
{
  private:
    using Parent = StringIterator<char, StringIteratorUTF8>;
    friend Parent;
    friend struct StringView;
    using StringIterator::StringIterator;

    [[nodiscard]] static constexpr StringEncoding getEncoding() { return StringEncoding::Utf8; }

    [[nodiscard]] static constexpr const char* getNextOf(const char* src)
    {
        char character = src[0];
        if ((character & 0x80) == 0)
        {
            return src + 1;
        }
        else if ((character & 0xE0) == 0xC0)
        {
            return src + 2;
        }
        else if ((character & 0xF0) == 0xE0)
        {
            return src + 3;
        }
        return src + 4;
    }

    [[nodiscard]] static constexpr const char* getPreviousOf(const char* src)
    {
        do
        {
            --src;
        } while ((*src & 0xC0) == 0x80);
        return src;
    }

    [[nodiscard]] static constexpr uint32_t decode(const char* src)
    {
        uint32_t character = static_cast<uint32_t>(src[0]);
        if ((character & 0x80) == 0)
        {
            return character;
        }
        else if ((character & 0xE0) == 0xC0)
        {
            character = src[0] & 0x1F;
            character = (character << 6) | (src[1] & 0x3F);
        }
        else if ((character & 0xF0) == 0xE0)
        {
            character = src[0] & 0x0F;
            character = (character << 6) | (src[1] & 0x3F);
            character = (character << 6) | (src[2] & 0x3F);
        }
        else
        {
            character = src[0] & 0x07;
            character = (character << 6) | (src[1] & 0x3F);
            character = (character << 6) | (src[2] & 0x3F);
            character = (character << 6) | (src[3] & 0x3F);
        }
        return character;
    }
};
#if SC_PLATFORM_WINDOWS
using StringIteratorNative = StringIteratorUTF16;
#else
using StringIteratorNative = StringIteratorUTF8;
#endif

} // namespace SC
