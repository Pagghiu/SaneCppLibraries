// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/StringSpan.h"

namespace SC
{
//! @addtogroup group_http
//! @{

/// @brief Incremental HTTP multipart/form-data parser
struct SC_COMPILER_EXPORT HttpMultipartParser
{
    /// @brief Initializes the parser with the given boundary (that excludes the leading '--')
    Result initWithBoundary(StringSpan boundary);

    /// @brief Resets the parser state
    void reset();

    /// @brief State of the parser
    enum class State
    {
        Parsing,  ///< Parser is parsing
        Result,   ///< Parser is reporting a result
        Finished, ///< Parser has finished
    };

    /// @brief One possible Token reported by the parser
    enum class Token
    {
        HeaderName,    ///< Name of a part header has been found
        HeaderValue,   ///< Value of a part header has been found
        PartBody,      ///< A chunk of the part body has been found
        Boundary,      ///< A boundary has been found (start of new part)
        PartHeaderEnd, ///< Headers for the current part have finished
        Finished,      ///< End of all parts
    };

    Token token = Token::Boundary; ///< Last found result
    State state = State::Parsing;  ///< Current state of the parser

    /// @brief Parse an incoming chunk of bytes, returning actually parsed span
    /// @param data Incoming chunk of bytes to be parsed
    /// @param readBytes Number of bytes actually read
    /// @param parsedData A sub-span of `data` pointing at the actually parsed data
    /// @return Valid result if parse didn't encounter any error
    Result parse(Span<const char> data, size_t& readBytes, Span<const char>& parsedData);

  private:
    char boundaryStorage[71] = {0}; // MAX = 70 + NUL
    char boundaryBuffer[128] = {0};

    size_t tokenStart   = 0;
    size_t tokenLength  = 0;
    size_t globalStart  = 0;
    size_t globalLength = 0;
    size_t matchIndex   = 0; // For matching boundary and other fixed strings

    int topLevelCoroutine     = 0;
    int nestedParserCoroutine = 0;

    uint8_t boundaryMatchIndex = 0;

    StringSpan boundary;

    // Helper functions for parsing
    [[nodiscard]] bool parseBoundary(char currentChar);
    [[nodiscard]] bool parseHeaders(char currentChar);
    [[nodiscard]] bool parseBody(char currentChar);

    // Specific parsers
    [[nodiscard]] bool parsePreamble(char currentChar);
    [[nodiscard]] bool parseBoundaryLine(char currentChar);
    [[nodiscard]] bool parseHeaderName(char currentChar);
    [[nodiscard]] bool parseHeaderValue(char currentChar);
    [[nodiscard]] bool parseBodyUntilBoundary(char currentChar);

    template <bool (HttpMultipartParser::*Func)(char), Token currentResult>
    Result process(Span<const char>& data, size_t& readBytes, Span<const char>& parsedData);
};

//! @}
} // namespace SC
