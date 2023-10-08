// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Assert.h" //Assert::unreachable
#include "../Base/Types.h"
#include "../Language/Span.h"

namespace SC
{
using StringCodePoint = uint32_t;
enum class StringEncoding : uint8_t
{
    Ascii = 0,
    Utf8  = 1,
    Utf16 = 2, // Little Endian
#if SC_PLATFORM_WINDOWS
    Native = Utf16,
    Wide   = Utf16
#else
    Native = Utf8
#endif
};

constexpr bool     StringEncodingAreBinaryCompatible(StringEncoding encoding1, StringEncoding encoding2);
constexpr uint32_t StringEncodingGetSize(StringEncoding encoding);

// Invariants: start <= end and it >= start and it <= end
template <typename CharIterator>
struct SC_COMPILER_EXPORT StringIterator
{
    static constexpr StringEncoding getEncoding() { return CharIterator::getEncoding(); }

    using CodeUnit  = char;
    using CodePoint = StringCodePoint;

    constexpr void setToStart() { it = start; }

    constexpr void setToEnd() { it = end; }

    [[nodiscard]] constexpr bool isAtEnd() const { return it >= end; }

    [[nodiscard]] constexpr bool isAtStart() const { return it <= start; }

    [[nodiscard]] constexpr bool advanceUntilMatches(CodePoint c);

    [[nodiscard]] bool reverseAdvanceUntilMatches(CodePoint c);

    [[nodiscard]] bool advanceAfterFinding(StringIterator other);

    [[nodiscard]] bool advanceBeforeFinding(StringIterator other);

    [[nodiscard]] bool advanceByLengthOf(StringIterator other) { return advanceOfBytes(other.end - other.it); }

    [[nodiscard]] bool advanceUntilMatchesAny(Span<const CodePoint> items, CodePoint& matched);

    /// Returns true if it finds at least one character different from c, false if iterator ends before finding it
    [[nodiscard]] bool advanceUntilDifferentFrom(CodePoint c, CodePoint* optionalReadChar = nullptr);

    [[nodiscard]] constexpr bool advanceIfMatches(CodePoint c);

    [[nodiscard]] bool advanceBackwardIfMatches(CodePoint c);

    [[nodiscard]] bool advanceIfMatchesAny(Span<const CodePoint> items);

    [[nodiscard]] bool advanceIfMatchesRange(CodePoint first, CodePoint last);

    [[nodiscard]] bool match(CodePoint c) { return it < end and CharIterator::decode(it) == c; }

    [[nodiscard]] constexpr bool advanceRead(CodePoint& c);

    [[nodiscard]] bool read(CodePoint& c);

    [[nodiscard]] bool advanceBackwardRead(CodePoint& c);

    [[nodiscard]] constexpr bool stepForward();

    [[nodiscard]] constexpr bool stepBackward();

    [[nodiscard]] constexpr bool advanceCodePoints(size_t numCodePoints);

    [[nodiscard]] bool reverseAdvanceCodePoints(size_t numCodePoints);

    [[nodiscard]] constexpr bool isFollowedBy(CodePoint c);

    [[nodiscard]] constexpr bool isPrecededBy(CodePoint c);

    [[nodiscard]] constexpr StringIterator sliceFromStartUntil(StringIterator otherPoint) const;

    [[nodiscard]] constexpr ssize_t bytesDistanceFrom(StringIterator other) const;

    [[nodiscard]] bool endsWithChar(CodePoint character) const;

    [[nodiscard]] bool startsWithChar(CodePoint character) const;

    template <typename IteratorType>
    [[nodiscard]] bool endsWith(IteratorType other) const;

    template <typename IteratorType>
    [[nodiscard]] bool startsWith(IteratorType other) const;

  protected:
    [[nodiscard]] bool advanceOfBytes(ssize_t bytesLength);

    friend struct StringView;
    static constexpr const CodeUnit* getNextOf(const CodeUnit* src) { return CharIterator::getNextOf(src); }
    static constexpr const CodeUnit* getPreviousOf(const CodeUnit* src) { return CharIterator::getPreviousOf(src); }
    constexpr StringIterator(const CodeUnit* it, const CodeUnit* end) : it(it), start(it), end(end) {}
    constexpr auto* getCurrentIt() const { return it; }
    const CodeUnit* it;
    const CodeUnit* start;
    const CodeUnit* end;
};

struct SC_COMPILER_EXPORT StringIteratorASCII : public StringIterator<StringIteratorASCII>
{
    [[nodiscard]] constexpr bool advanceUntilMatches(CodePoint c);

  private:
    [[nodiscard]] bool advanceUntilMatchesNonConstexpr(CodePoint c);
    using StringIterator::StringIterator;
    using Parent = StringIterator<StringIteratorASCII>;
    friend Parent;
    friend struct StringView;

    [[nodiscard]] static constexpr StringEncoding getEncoding() { return StringEncoding::Ascii; }

