// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/StringView.h"

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
        size_t tokenStartBytes = 0;
        size_t tokenLength     = 0;

        constexpr StringView getToken(StringView source) const
        {
            SC_DEBUG_ASSERT(StringEncodingGetSize(source.getEncoding()) == 1);
            return source.sliceStartLength(tokenStartBytes, tokenLength); // Assuming it's utf8 or ASCII
        }
    };

    // Return false when iterator is at end
    [[nodiscard]] static constexpr bool tokenizeNext(StringIteratorASCII& it, Token& token, size_t tokenStartBytes)
    {
        if (skipWhitespaces(it))
        {
            token.tokenStartBytes = tokenStartBytes;
            return scanToken(it, token);
        }
        return false;
    }

    [[nodiscard]] static constexpr bool scanToken(StringIteratorASCII& it, Token& token)
    {
        char                      current = 0;
        const StringIteratorASCII start   = it;
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
        default: tokenizeNumber(it, token); break;
        }
        token.tokenLength = static_cast<size_t>(it.bytesDistanceFrom(start));
        return true;
    }

  private:
    struct WhitespaceTable
    {
        bool isWhitespace[256] = {false};
        constexpr WhitespaceTable()
        {
            isWhitespace['\t'] = true;
            isWhitespace['\n'] = true;
            isWhitespace['\r'] = true;
            isWhitespace[' ']  = true;
        }
    };

    // Returns false when iterator is at end
    [[nodiscard]] static constexpr bool skipWhitespaces(StringIteratorASCII& it)
    {
        constexpr WhitespaceTable table;
        char                      current = 0;
        while (it.advanceRead(current))
        {
            if (not table.isWhitespace[current])
            {
                break;
            }
        }
        (void)it.stepBackward(); // put back the read character (if any)
        return not it.isEmpty();
    }

    static constexpr void tokenizeString(StringIteratorASCII& it, const StringIteratorASCII start, Token& token)
    {
        while (it.advanceUntilMatches('"')) // find the end of the string
        {
            if (it.isPrecededBy('\\')) // if the quote is escaped continue search
                continue;
            StringIteratorASCII startNext = start;
            (void)startNext.stepForward(); // but let's slice away leading '"'
            token.type = Token::String;    // Ok we have an (unvalidated) string
            token.tokenStartBytes += static_cast<size_t>(startNext.bytesDistanceFrom(start));
            token.tokenLength = static_cast<size_t>(it.bytesDistanceFrom(startNext));
            break;
        }
    }
    static constexpr void tokenizeNumber(StringIteratorASCII& it, Token& token)
    {
        // eat all non whitespaces that could possibly form a number (to be validated)
        constexpr WhitespaceTable table;

        char current                 = 0;
        bool atLeastOneNonWhitespace = false;
        while (it.advanceRead(current))
        {
            if (table.isWhitespace[current])
                break;
            atLeastOneNonWhitespace = true;
        }
        if (atLeastOneNonWhitespace)
            token.type = Token::Number; // Ok we have an (unvalidated) number
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
