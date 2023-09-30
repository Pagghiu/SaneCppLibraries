// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Objects/Result.h"
#include "../Foundation/Strings/StringView.h"

namespace SC
{
struct JsonTokenizer;
} // namespace SC

struct SC::JsonTokenizer
{
    struct Token
    {
        enum Type
        {
            Invalid = 0,
            ObjectStart,
            ObjectEnd,
            ArrayStart,
            ArrayEnd,
            Colon,
            Comma,
            True,
            False,
            Null,
            String, // Unvalidated
            Number, // Unvalidated
        };
        constexpr Token() : type(Invalid) {}

        Type   type;
        size_t tokenStartBytes  = 0;
        size_t tokenLengthBytes = 0;

        constexpr StringView getToken(StringView source) const
        {
            SC_ASSERT_DEBUG(StringEncodingGetSize(source.getEncoding()) == 1);
            return source.sliceStartLengthBytes(tokenStartBytes, tokenLengthBytes); // Assuming it's utf8 or ASCII
        }
    };

    // Return false when iterator is at end
    [[nodiscard]] static constexpr bool tokenizeNext(StringIteratorASCII& it, Token& token)
    {
        if (skipWhitespaces(it))
        {
            return scanToken(it, token);
        }
        return false;
    }

    [[nodiscard]] static constexpr bool scanToken(StringIteratorASCII& it, Token& token)
    {
        StringIteratorASCII::CodePoint current = 0;
        const StringIteratorASCII      start   = it;
        if (not it.advanceRead(current))
        {
            return false;
        }
        switch (current)
        {
        case '{': token.type = Token::ObjectStart; break;
        case '}': token.type = Token::ObjectEnd; break;
        case '[': token.type = Token::ArrayStart; break;
        case ']': token.type = Token::ArrayEnd; break;
        case ':': token.type = Token::Colon; break;
        case ',': token.type = Token::Comma; break;
        case 't': tokenizeTrue(it, token); break;
        case 'f': tokenizeFalse(it, token); break;
        case 'n': tokenizeNull(it, token); break;
        case '"': tokenizeString(it, start, token); return true;
        default: tokenizeNumber(it, current, token); break;
        }
        token.tokenLengthBytes = static_cast<size_t>(it.bytesDistanceFrom(start));
        auto realStart         = start;
        realStart.setToStart();
        token.tokenStartBytes = static_cast<size_t>(start.bytesDistanceFrom(realStart));
        return true;
    }

  private:
    // Returns false when iterator is at end
    [[nodiscard]] static constexpr bool skipWhitespaces(StringIteratorASCII& it)
    {
        constexpr StringIteratorSkipTable table({'\t', '\n', '\r', ' '});
        StringIteratorASCII::CodePoint    current = 0;
        while (it.advanceRead(current))
        {
            if (not table.matches[current])
            {
                break;
            }
        }
        (void)it.stepBackward(); // put back the read character (if any)
        return not it.isAtEnd();
    }

    static constexpr void tokenizeString(StringIteratorASCII& it, const StringIteratorASCII start, Token& token)
    {
        while (it.advanceUntilMatches('"')) // find the end of the string
        {
            if (it.isPrecededBy('\\')) // if the quote is escaped continue search
                continue;
            StringIteratorASCII startNext = start;
            (void)startNext.stepForward();  // but let's slice away leading '"'
            token.type     = Token::String; // Ok we have an (unvalidated) string
            auto realStart = start;
            realStart.setToStart();
            token.tokenStartBytes  = static_cast<size_t>(startNext.bytesDistanceFrom(realStart));
            token.tokenLengthBytes = static_cast<size_t>(it.bytesDistanceFrom(startNext));
            (void)it.advanceCodePoints(1); // eat the ending \" in the iterator
            break;
        }
    }
    static constexpr void tokenizeNumber(StringIteratorASCII& it, StringIteratorASCII::CodePoint previousChar,
                                         Token& token)
    {
        // eat all non whitespaces that could possibly form a number (to be validated, as it may contain multiple dots)
        constexpr StringIteratorSkipTable numbersTable({'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.'});

        if (not numbersTable.matches[previousChar])
        {
            return;
        }
        StringIteratorASCII::CodePoint current = 0;
        while (it.advanceRead(current))
        {
            if (not numbersTable.matches[current])
            {
                // not dot and not number
                (void)it.stepBackward(); // put it back
                break;
            }
        }

        token.type = Token::Number;
    }

    static constexpr void tokenizeTrue(StringIteratorASCII& it, Token& token)
    {
        if (it.advanceIfMatches('r') and it.advanceIfMatches('u') and it.advanceIfMatches('e'))
        {
            token.type = Token::True;
        }
    }

    static constexpr void tokenizeFalse(StringIteratorASCII& it, Token& token)
    {
        if (it.advanceIfMatches('a') and it.advanceIfMatches('l') and it.advanceIfMatches('s') and
            it.advanceIfMatches('e'))
        {
            token.type = Token::False;
        }
    }

    static constexpr void tokenizeNull(StringIteratorASCII& it, Token& token)
    {
        if (it.advanceIfMatches('u') and it.advanceIfMatches('l') and it.advanceIfMatches('l'))
        {
            token.type = Token::Null;
        }
    }
};
