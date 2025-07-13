// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h" //Assert::unreachable
#include "../Foundation/Span.h"
#include "../Foundation/StringSpan.h"

namespace SC
{
//! @addtogroup group_strings
//! @{

/// @brief UTF code point (32 bit)
using StringCodePoint = uint32_t;

/// @brief Checks if two encodings have the same utf unit size
/// @param encoding1 First encoding
/// @param encoding2 Second encoding
/// @return `true` if the two encodings have the same unit size
constexpr bool StringEncodingAreBinaryCompatible(StringEncoding encoding1, StringEncoding encoding2)
{
    return (encoding1 == encoding2) or (encoding2 == StringEncoding::Ascii and encoding1 == StringEncoding::Utf8) or
           (encoding2 == StringEncoding::Utf8 and encoding1 == StringEncoding::Ascii);
}

/// @brief A position inside a fixed range `[start, end)` of UTF code points.
///
/// It's a range of bytes (start and end pointers) with a *current* pointer pointing at a specific code point of the
/// range. There are three classes derived from it (SC::StringIteratorASCII, SC::StringIteratorUTF8 and
/// SC::StringIteratorUTF16) and they allow doing operations along the string view in UTF code points.
/// @note Code points are not the same as perceived characters (that would be grapheme clusters).
/// Invariants: start <= end and it >= start and it <= end.
/// @tparam CharIterator StringIteratorASCII, StringIteratorUTF8 or StringIteratorUTF16
template <typename CharIterator>
struct SC_COMPILER_EXPORT StringIterator
{
    static constexpr StringEncoding getEncoding() { return CharIterator::getEncoding(); }

    using CodeUnit  = char;
    using CodePoint = StringCodePoint;

    /// @brief Rewind current position to start of iterator range
    constexpr void setToStart() { it = start; }

    /// @brief Set current position to end of iterator range
    constexpr void setToEnd() { it = end; }

    /// @brief Check if current position is at end of iterator range
    /// @return `true` if position is at end of iterator range
    [[nodiscard]] constexpr bool isAtEnd() const { return it >= end; }

    /// @brief Check if current position is at start of iterator range
    /// @return `true` if position is at start of iterator range
    [[nodiscard]] constexpr bool isAtStart() const { return it <= start; }

    /// @brief Advances position towards `end` until it matches CodePoint `c` or `position == end`
    /// @param c The CodePoint to be searched
    /// @return `true` if c was found, `false` if end was reached
    [[nodiscard]] constexpr bool advanceUntilMatches(CodePoint c);

    /// @brief Moves position towards start until CodePoint `c` is found or `position == end`
    /// @param c The CodePoint to be searched
    /// @return `true` if `c` was found, `false` if `start` was reached
    [[nodiscard]] bool reverseAdvanceUntilMatches(CodePoint c);

    /// @brief Advances position towards `end` until a matching range of character equal to `other[it, end)` is found.
    /// Position pointer is advanced additional after the matching range.
    /// @param other The range of character to be found `[it, end)`
    /// @return `true` if other was found, `false` if end was reached
    [[nodiscard]] bool advanceAfterFinding(StringIterator other);

    /// @brief Advances position towards `end` until a matching range of character equal to `other[it, end)` is found.
    /// Position pointer is stopped before the first matching character of the range.
    /// @param other The range of character to be found `[it, end)`
    /// @return `true` if other was found, `false` if end was reached
    [[nodiscard]] bool advanceBeforeFinding(StringIterator other);

    /// @brief Advances position by the same number of code points as other
    /// @param other The other range of character
    /// @return `true` if advance succeeded, `false` if `end` was reached
    [[nodiscard]] bool advanceByLengthOf(StringIterator other) { return advanceOfBytes(other.end - other.it); }

    /// @brief Advances position until any CodePoint in the given Span is found
    /// @param items A contiguous span of CodePoint to match
    /// @param matched The matched CodePoint in the Span
    /// @return `true` if one CodePoint was matched, `false` if `end` was reached
    [[nodiscard]] bool advanceUntilMatchesAny(Span<const CodePoint> items, CodePoint& matched);

    /// @brief Moves position towards start until any CodePoint in the given Span is found
    /// @param items A contiguous span of CodePoint to match
    /// @param matched The matched CodePoint in the Span
    /// @return `true` if one CodePoint was matched, `false` if `start` was reached
    [[nodiscard]] bool reverseAdvanceUntilMatchesAny(Span<const CodePoint> items, CodePoint& matched);

