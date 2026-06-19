// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/CompilerMacrosExport.h"
#include "../../Common/StringSpan.h"
#ifndef SC_EXPORT_LIBRARY_SERIALIZATION_TEXT
#define SC_EXPORT_LIBRARY_SERIALIZATION_TEXT 0
#endif
#define SC_SERIALIZATION_TEXT_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_SERIALIZATION_TEXT)

namespace SC
{
struct SC_SERIALIZATION_TEXT_EXPORT JsonTokenizer;
struct SC_SERIALIZATION_TEXT_EXPORT JsonTokenizerTest;
} // namespace SC

/// @brief Tokenize a JSON text stream, without validating numbers and strings
struct SC::JsonTokenizer
{
    struct SC_SERIALIZATION_TEXT_EXPORT Cursor
    {
        StringSpan text;
        size_t     position = 0;
        bool       valid    = true;

        constexpr Cursor() = default;
        constexpr Cursor(StringSpan text) : text(text), valid(text.getEncoding() != StringEncoding::Utf16) {}

        [[nodiscard]] constexpr bool read(char& character)
        {
            if (not valid or position >= text.sizeInBytes())
            {
                return false;
            }
            character = text.bytesWithoutTerminator()[position++];
            return true;
        }

        constexpr void stepBackward()
        {
            if (position > 0)
            {
                position -= 1;
            }
        }

        [[nodiscard]] constexpr bool advanceIfMatches(char character)
        {
            if (not valid or position >= text.sizeInBytes() or text.bytesWithoutTerminator()[position] != character)
            {
                return false;
            }
            position += 1;
            return true;
        }
    };

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

        /// @brief Get current Token as StringSpan slice from the passed string
        /// @param source The StringSpan that has been used when parsing
        /// @return StringSpan slice of `source` representing this token
        constexpr StringSpan getToken(StringSpan source) const
        {
            return {{source.bytesWithoutTerminator() + tokenStartBytes, tokenLengthBytes}, false, source.getEncoding()};
        }

      private:
        friend struct JsonTokenizer;
        size_t tokenStartBytes  = 0;
        size_t tokenLengthBytes = 0;
        Type   type;
    };

    /// @brief Finds next json token in the cursor
    /// @param[in,out] it cursor pointing at the text stream of json to parse
    /// @param[out] token Output token from this tokenization operation
    /// @return `false` when cursor is at end
    /// @note Token bytes offset used then by Token::getToken are relative to the passed `cursor` start
    [[nodiscard]] static constexpr bool tokenizeNext(Cursor& it, Token& token);

  private:
    friend struct SC::JsonTokenizerTest;
    [[nodiscard]] static constexpr bool scanToken(Cursor& it, Token& token);
    [[nodiscard]] static constexpr bool skipWhitespaces(Cursor& it);

    static constexpr void               tokenizeString(Cursor& it, size_t start, Token& token);
    [[nodiscard]] static constexpr bool isEscapedQuote(StringSpan text, size_t quotePosition);
    static constexpr void               tokenizeNumber(Cursor& it, char previousChar, Token& token);
    static constexpr void               tokenizeTrue(Cursor& it, Token& token);
    static constexpr void               tokenizeFalse(Cursor& it, Token& token);
    static constexpr void               tokenizeNull(Cursor& it, Token& token);

    [[nodiscard]] static constexpr bool isWhitespace(char character)
    {
        return character == '\t' or character == '\n' or character == '\r' or character == ' ';
    }

    [[nodiscard]] static constexpr bool isNumberCharacter(char character)
    {
        return (character >= '0' and character <= '9') or character == '.';
    }
};

//-----------------------------------------------------------------------------------------------------------------------
// Implementation details
//-----------------------------------------------------------------------------------------------------------------------
constexpr bool SC::JsonTokenizer::tokenizeNext(Cursor& it, Token& token)
{
    if (skipWhitespaces(it))
    {
        return scanToken(it, token);
    }
    return false;
}

constexpr bool SC::JsonTokenizer::scanToken(Cursor& it, Token& token)
{
    token = Token();

    char         current = 0;
    const size_t start   = it.position;
    if (not it.read(current))
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
    token.tokenStartBytes  = start;
    token.tokenLengthBytes = it.position - start;
    return true;
}

// Returns false when cursor is at end
constexpr bool SC::JsonTokenizer::skipWhitespaces(Cursor& it)
{
    char current = 0;
    while (it.read(current))
    {
        if (not isWhitespace(current))
        {
            it.stepBackward(); // put back the read character
            return true;
        }
    }
    return false;
}

constexpr void SC::JsonTokenizer::tokenizeString(Cursor& it, size_t start, Token& token)
{
    const char*  bytes        = it.text.bytesWithoutTerminator();
    const size_t contentStart = start + 1;
    while (it.position < it.text.sizeInBytes())
    {
        if (bytes[it.position] == '"' and not isEscapedQuote(it.text, it.position))
        {
            token.type             = Token::String;
            token.tokenStartBytes  = contentStart;
            token.tokenLengthBytes = it.position - contentStart;
            it.position += 1; // eat the ending quote
            break;
        }
        it.position += 1;
    }
}

constexpr bool SC::JsonTokenizer::isEscapedQuote(StringSpan text, size_t quotePosition)
{
    size_t      backslashes = 0;
    const char* bytes       = text.bytesWithoutTerminator();
    while (quotePosition > 0 and bytes[quotePosition - 1] == '\\')
    {
        backslashes += 1;
        quotePosition -= 1;
    }
    return (backslashes % 2) == 1;
}

constexpr void SC::JsonTokenizer::tokenizeNumber(Cursor& it, char previousChar, Token& token)
{
    // eat all non whitespaces that could possibly form a number (to be validated, as it may contain multiple dots)
    if (not isNumberCharacter(previousChar))
    {
        return;
    }
    char current = 0;
    while (it.read(current))
    {
        if (not isNumberCharacter(current))
        {
            it.stepBackward(); // put it back
            break;
        }
    }

    token.type = Token::Number;
}

constexpr void SC::JsonTokenizer::tokenizeTrue(Cursor& it, Token& token)
{
    if (it.advanceIfMatches('r') and it.advanceIfMatches('u') and it.advanceIfMatches('e'))
    {
        token.type = Token::True;
    }
}

constexpr void SC::JsonTokenizer::tokenizeFalse(Cursor& it, Token& token)
{
    if (it.advanceIfMatches('a') and it.advanceIfMatches('l') and it.advanceIfMatches('s') and it.advanceIfMatches('e'))
    {
        token.type = Token::False;
    }
}

constexpr void SC::JsonTokenizer::tokenizeNull(Cursor& it, Token& token)
{
    if (it.advanceIfMatches('u') and it.advanceIfMatches('l') and it.advanceIfMatches('l'))
    {
        token.type = Token::Null;
    }
}