    [[nodiscard]] static constexpr const char* getNextOf(const char* src) { return src + 1; }
    [[nodiscard]] static constexpr const char* getPreviousOf(const char* src) { return src - 1; }
    [[nodiscard]] static constexpr CodePoint   decode(const char* src) { return static_cast<CodePoint>(*src); }
};

struct SC_COMPILER_EXPORT StringIteratorUTF16 : public StringIterator<StringIteratorUTF16>
{
  private:
    using StringIterator::StringIterator;
    using Parent = StringIterator<StringIteratorUTF16>;
    friend Parent;
    friend struct StringView;

    [[nodiscard]] static StringEncoding getEncoding() { return StringEncoding::Utf16; }

    [[nodiscard]] static const char* getNextOf(const char* bytes);

    [[nodiscard]] static const char* getPreviousOf(const char* bytes);

    [[nodiscard]] static uint32_t decode(const char* bytes);
};

struct SC_COMPILER_EXPORT StringIteratorUTF8 : public StringIterator<StringIteratorUTF8>
{
  private:
    using Parent = StringIterator<StringIteratorUTF8>;
    friend Parent;
    friend struct StringView;
    using StringIterator::StringIterator;

    [[nodiscard]] static StringEncoding getEncoding() { return StringEncoding::Utf8; }

    [[nodiscard]] static const char* getNextOf(const char* src);

    [[nodiscard]] static const char* getPreviousOf(const char* src);

    [[nodiscard]] static uint32_t decode(const char* src);
};

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

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------
constexpr bool StringEncodingAreBinaryCompatible(StringEncoding encoding1, StringEncoding encoding2)
{
    return (encoding1 == encoding2) or (encoding2 == StringEncoding::Ascii and encoding1 == StringEncoding::Utf8) or
           (encoding2 == StringEncoding::Utf8 and encoding1 == StringEncoding::Ascii);
}

constexpr uint32_t StringEncodingGetSize(StringEncoding encoding)
{
    switch (encoding)
    {
    case StringEncoding::Utf16: return 2;
    case StringEncoding::Ascii: return 1;
    case StringEncoding::Utf8: return 1;
    }
    Assert::unreachable();
}

template <typename CharIterator>
constexpr bool StringIterator<CharIterator>::advanceUntilMatches(CodePoint c)
{
    while (it < end)
    {
        if (CharIterator::decode(it) == c)
            return true;
        it = getNextOf(it);
    }
    return false;
}

template <typename CharIterator>
constexpr bool StringIterator<CharIterator>::advanceIfMatches(CodePoint c)
{
    if (it < end and CharIterator::decode(it) == c)
    {
        it = getNextOf(it);
        return true;
    }
    return false;
}

template <typename CharIterator>
constexpr bool StringIterator<CharIterator>::advanceRead(CodePoint& c)
{
    if (it < end)
    {
        c  = CharIterator::decode(it);
        it = getNextOf(it);
        return true;
    }
    return false;
}

template <typename CharIterator>
constexpr bool StringIterator<CharIterator>::stepForward()
{
    if (it < end)
    {
        it = getNextOf(it);
        return true;
    }
    return false;
}

template <typename CharIterator>
constexpr bool StringIterator<CharIterator>::stepBackward()
{
    if (it > start)
    {
        it = getPreviousOf(it);
        return true;
    }
    return false;
}

template <typename CharIterator>
constexpr bool StringIterator<CharIterator>::advanceCodePoints(size_t numCodePoints)
{
    while (numCodePoints > 0)
    {
        numCodePoints -= 1;
        if (it >= end)
        {
            return false;
        }
        it = getNextOf(it);
    }
    return true;
}

template <typename CharIterator>
constexpr bool StringIterator<CharIterator>::isFollowedBy(CodePoint c)
{
    return it < end ? CharIterator::decode(getNextOf(it)) == c : false;
}

template <typename CharIterator>
constexpr bool StringIterator<CharIterator>::isPrecededBy(CodePoint c)
{
    return it > start ? CharIterator::decode(getPreviousOf(it)) == c : false;
}

template <typename CharIterator>
constexpr StringIterator<CharIterator> StringIterator<CharIterator>::sliceFromStartUntil(
    StringIterator otherPoint) const
{
    SC_ASSERT_RELEASE(it <= otherPoint.it);
    return StringIterator(it, otherPoint.it);
}

template <typename CharIterator>
constexpr ssize_t StringIterator<CharIterator>::bytesDistanceFrom(StringIterator other) const
{
    return (it - other.it) * static_cast<ssize_t>(sizeof(CodeUnit));
}

// StringIteratorASCII
[[nodiscard]] constexpr bool StringIteratorASCII::advanceUntilMatches(CodePoint c)
{
    return __builtin_is_constant_evaluated() ? StringIterator::advanceUntilMatches(c)
                                             : advanceUntilMatchesNonConstexpr(c);
}

} // namespace SC