    /// @brief Advances position until a code point different from `c` is found or `end` is reached
    /// @param c The CodePoint to be compared
    /// @param optionalReadChar The CodePoint that was found, different from `c`
    /// @return `true` if it finds at least one code point different from c, `false` if iterator ends before finding it
    [[nodiscard]] bool advanceUntilDifferentFrom(CodePoint c, CodePoint* optionalReadChar = nullptr);

    /// @brief Advance position only if next code point matches `c`.
    /// @param c The CodePoint being searched
    /// @return `true` if next code point matches `c`, `false` otherwise or if position is already at end
    [[nodiscard]] constexpr bool advanceIfMatches(CodePoint c);

    /// @brief Move position by one code point towards start if previous code point matches `c`
    /// @param c The CodePoint being searched
    /// @return `true` if next code point matches `c`, `false` otherwise or if position is already at end
    [[nodiscard]] bool advanceBackwardIfMatches(CodePoint c);

    /// @brief Advance position only if any of the code points in given Span is matched
    /// @param items Span of points to be checked
    /// @return `true` if next code point was successfully matched
    [[nodiscard]] bool advanceIfMatchesAny(Span<const CodePoint> items);

    /// @brief Advance position if any code point in the range [first, last] is matched
    /// @param first The initial CodePoint defining the range to be checked
    /// @param last The final CodePoint defining the range to be checked
    /// @return `true` if a code point in the given range was matched
    [[nodiscard]] bool advanceIfMatchesRange(CodePoint first, CodePoint last);

    /// @brief Check if code unit at current position matches CodePoint `c`
    /// @param c code point to match
    /// @return `true` if code unit at current position matches `c`, `false` if there is no match or position is at
    /// `end`
    [[nodiscard]] bool match(CodePoint c) { return it < end and CharIterator::decode(it) == c; }

    /// @brief Decode code unit at current position and advance
    /// @param c output code point read
    /// @return `true` if position is at `end` before decoding code unit
    [[nodiscard]] constexpr bool advanceRead(CodePoint& c);

    /// @brief Read code unit at current position
    /// @param c output code point read
    /// @return `true` if position is at `end` before decoding code unit
    [[nodiscard]] bool read(CodePoint& c);

    /// @brief Move to previous position and read code unit
    /// @param c output code point read
    /// @return `true` if position is at `start` before decoding code unit
    [[nodiscard]] bool advanceBackwardRead(CodePoint& c);

    /// @brief Move position to next code point
    /// @return `true` if position is not at `end` before trying to move forward
    [[nodiscard]] constexpr bool stepForward();

    /// @brief Move position to previous code point
    /// @return `true` if position is not at `start` before trying to move backwards
    [[nodiscard]] constexpr bool stepBackward();

    /// @brief Move position forward (towards `end`) by variable number of code points
    /// @param numCodePoints number of code points to move forward
    /// @return `true` if it's possible advancing `numCodePoints` before reaching `end`
    [[nodiscard]] constexpr bool advanceCodePoints(size_t numCodePoints);

    /// @brief Move position backwards (towards `start`) by variable number of code pints
    /// @param numCodePoints number of code points to move backwards
    /// @return `true` if it's possible moving `numCodePoints` backwards before reaching `start`
    [[nodiscard]] bool reverseAdvanceCodePoints(size_t numCodePoints);

    /// @brief Check if next code point is `c`
    /// @param c the code point
    /// @return `true` if next code point is `c`
    [[nodiscard]] constexpr bool isFollowedBy(CodePoint c);

    /// @brief Check if previous code point is `c`
    /// @param c the code point
    /// @return `true` if previous code point is `c`
    [[nodiscard]] constexpr bool isPrecededBy(CodePoint c);

    /// @brief Returns another StringIterator range, starting from `start` to `otherPoint` position
    /// @param otherPoint The StringIterator containing the ending position to slice to
    /// @return The new StringIterator `[start, otherPoint.position]`
    [[nodiscard]] constexpr StringIterator sliceFromStartUntil(StringIterator otherPoint) const;

    /// @brief Get distance in bytes from current position to another StringIterator current position
    /// @param other The StringIterator from which to compute distance
    /// @return (signed) number of bytes between the two StringIterator
    [[nodiscard]] constexpr ssize_t bytesDistanceFrom(StringIterator other) const;

