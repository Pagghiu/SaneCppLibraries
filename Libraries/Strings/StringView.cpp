// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Strings/StringView.h"

#include <errno.h>  // errno
#include <stdint.h> // INT32_MIN/MAX
#include <stdlib.h> //atoi
#include <string.h> //strlen
SC::StringView SC::StringView::fromNullTerminated(const char* text, StringEncoding encoding)
{
    if (text == nullptr)
    {
        return StringView({nullptr, 0}, false, encoding);
    }
    else
    {
        return StringView({text, ::strlen(text)}, true, encoding);
    }
}

#if SC_PLATFORM_WINDOWS
SC::StringView SC::StringView::fromNullTerminated(const wchar_t* text, StringEncoding)
{
    return StringView({text, ::wcslen(text)}, true);
}
#endif

bool SC::StringView::parseInt32(int32_t& value) const
{
    if (text == nullptr)
        return false;
    char buffer[12]; // 10 digits + sign + nullTerm
    switch (getEncoding())
    {
    case StringEncoding::Utf16: {
        StringIteratorUTF16 it(getIterator<StringIteratorUTF16>());
        StringCodePoint     codePoint;
        if (not it.advanceRead(codePoint))
            return false;

        if ((codePoint < '0' or codePoint > '9') and (codePoint != '-' and codePoint != '+'))
            return false;
        int index       = 0;
        buffer[index++] = static_cast<char>(codePoint);

        while (it.advanceRead(codePoint))
        {
            if (codePoint < '0' or codePoint > '9')
                return false;
            buffer[index++] = static_cast<char>(codePoint);
        }
        buffer[index] = 0;

        return StringView({buffer, sizeof(buffer) - 1}, true, StringEncoding::Ascii).parseInt32(value);
    }
    break;

    default: {
        const auto* parseText = text;
        if (!hasNullTerm)
        {
            if (textSizeInBytes >= sizeof(buffer))
                return false;
            memcpy(buffer, textWide, textSizeInBytes);
            buffer[textSizeInBytes] = 0;
            parseText               = buffer;
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
    }
    break;
    }

    return false;
}

bool SC::StringView::parseFloat(float& value) const
{
    double dValue;
    if (parseDouble(dValue))
    {
        value = static_cast<float>(dValue);
        return true;
    }
    return false;
}

bool SC::StringView::parseDouble(double& value) const
{
    if (text == nullptr)
        return false;
    if (hasNullTerm)
    {
        value = atof(text);
    }
    else
    {
        char         buffer[255];
        const size_t bufferSize = min(textSizeInBytes, static_cast<decltype(textSizeInBytes)>(sizeof(buffer) - 1));
        if (text != nullptr)
            memcpy(buffer, text, bufferSize);
        buffer[bufferSize] = 0;
        value              = atof(buffer);
    }
    if (value == 0.0f)
    {
        // atof returns 0 on failed parsing...
        // TODO: Handle float scientific notation
        StringIteratorASCII it = getIterator<StringIteratorASCII>();
        (void)it.advanceIfMatchesAny({'-', '+'}); // optional
        if (it.isAtEnd())                         // we now require something
        {
            return false;
        }
        if (it.advanceIfMatches('.')) // optional
        {
            if (it.isAtEnd()) // but if it exists now we need at least a number
            {
                return false;
            }
        }
        (void)it.advanceUntilDifferentFrom('0'); // any number of 0s
        return it.isAtEnd();                     // if they where all zeroes
    }
    return true;
}

SC::StringView::Comparison SC::StringView::compare(StringView other) const
{
    if (hasCompatibleEncoding(other))
    {
        const int res = memcmp(text, other.text, min(textSizeInBytes, other.textSizeInBytes));
        if (res < 0)
            return Comparison::Smaller;
        else if (res == 0)
            return Comparison::Equals;
        else
            return Comparison::Bigger;
    }
    else
    {
        return withIterator(
            [other](auto it1)
            {
                return other.withIterator(
                    [&it1](auto it2)
                    {
                        StringCodePoint c1 = 0, c2 = 0;
                        while (it1.advanceRead(c1) and it2.advanceRead(c2))
                        {
                            if (c1 < c2)
                            {
                                return Comparison::Smaller;
                            }
                            else if (c1 > c2)
                            {
                                return Comparison::Bigger;
                            }
                        }
                        if (it1.isAtEnd() and it2.isAtEnd())
                        {
                            return Comparison::Equals;
                        }
                        if (it1.isAtEnd())
                        {
                            return Comparison::Bigger;
                        }
                        return Comparison::Smaller;
                    });
            });
    }
}

bool SC::StringView::startsWith(const StringView str) const
{
    if (hasCompatibleEncoding(str))
    {
        if (str.textSizeInBytes <= textSizeInBytes)
        {
            const StringView ours({text, str.textSizeInBytes}, false, getEncoding());
            return str == ours;
        }
        return false;
    }
    return withIterator([str](auto it1) { return str.withIterator([it1](auto it2) { return it1.startsWith(it2); }); });
}

bool SC::StringView::endsWith(const StringView str) const
{
    if (hasCompatibleEncoding(str))
    {
        if (str.sizeInBytes() <= sizeInBytes())
        {
            const StringView ours({text + textSizeInBytes - str.textSizeInBytes, str.textSizeInBytes}, false,
                                  getEncoding());
            return str == ours;
        }
        return false;
    }
    return withIterator([str](auto it1) { return str.withIterator([it1](auto it2) { return it1.endsWith(it2); }); });
}

bool SC::StringView::containsString(const StringView str) const
{
    SC_ASSERT_RELEASE(hasCompatibleEncoding(str));
    return withIterator([str](auto it) { return it.advanceAfterFinding(str.getIterator<decltype(it)>()); });
}

bool SC::StringView::splitAfter(const StringView stringToMatch, StringView& remainingAfterSplit) const
{
    SC_ASSERT_RELEASE(hasCompatibleEncoding(stringToMatch));
    return withIterator(
        [&](auto it)
        {
            if (it.advanceAfterFinding(stringToMatch.getIterator<decltype(it)>()))
            {
                remainingAfterSplit = StringView::fromIteratorUntilEnd(it);
                return true;
            }
            return false;
        });
}

bool SC::StringView::containsCodePoint(StringCodePoint c) const
{
    return withIterator([c](auto it) { return it.advanceUntilMatches(c); });
}

bool SC::StringView::endsWithAnyOf(Span<const StringCodePoint> codePoints) const
{
    return withIterator([codePoints](auto it) { return it.endsWithAnyOf(codePoints); });
}

bool SC::StringView::startsWithAnyOf(Span<const StringCodePoint> codePoints) const
{
    return withIterator([codePoints](auto it) { return it.startsWithAnyOf(codePoints); });
}

SC::StringView SC::StringView::sliceStartEnd(size_t start, size_t end) const
{
    return withIterator(
        [&](auto it)
        {
            SC_ASSERT_RELEASE(it.advanceCodePoints(start));
            auto startIt = it;
            SC_ASSERT_RELEASE(start <= end && it.advanceCodePoints(end - start));
            const size_t distance = static_cast<size_t>(it.bytesDistanceFrom(startIt));
            return StringView({startIt.getCurrentIt(), distance},
                              hasNullTerm and (start + distance == sizeInBytesIncludingTerminator()), getEncoding());
        });
}

SC::StringView SC::StringView::sliceStart(size_t offset) const
{
    return withIterator(
        [&](auto it)
        {
            SC_ASSERT_RELEASE(it.advanceCodePoints(offset));
            auto startIt = it;
            it.setToEnd();
            const size_t distance = static_cast<size_t>(it.bytesDistanceFrom(startIt));
            return StringView({startIt.getCurrentIt(), distance},
                              hasNullTerm and (offset + distance == sizeInBytesIncludingTerminator()), getEncoding());
        });
}

SC::StringView SC::StringView::sliceEnd(size_t offset) const
{
    return withIterator(
        [&](auto it)
        {
            auto startIt = it;
            it.setToEnd();
            SC_ASSERT_RELEASE(it.reverseAdvanceCodePoints(offset));
            const size_t distance = static_cast<size_t>(it.bytesDistanceFrom(startIt));
            return StringView({startIt.getCurrentIt(), distance},
                              hasNullTerm and (offset + distance == sizeInBytesIncludingTerminator()), getEncoding());
        });
}

SC::StringView SC::StringView::trimEndAnyOf(Span<const StringCodePoint> codePoints) const
{
    auto sv = *this;
    while (sv.endsWithAnyOf(codePoints))
    {
        sv = sv.sliceEnd(1);
    }
    return sv;
}

SC::StringView SC::StringView::trimStartAnyOf(Span<const StringCodePoint> codePoints) const
{
    auto sv = *this;
    while (sv.startsWithAnyOf(codePoints))
    {
        sv = sv.sliceStart(1);
    }
    return sv;
}

SC::StringView SC::StringView::trimAnyOf(Span<const StringCodePoint> codePoints) const
{
    return trimEndAnyOf(codePoints).trimStartAnyOf(codePoints);
}

SC::StringView SC::StringView::trimWhiteSpaces() const { return trimAnyOf({'\r', '\n', '\t', ' '}); }

SC::StringView SC::StringView::sliceStartLength(size_t start, size_t length) const
{
    return sliceStartEnd(start, start + length);
}

bool SC::StringView::isIntegerNumber() const
{
    return withIterator(
        [&](auto it)
        {
            (void)it.advanceIfMatchesAny({'-', '+'}); // optional
            bool matchedAtLeastOneDigit = false;
            while (it.advanceIfMatchesRange('0', '9'))
                matchedAtLeastOneDigit = true;
            return matchedAtLeastOneDigit and it.isAtEnd();
        });
}

bool SC::StringView::isFloatingNumber() const
{
    // TODO: Floating point exponential notation
    return withIterator(
        [&](auto it)
        {
            (void)it.advanceIfMatchesAny({'-', '+'}); // optional
            bool matchedAtLeastOneDigit = false;
            while (it.advanceIfMatchesRange('0', '9'))
                matchedAtLeastOneDigit = true;
            if (it.advanceIfMatches('.')) // optional
                while (it.advanceIfMatchesRange('0', '9'))
                    matchedAtLeastOneDigit = true;
            return matchedAtLeastOneDigit and it.isAtEnd();
        });
}

//-----------------------------------------------------------------------------------------------------------------------
// StringViewTokenizer
//-----------------------------------------------------------------------------------------------------------------------
[[nodiscard]] bool SC::StringViewTokenizer::isFinished() const { return remaining.isEmpty(); }

bool SC::StringViewTokenizer::tokenizeNext(Span<const StringCodePoint> separators, Options options)
{
    if (isFinished())
    {
        return false;
    }
    auto oldNonEmpty = numSplitsNonEmpty;
    remaining.withIterator(
        [&](auto iterator)
        {
            auto originalTextStart = originalText.getIterator<decltype(iterator)>();
            do
            {
                auto componentStart = iterator;
                (void)iterator.advanceUntilMatchesAny(separators, splittingCharacter);
                component = StringView::fromIterators(componentStart, iterator);
                processed = StringView::fromIterators(originalTextStart, iterator);
                (void)iterator.stepForward();
                remaining = StringView::fromIteratorUntilEnd(iterator);
                numSplitsTotal++;
                if (not component.isEmpty())
                {
                    numSplitsNonEmpty++;
                    break;
                }
                else if (options == IncludeEmpty)
                {
                    break;
                }
            } while (not remaining.isEmpty());
        });
    return options == IncludeEmpty ? true : numSplitsNonEmpty > oldNonEmpty;
}

SC::StringViewTokenizer& SC::StringViewTokenizer::countTokens(Span<const StringCodePoint> separators)
{
    while (tokenizeNext(separators, SkipEmpty)) {}
    return *this;
}
//-----------------------------------------------------------------------------------------------------------------------
// StringViewAlgorithms
//-----------------------------------------------------------------------------------------------------------------------

bool SC::StringAlgorithms::matchWildcard(StringView s1, StringView s2)
{
    return StringView::withIterators(s1, s2, [](auto it1, auto it2) { return matchWildcardIterator(it1, it2); });
}

template <typename StringIterator1, typename StringIterator2>
bool SC::StringAlgorithms::matchWildcardIterator(StringIterator1 pattern, StringIterator2 text)
{
    typename decltype(pattern)::CodePoint patternChar = 0;
    typename decltype(text)::CodePoint    textChar    = 0;
    StringIterator1                       lastPattern = pattern;
    StringIterator2                       lastText    = text;
    if (not pattern.read(patternChar))
    {
        // If pattern is empty, only match with an empty text
        return text.isAtEnd();
    }
    while (text.advanceRead(textChar))
    {
        if (patternChar == '*')
        {
            // Skip consecutive asterisks
            if (not pattern.advanceUntilDifferentFrom('*', &patternChar))
            {
                // but if pattern ends with asterisk, it matches everything
                return true;
            }
            lastPattern = pattern;
            lastText    = text;
            (void)lastText.stepForward();
        }
        else if (patternChar == '?' or patternChar == textChar)
        {
            (void)pattern.stepForward();
            (void)pattern.read(patternChar);
        }
        else if (not lastPattern.isAtStart())
        {
            pattern = lastPattern;
            text    = lastText;
            (void)pattern.read(patternChar);
            (void)lastText.stepForward();
        }
        else
        {
            return false;
        }
    }
    // Discard any trailing * and if pattern is at end
    if (pattern.advanceUntilDifferentFrom('*'))
    {
        // there are some more chars in the pattern that are unmatched
        return false;
    }
    else
    {
        // pattern is now at end and fully matched
        return true;
    }
};
