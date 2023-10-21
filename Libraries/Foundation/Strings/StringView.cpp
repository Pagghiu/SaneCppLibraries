// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringView.h"

#include <stdlib.h> //atoi
#include <string.h> //strlen

SC::StringView SC::StringView::fromNullTerminated(const char* text, StringEncoding encoding)
{
    return StringView(text, ::strlen(text), true, encoding);
}

bool SC::StringView::parseInt32(int32_t& value) const
{
    if (getEncoding() != StringEncoding::Ascii and getEncoding() != StringEncoding::Utf8)
    {
        return false;
    }
    char buffer[12]; // 10 digits + sign + nullterm
    if (textSizeInBytes >= sizeof(buffer))
        return false;

    if (hasNullTerm)
    {
        value = atoi(text);
    }
    else
    {
        memcpy(buffer, text, textSizeInBytes);
        buffer[textSizeInBytes] = 0;

        value = atoi(buffer);
    }
    if (value == 0)
    {
        // atoi returns 0 on failed parsing...
        StringIteratorASCII it = getIterator<StringIteratorASCII>();
        (void)it.advanceIfMatchesAny({'-', '+'}); // optional
        if (it.isAtEnd())
        {
            return false;
        }
        (void)it.advanceUntilDifferentFrom('0'); // any number of 0s
        return it.isAtEnd();
    }
    return true;
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
    if (hasNullTerm)
    {
        value = atof(text);
    }
    else
    {
        char         buffer[255];
        const size_t bufferSize = min(textSizeInBytes, static_cast<decltype(textSizeInBytes)>(sizeof(buffer) - 1));
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
            const StringView ours(text, str.textSizeInBytes, false, getEncoding());
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
            const StringView ours(text + textSizeInBytes - str.textSizeInBytes, str.textSizeInBytes, false,
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

bool SC::StringView::containsChar(StringCodePoint c) const
{
    return withIterator([c](auto it) { return it.advanceUntilMatches(c); });
}

bool SC::StringView::endsWithChar(StringCodePoint c) const
{
    return withIterator([c](auto it) { return it.endsWithChar(c); });
}

bool SC::StringView::startsWithChar(StringCodePoint c) const
{
    return withIterator([c](auto it) { return it.startsWithChar(c); });
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
            return StringView(startIt.getCurrentIt(), distance,
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
            return StringView(startIt.getCurrentIt(), distance,
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
            return StringView(startIt.getCurrentIt(), distance,
                              hasNullTerm and (offset + distance == sizeInBytesIncludingTerminator()), getEncoding());
        });
}

SC::StringView SC::StringView::trimEndingChar(StringCodePoint c) const
{
    auto sv = *this;
    while (sv.endsWithChar(c))
    {
        sv = sv.sliceEnd(1);
    }
    return sv;
}

SC::StringView SC::StringView::trimStartingChar(StringCodePoint c) const
{
    auto sv = *this;
    while (sv.startsWithChar(c))
    {
        sv = sv.sliceStart(1);
    }
    return sv;
}

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
[[nodiscard]] bool SC::StringViewTokenizer::isFinished() const { return current.isEmpty(); }

bool SC::StringViewTokenizer::tokenizeNext(Span<const StringCodePoint> separators, Options options)
{
    if (isFinished())
    {
        return false;
    }
    auto oldNonEmpty = numSplitsNonEmpty;
    current.withIterator(
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
                current = StringView::fromIteratorUntilEnd(iterator);
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
            } while (not current.isEmpty());
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