    /// @brief Check if this Iterator ends with any code point in the given span
    /// @param codePoints A span of code points to check for
    /// @return `true` if at least one code point of `codepoints` exists at the end of the range
    [[nodiscard]] bool endsWithAnyOf(Span<const CodePoint> codePoints) const;

    /// @brief Check if this Iterator starts with any code point in the given span
    /// @param codePoints A span of code points to check for
    /// @return `true` if at least one code point of `codepoints` exists at the start of the range
    [[nodiscard]] bool startsWithAnyOf(Span<const CodePoint> codePoints) const;

    /// @brief Check if this Iterator at its end matches entirely another Iterator's range
    /// @param other  The other iterator to match
    /// @return `true` if this Iterator matches entire `other` Iterator at its `end`
    template <typename IteratorType>
    [[nodiscard]] bool endsWith(IteratorType other) const;

    /// @brief Check if this Iterator at its start matches entirely another Iterator's range
    /// @param other  The other iterator to match
    /// @return `true` if this Iterator matches entire `other` at its `start`
    template <typename IteratorType>
    [[nodiscard]] bool startsWith(IteratorType other) const;

  protected:
    [[nodiscard]] bool advanceOfBytes(ssize_t bytesLength);

    friend struct StringView;
    static constexpr const CodeUnit* getNextOf(const CodeUnit* src) { return CharIterator::getNextOf(src); }
    static constexpr const CodeUnit* getPreviousOf(const CodeUnit* src) { return CharIterator::getPreviousOf(src); }
    constexpr StringIterator(const CodeUnit* it, const CodeUnit* end) : it(it), start(it), end(end) {}
    const CodeUnit* it;
    const CodeUnit* start;
    const CodeUnit* end;
};

/// @brief A string iterator for ASCII strings
struct SC_COMPILER_EXPORT StringIteratorASCII : public StringIterator<StringIteratorASCII>
{
    [[nodiscard]] constexpr bool advanceUntilMatches(CodePoint c);

  private:
    [[nodiscard]] bool advanceUntilMatchesNonConstexpr(CodePoint c);
    using StringIterator::StringIterator;
    constexpr StringIteratorASCII(const CodeUnit* it, const CodeUnit* end) : StringIterator(it, end) {}
    using Parent = StringIterator<StringIteratorASCII>;
    friend Parent;
    friend struct StringView;

    [[nodiscard]] static constexpr StringEncoding getEncoding() { return StringEncoding::Ascii; }

    [[nodiscard]] static constexpr const char* getNextOf(const char* src) { return src + 1; }
    [[nodiscard]] static constexpr const char* getPreviousOf(const char* src) { return src - 1; }
    [[nodiscard]] static constexpr CodePoint   decode(const char* src) { return static_cast<CodePoint>(*src); }
};

/// @brief A string iterator for UTF16 strings
struct SC_COMPILER_EXPORT StringIteratorUTF16 : public StringIterator<StringIteratorUTF16>
{
  private:
    using StringIterator::StringIterator;
    constexpr StringIteratorUTF16(const CodeUnit* it, const CodeUnit* end) : StringIterator(it, end) {}
    using Parent = StringIterator<StringIteratorUTF16>;
    friend Parent;
    friend struct StringView;

    [[nodiscard]] static StringEncoding getEncoding() { return StringEncoding::Utf16; }

    [[nodiscard]] static const char* getNextOf(const char* bytes);

    [[nodiscard]] static const char* getPreviousOf(const char* bytes);

    [[nodiscard]] static uint32_t decode(const char* bytes);
};

/// @brief A string iterator for UTF8 strings
struct SC_COMPILER_EXPORT StringIteratorUTF8 : public StringIterator<StringIteratorUTF8>
{
  private:
    using Parent = StringIterator<StringIteratorUTF8>;
    friend Parent;
    friend struct StringView;
    using StringIterator::StringIterator;
    constexpr StringIteratorUTF8(const CodeUnit* it, const CodeUnit* end) : StringIterator(it, end) {}

    [[nodiscard]] static StringEncoding getEncoding() { return StringEncoding::Utf8; }

    [[nodiscard]] static const char* getNextOf(const char* src);

    [[nodiscard]] static const char* getPreviousOf(const char* src);

    [[nodiscard]] static uint32_t decode(const char* src);
};

/// @brief Builds a constexpr bool skip table of 256 entries used in some parsers
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
//! @}

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------
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
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
    return __builtin_is_constant_evaluated() ? StringIterator::advanceUntilMatches(c)
                                             : advanceUntilMatchesNonConstexpr(c);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

} // namespace SC
