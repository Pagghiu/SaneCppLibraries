// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Strings/StringView.h"

namespace SC
{
struct JsonTokenizer;
struct JsonTokenizerTest;
} // namespace SC

/// @brief Tokenize a JSON text stream, without validating numbers and strings
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
            String, // Not validated
            Number, // Not validated
        };
        /// @brief Constructs an invalid Token
        constexpr Token() : type(Invalid) {}

        /// @brief Get Token::Type
        /// @return Token::Type
        constexpr Type getType() const { return type; }

        /// @brief Get current Token as StringView slice from the passed string
        /// @param source The StringView that has been used (must be UTF8 or ASCII)
        /// @return StringView slice of `source` representing this token
        constexpr StringView getToken(StringView source) const
        {
            SC_ASSERT_DEBUG(StringEncodingGetSize(source.getEncoding()) == 1);
            return source.sliceStartLengthBytes(tokenStartBytes, tokenLengthBytes); // Assuming it's utf8 or ASCII
        }

      private:
        friend struct JsonTokenizer;
        size_t tokenStartBytes  = 0;
        size_t tokenLengthBytes = 0;
        Type   type;
    };

    // Return

    /// @brief Finds next json token in the iterator
    /// @param[in,out] it iterator pointing at the text stream of json to parse
    /// @param[out] token Output token from this tokenization operation
    /// @return `false` when iterator is at end
    /// @note Token bytes offset used then by Token::getToken are relative to the passed `iterator` start
    [[nodiscard]] static constexpr bool tokenizeNext(StringIteratorASCII& it, Token& token);

  private:
    friend struct SC::JsonTokenizerTest;
    [[nodiscard]] static constexpr bool scanToken(StringIteratorASCII& it, Token& token);
    [[nodiscard]] static constexpr bool skipWhitespaces(StringIteratorASCII& it);

    static constexpr void tokenizeString(StringIteratorASCII& it, const StringIteratorASCII start, Token& token);
    static constexpr void tokenizeNumber(StringIteratorASCII& it, StringIteratorASCII::CodePoint previousChar,
                                         Token& token);
    static constexpr void tokenizeTrue(StringIteratorASCII& it, Token& token);
    static constexpr void tokenizeFalse(StringIteratorASCII& it, Token& token);
    static constexpr void tokenizeNull(StringIteratorASCII& it, Token& token);
};

//-----------------------------------------------------------------------------------------------------------------------
// Implementation details
//-----------------------------------------------------------------------------------------------------------------------
constexpr bool SC::JsonTokenizer::tokenizeNext(StringIteratorASCII& it, Token& token)
{
    if (skipWhitespaces(it))
    {
        return scanToken(it, token);
    }
    return false;
}

constexpr bool SC::JsonTokenizer::scanToken(StringIteratorASCII& it, Token& token)
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

// Returns false when iterator is at end
constexpr bool SC::JsonTokenizer::skipWhitespaces(StringIteratorASCII& it)
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

constexpr void SC::JsonTokenizer::tokenizeString(StringIteratorASCII& it, const StringIteratorASCII start, Token& token)
{
    while (it.advanceUntilMatches('"')) // find the end of the string
    {
        if (it.isPrecededBy('\\')) // if the quote is escaped continue search
            continue;
        StringIteratorASCII startNext = start;
        (void)startNext.stepForward();  // but let's slice away leading '"'
        token.type     = Token::String; // Ok we have a (not validated) string
        auto realStart = start;
        realStart.setToStart();
        token.tokenStartBytes  = static_cast<size_t>(startNext.bytesDistanceFrom(realStart));
        token.tokenLengthBytes = static_cast<size_t>(it.bytesDistanceFrom(startNext));
        (void)it.advanceCodePoints(1); // eat the ending \" in the iterator
        break;
    }
}

constexpr void SC::JsonTokenizer::tokenizeNumber(StringIteratorASCII& it, StringIteratorASCII::CodePoint previousChar,
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

constexpr void SC::JsonTokenizer::tokenizeTrue(StringIteratorASCII& it, Token& token)
{
    if (it.advanceIfMatches('r') and it.advanceIfMatches('u') and it.advanceIfMatches('e'))
    {
        token.type = Token::True;
    }
}

constexpr void SC::JsonTokenizer::tokenizeFalse(StringIteratorASCII& it, Token& token)
{
    if (it.advanceIfMatches('a') and it.advanceIfMatches('l') and it.advanceIfMatches('s') and it.advanceIfMatches('e'))
    {
        token.type = Token::False;
    }
}

constexpr void SC::JsonTokenizer::tokenizeNull(StringIteratorASCII& it, Token& token)
{
    if (it.advanceIfMatches('u') and it.advanceIfMatches('l') and it.advanceIfMatches('l'))
    {
        token.type = Token::Null;
    }
}
