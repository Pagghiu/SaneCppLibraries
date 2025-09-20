// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Strings/StringIterator.h"

namespace SC
{
template <typename CharIterator>
bool StringIterator<CharIterator>::reverseAdvanceUntilMatches(CodePoint c)
{
    while (it > start)
    {
        auto prevIt = it;
        it          = getPreviousOf(it, start);
        if (CharIterator::decode(it, prevIt) == c)
            return true;
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::reverseAdvanceUntilMatches(CodePoint c);
template bool StringIterator<StringIteratorUTF8>::reverseAdvanceUntilMatches(CodePoint c);
template bool StringIterator<StringIteratorUTF16>::reverseAdvanceUntilMatches(CodePoint c);

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceAfterFindingSameIterator(StringIterator other)
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

template <typename CharIterator>
template <typename OtherIterator, bool after>
bool StringIterator<CharIterator>::advanceBeforeOrAfterFinding(StringIterator<OtherIterator> other)
{
    if (StringEncodingAreBinaryCompatible(StringIterator<CharIterator>::getEncoding(),
                                          StringIterator<OtherIterator>::getEncoding()))
    {
        if (after)
            return advanceAfterFindingSameIterator(reinterpret_cast<StringIterator<CharIterator>&>(other));
        else
            return advanceBeforeFindingSameIterator(reinterpret_cast<StringIterator<CharIterator>&>(other));
    }
    auto outerIterator = *this;
    while (not outerIterator.isAtEnd())
    {
        auto innerIterator = outerIterator;
        bool matchFound    = true;

        CodePoint otherCodePoint;
        CodePoint thisCodePoint;

        auto otherIterator = other;
        while (otherIterator.advanceRead(otherCodePoint))
        {
            if (not innerIterator.advanceRead(thisCodePoint))
            {
                matchFound = false;
                break;
            }
            if (thisCodePoint != otherCodePoint)
            {
                matchFound = false;
                break;
            }
        }

        if (matchFound and otherIterator.isAtEnd())
        {
            if (after)
            {
                it = innerIterator.it;
            }
            else
            {
                it = outerIterator.it;
            }
            return true;
        }
        outerIterator.it = getNextOf(outerIterator.it, outerIterator.end);
    }
    return false;
}

// clang-format off
template bool StringIterator<StringIteratorASCII>::advanceBeforeOrAfterFinding<StringIteratorUTF8, false>(StringIterator<StringIteratorUTF8>);
template bool StringIterator<StringIteratorASCII>::advanceBeforeOrAfterFinding<StringIteratorUTF8, true>(StringIterator<StringIteratorUTF8>);
template bool StringIterator<StringIteratorASCII>::advanceBeforeOrAfterFinding<StringIteratorUTF16, false>(StringIterator<StringIteratorUTF16>);
template bool StringIterator<StringIteratorASCII>::advanceBeforeOrAfterFinding<StringIteratorUTF16, true>(StringIterator<StringIteratorUTF16>);
template bool StringIterator<StringIteratorUTF8>::advanceBeforeOrAfterFinding<StringIteratorASCII, false>(StringIterator<StringIteratorASCII>);
template bool StringIterator<StringIteratorUTF8>::advanceBeforeOrAfterFinding<StringIteratorASCII, true>(StringIterator<StringIteratorASCII>);
template bool StringIterator<StringIteratorUTF8>::advanceBeforeOrAfterFinding<StringIteratorUTF16, true>(StringIterator<StringIteratorUTF16>);
template bool StringIterator<StringIteratorUTF8>::advanceBeforeOrAfterFinding<StringIteratorUTF16, false>(StringIterator<StringIteratorUTF16>);
template bool StringIterator<StringIteratorUTF16>::advanceBeforeOrAfterFinding<StringIteratorUTF8, true>(StringIterator<StringIteratorUTF8>);
template bool StringIterator<StringIteratorUTF16>::advanceBeforeOrAfterFinding<StringIteratorUTF8, false>(StringIterator<StringIteratorUTF8>);
template bool StringIterator<StringIteratorUTF16>::advanceBeforeOrAfterFinding<StringIteratorASCII, false>(StringIterator<StringIteratorASCII>);
template bool StringIterator<StringIteratorUTF16>::advanceBeforeOrAfterFinding<StringIteratorASCII, true>(StringIterator<StringIteratorASCII>);

// clang-format on

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceBeforeFindingSameIterator(StringIterator other)
{
    if (advanceAfterFindingSameIterator(other))
    {
        return advanceOfBytes(other.it - other.end);
    }
    return false;
}

// clang-format off
template bool StringIterator<StringIteratorASCII>::advanceBeforeFindingSameIterator(StringIterator<StringIteratorASCII>);
template bool StringIterator<StringIteratorASCII>::advanceAfterFindingSameIterator(StringIterator<StringIteratorASCII>);
template bool StringIterator<StringIteratorUTF8>::advanceBeforeFindingSameIterator(StringIterator<StringIteratorUTF8>);
template bool StringIterator<StringIteratorUTF8>::advanceAfterFindingSameIterator(StringIterator<StringIteratorUTF8>);
template bool StringIterator<StringIteratorUTF16>::advanceBeforeFindingSameIterator(StringIterator<StringIteratorUTF16>);
template bool StringIterator<StringIteratorUTF16>::advanceAfterFindingSameIterator(StringIterator<StringIteratorUTF16>);
// clang-format on

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceOfBytes(ssize_t bytesLength)
{
    auto newIt = it + bytesLength;
    if (newIt >= start and newIt <= end)
    {
        it = newIt;
        return true;
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::advanceOfBytes(ssize_t bytesLength);
template bool StringIterator<StringIteratorUTF8>::advanceOfBytes(ssize_t bytesLength);
template bool StringIterator<StringIteratorUTF16>::advanceOfBytes(ssize_t bytesLength);

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceUntilMatchesAny(Span<const CodePoint> items, CodePoint& matched)
{
    while (it < end)
    {
        const auto decoded = CharIterator::decode(it, end);
        for (auto c : items)
        {
            if (decoded == c)
            {
                matched = c;
                return true;
            }
        }
        it = getNextOf(it, end);
    }
    return false;
}

// clang-format off
template bool StringIterator<StringIteratorASCII>::advanceUntilMatchesAny(Span<const CodePoint> items,CodePoint& matched);
template bool StringIterator<StringIteratorUTF8>::advanceUntilMatchesAny(Span<const CodePoint> items,CodePoint& matched);
template bool StringIterator<StringIteratorUTF16>::advanceUntilMatchesAny(Span<const CodePoint> items,CodePoint& matched);
// clang-format on

template <typename CharIterator>
bool StringIterator<CharIterator>::reverseAdvanceUntilMatchesAny(Span<const CodePoint> items, CodePoint& matched)
{
    while (it > start)
    {
        auto prevIt = it;
        it          = getPreviousOf(it, start);

        const auto decoded = CharIterator::decode(it, prevIt);
        for (auto c : items)
        {
            if (decoded == c)
            {
                matched = c;
                return true;
            }
        }
    }
    return false;
}

// clang-format off
template bool StringIterator<StringIteratorASCII>::reverseAdvanceUntilMatchesAny(Span<const CodePoint> items, CodePoint& matched);
template bool StringIterator<StringIteratorUTF8>::reverseAdvanceUntilMatchesAny(Span<const CodePoint> items, CodePoint& matched);
template bool StringIterator<StringIteratorUTF16>::reverseAdvanceUntilMatchesAny(Span<const CodePoint> items,CodePoint& matched);
// clang-format on

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceUntilDifferentFrom(CodePoint c, CodePoint* optionalReadChar)
{
    while (it < end)
    {
        auto readChar = CharIterator::decode(it, end);
        if (readChar != c)
        {
            if (optionalReadChar)
            {
                *optionalReadChar = readChar;
            }
            return true;
        }
        it = getNextOf(it, end);
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::advanceUntilDifferentFrom(CodePoint c, CodePoint* optionalReadChar);
template bool StringIterator<StringIteratorUTF8>::advanceUntilDifferentFrom(CodePoint c, CodePoint* optionalReadChar);
template bool StringIterator<StringIteratorUTF16>::advanceUntilDifferentFrom(CodePoint c, CodePoint* optionalReadChar);

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceBackwardIfMatches(CodePoint c)
{
    if (it > start)
    {
        auto otherIt = getPreviousOf(it, start);
        if (CharIterator::decode(otherIt, it) == c)
        {
            it = otherIt;
            return true;
        }
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::advanceBackwardIfMatches(CodePoint c);
template bool StringIterator<StringIteratorUTF8>::advanceBackwardIfMatches(CodePoint c);
template bool StringIterator<StringIteratorUTF16>::advanceBackwardIfMatches(CodePoint c);

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceIfMatchesAny(Span<const CodePoint> items)
{
    if (it < end)
    {
        const auto decoded = CharIterator::decode(it, end);
        for (auto c : items)
        {
            if (decoded == c)
            {
                it = getNextOf(it, end);
                return true;
            }
        }
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::advanceIfMatchesAny(Span<const CodePoint> items);
template bool StringIterator<StringIteratorUTF8>::advanceIfMatchesAny(Span<const CodePoint> items);
template bool StringIterator<StringIteratorUTF16>::advanceIfMatchesAny(Span<const CodePoint> items);

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceIfMatchesRange(CodePoint first, CodePoint last)
{
    SC_ASSERT_RELEASE(first <= last);
    if (it < end)
    {
        const auto decoded = CharIterator::decode(it, end);
        if (decoded >= first and decoded <= last)
        {
            it = getNextOf(it, end);
            return true;
        }
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::advanceIfMatchesRange(CodePoint first, CodePoint last);
template bool StringIterator<StringIteratorUTF8>::advanceIfMatchesRange(CodePoint first, CodePoint last);
template bool StringIterator<StringIteratorUTF16>::advanceIfMatchesRange(CodePoint first, CodePoint last);

template <typename CharIterator>
bool StringIterator<CharIterator>::read(CodePoint& c)
{
    if (it < end)
    {
        c = CharIterator::decode(it, end);
        return true;
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::read(CodePoint& c);
template bool StringIterator<StringIteratorUTF8>::read(CodePoint& c);
template bool StringIterator<StringIteratorUTF16>::read(CodePoint& c);

template <typename CharIterator>
bool StringIterator<CharIterator>::advanceBackwardRead(CodePoint& c)
{
    if (it > start)
    {
        auto prevIt = it;

        it = getPreviousOf(it, start);
        c  = CharIterator::decode(it, prevIt);
        return true;
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::advanceBackwardRead(CodePoint& c);
template bool StringIterator<StringIteratorUTF8>::advanceBackwardRead(CodePoint& c);
template bool StringIterator<StringIteratorUTF16>::advanceBackwardRead(CodePoint& c);

template <typename CharIterator>
bool StringIterator<CharIterator>::reverseAdvanceCodePoints(size_t numCodePoints)
{
    while (numCodePoints-- > 0)
    {
        if (it <= start)
        {
            return false;
        }
        it = getPreviousOf(it, start);
    }
    return true;
}
template bool StringIterator<StringIteratorASCII>::reverseAdvanceCodePoints(size_t numCodePoints);
template bool StringIterator<StringIteratorUTF8>::reverseAdvanceCodePoints(size_t numCodePoints);
template bool StringIterator<StringIteratorUTF16>::reverseAdvanceCodePoints(size_t numCodePoints);

template <typename CharIterator>
bool StringIterator<CharIterator>::endsWithAnyOf(Span<const CodePoint> codePoints) const
{
    if (start != end)
    {
        auto pointerToLast    = CharIterator::getPreviousOf(end, start);
        auto decodedCodePoint = CharIterator::decode(pointerToLast, end);
        for (CodePoint codePoint : codePoints)
        {
            if (codePoint == decodedCodePoint)
            {
                return true;
            }
        }
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::endsWithAnyOf(Span<const CodePoint> codePoints) const;
template bool StringIterator<StringIteratorUTF8>::endsWithAnyOf(Span<const CodePoint> codePoints) const;
template bool StringIterator<StringIteratorUTF16>::endsWithAnyOf(Span<const CodePoint> codePoints) const;

template <typename CharIterator>
bool StringIterator<CharIterator>::startsWithAnyOf(Span<const CodePoint> codePoints) const
{
    if (start != end)
    {
        auto decodedCodePoint = CharIterator::decode(start, end);
        for (CodePoint codePoint : codePoints)
        {
            if (codePoint == decodedCodePoint)
            {
                return true;
            }
        }
    }
    return false;
}
template bool StringIterator<StringIteratorASCII>::startsWithAnyOf(Span<const CodePoint> codePoints) const;
template bool StringIterator<StringIteratorUTF8>::startsWithAnyOf(Span<const CodePoint> codePoints) const;
template bool StringIterator<StringIteratorUTF16>::startsWithAnyOf(Span<const CodePoint> codePoints) const;

template <typename CharIterator>
template <typename IteratorType>
bool StringIterator<CharIterator>::endsWith(IteratorType other) const
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
template bool StringIterator<StringIteratorASCII>::endsWith(StringIteratorASCII other) const;
template bool StringIterator<StringIteratorASCII>::endsWith(StringIteratorUTF8 other) const;
template bool StringIterator<StringIteratorASCII>::endsWith(StringIteratorUTF16 other) const;
template bool StringIterator<StringIteratorUTF8>::endsWith(StringIteratorUTF8 other) const;
template bool StringIterator<StringIteratorUTF8>::endsWith(StringIteratorASCII other) const;
template bool StringIterator<StringIteratorUTF8>::endsWith(StringIteratorUTF16 other) const;
template bool StringIterator<StringIteratorUTF16>::endsWith(StringIteratorUTF16 other) const;
template bool StringIterator<StringIteratorUTF16>::endsWith(StringIteratorASCII other) const;
template bool StringIterator<StringIteratorUTF16>::endsWith(StringIteratorUTF8 other) const;

template <typename CharIterator>
template <typename IteratorType>
bool StringIterator<CharIterator>::startsWith(IteratorType other) const
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
template bool StringIterator<StringIteratorASCII>::startsWith(StringIteratorASCII other) const;
template bool StringIterator<StringIteratorASCII>::startsWith(StringIteratorUTF8 other) const;
template bool StringIterator<StringIteratorASCII>::startsWith(StringIteratorUTF16 other) const;
template bool StringIterator<StringIteratorUTF8>::startsWith(StringIteratorUTF8 other) const;
template bool StringIterator<StringIteratorUTF8>::startsWith(StringIteratorASCII other) const;
template bool StringIterator<StringIteratorUTF8>::startsWith(StringIteratorUTF16 other) const;
template bool StringIterator<StringIteratorUTF16>::startsWith(StringIteratorUTF16 other) const;
template bool StringIterator<StringIteratorUTF16>::startsWith(StringIteratorASCII other) const;
template bool StringIterator<StringIteratorUTF16>::startsWith(StringIteratorUTF8 other) const;

// StringIteratorASCII
bool StringIteratorASCII::advanceUntilMatchesNonConstexpr(CodePoint c)
{
    // clang-format off
    if (it == nullptr or c > 127) SC_LANGUAGE_UNLIKELY
    {
        it = end;
        return false;
    }
    // clang-format on
    char charC = static_cast<char>(c);
    auto res   = ::memchr(it, charC, static_cast<size_t>(end - it));
    if (res != nullptr)
        it = static_cast<const char*>(res);
    else
        it = end;
    return it < end;
}

// StringIteratorUTF16
const char* StringIteratorUTF16::getNextOf(const char* src, const char* end)
{
    const char* it = src;
    StringSpan::advanceUTF16(it, end);
    return it;
}

const char* StringIteratorUTF16::getPreviousOf(const char* bytes, const char* start)
{
    if (bytes - 2 < start)
        return nullptr;
    uint16_t value;
    ::memcpy(&value, bytes - 2, sizeof(uint16_t)); // Avoid potential unaligned read
    if (value >= 0xD800 and value <= 0xDFFF)       // TODO: This assumes Little endian
    {
        return bytes - 4 < start ? nullptr : bytes - 4; // Multi-byte character
    }
    else
    {
        return bytes - 2; // Single-byte character
    }
}

SC::uint32_t StringIteratorUTF16::decode(const char* bytes, const char* end)
{
    const char* srcCopy = bytes;
    return StringSpan::advanceUTF16(srcCopy, end);
}

// StringIteratorUTF8
const char* StringIteratorUTF8::getNextOf(const char* src, const char* end)
{
    const char* it = src;
    StringSpan::advanceUTF8(it, end);
    return it;
}

const char* StringIteratorUTF8::getPreviousOf(const char* src, const char* start)
{
    do
    {
        --src;
    } while (src >= start and (*src & 0xC0) == 0x80);
    return src;
}

SC::uint32_t StringIteratorUTF8::decode(const char* src, const char* end)
{
    const char* srcCopy = src;
    return StringSpan::advanceUTF8(srcCopy, end);
}
} // namespace SC
