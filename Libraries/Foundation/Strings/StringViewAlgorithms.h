// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringIterator.h"
#include "StringView.h"

namespace SC
{
struct StringAlgorithms
{
  private:
    template <typename Func>
    [[nodiscard]] static constexpr auto withIterator(StringView string, Func&& func)
    {
        switch (string.getEncoding())
        {
        case StringEncoding::Ascii: return func(string.getIterator<StringIteratorASCII>());
        case StringEncoding::Utf8: return func(string.getIterator<StringIteratorUTF8>());
        case StringEncoding::Utf16: return func(string.getIterator<StringIteratorUTF16>());
        }
        SC_UNREACHABLE();
    }

    template <typename Func>
    [[nodiscard]] static constexpr auto applyFunc(StringView s1, StringView s2, Func&& func)
    {
        return withIterator(s1, [&s2, &func](auto it1)
                            { return withIterator(s2, [&it1, &func](auto it2) { return func(it1, it2); }); });
    }

  public:
    [[nodiscard]] static bool matchWildcard(StringView s1, StringView s2)
    {
        return applyFunc(s1, s2, [](auto it1, auto it2) { return matchWildcardIterator(it1, it2); });
    }

    template <typename StringIterator1, typename StringIterator2>
    [[nodiscard]] static bool matchWildcardIterator(StringIterator1 pattern, StringIterator2 text)
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
};
} // namespace SC
