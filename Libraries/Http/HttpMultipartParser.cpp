// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpMultipartParser.h"
#include <string.h>

// https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
#if SC_COMPILER_MSVC
#define SC_CAT(x, y)  SC_CAT2(x, y)
#define SC_CAT2(x, y) x##y
// https://developercommunity.visualstudio.com/t/-line-cannot-be-used-as-an-argument-for-constexpr/195665
//  __LINE__ cannot be used as a constexpr expression when edit and continue is enabled
#define SC_LINE int(SC_CAT(__LINE__, u))
#else
#define SC_LINE __LINE__
#endif

#define SC_CO_BEGIN(state)                                                                                             \
    switch (state)                                                                                                     \
    {                                                                                                                  \
    case 0:
#define SC_CO_RETURN(state, x)                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        state = SC_LINE;                                                                                               \
        return x;                                                                                                      \
    case SC_LINE:;                                                                                                     \
    } while (0)
#define SC_CO_FINISH(state)                                                                                            \
    state = -1;                                                                                                        \
    }
#define crReset(state) state = 0;

namespace SC
{
Result HttpMultipartParser::initWithBoundary(StringSpan boundaryValue)
{
    reset();
#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
    ::strncpy_s(boundaryStorage, sizeof(boundaryStorage), boundaryValue.bytesWithoutTerminator(),
                min(sizeof(boundaryStorage) - 1, boundaryValue.sizeInBytes()));
#else
    ::strncpy(boundaryStorage, boundaryValue.bytesWithoutTerminator(),
              min(sizeof(boundaryStorage) - 1, boundaryValue.sizeInBytes()));
#endif
    boundary = StringSpan::fromNullTerminated(boundaryStorage, StringEncoding::Ascii);
    return Result(boundary.StringSpan::sizeInBytes() == boundaryValue.sizeInBytes());
}

void HttpMultipartParser::reset()
{
    tokenStart            = 0;
    tokenLength           = 0;
    token                 = Token::Boundary;
    state                 = State::Parsing;
    globalStart           = 0;
    globalLength          = 0;
    topLevelCoroutine     = 0;
    nestedParserCoroutine = 0;
    matchIndex            = 0;
    boundaryMatchIndex    = 0;
    boundaryCandidateLength = 0;
    emitBoundaryCandidate   = false;
    ::memset(boundaryBuffer, 0, sizeof(boundaryBuffer));
}

Result HttpMultipartParser::parse(Span<const char> data, size_t& readBytes, Span<const char>& parsedData)
{
    readBytes = 0;
    if (state == State::Finished)
    {
        return Result(false);
    }

    if (emitBoundaryCandidate)
    {
        token                 = Token::PartBody;
        state                 = State::Result;
        parsedData            = {boundaryBuffer, boundaryCandidateLength};
        readBytes             = 0;
        emitBoundaryCandidate = false;
        boundaryCandidateLength = 0;
        return Result(true);
    }

    // Check if we are in "CheckingBoundary" state
    if (boundaryMatchIndex == 1)
    {
        const size_t newPartLength = 4 + boundary.sizeInBytes() + 2; // \r\n--boundary\r\n
        const size_t finalLength   = 4 + boundary.sizeInBytes() + 2; // \r\n--boundary--
        size_t       consumed      = 0;

        const auto isPrefix = [&](const char* suffix) -> bool
        {
            for (size_t idx = 0; idx < boundaryCandidateLength; ++idx)
            {
                char expected = 0;
                if (idx == 0)
                    expected = '\r';
                else if (idx == 1)
                    expected = '\n';
                else if (idx == 2 or idx == 3)
                    expected = '-';
                else if (idx < 4 + boundary.sizeInBytes())
                    expected = boundary.bytesWithoutTerminator()[idx - 4];
                else
                    expected = suffix[idx - 4 - boundary.sizeInBytes()];

                if (boundaryBuffer[idx] != expected)
                {
                    return false;
                }
            }
            return true;
        };

        while (consumed < data.sizeInBytes())
        {
            SC_TRY_MSG(boundaryCandidateLength < sizeof(boundaryBuffer),
                       "HttpMultipartParser boundary candidate too large");
            boundaryBuffer[boundaryCandidateLength++] = data[consumed++];

            const bool newPartPrefix = isPrefix("\r\n");
            const bool finalPrefix   = isPrefix("--");
            if (not newPartPrefix and not finalPrefix)
            {
                boundaryMatchIndex    = 0;
                emitBoundaryCandidate = true;
                state                 = State::Parsing;
                readBytes             = consumed;
                globalLength += consumed;
                parsedData            = {};
                return Result(true);
            }
            if (newPartPrefix and boundaryCandidateLength == newPartLength)
            {
                token                   = Token::Boundary;
                state                   = State::Result;
                boundaryMatchIndex      = 0;
                boundaryCandidateLength = 0;
                readBytes               = consumed;
                globalLength += consumed;
                parsedData              = {};
                return Result(true);
            }
            if (finalPrefix and boundaryCandidateLength == finalLength)
            {
                token                   = Token::Finished;
                state                   = State::Finished;
                boundaryMatchIndex      = 0;
                boundaryCandidateLength = 0;
                readBytes               = consumed;
                globalLength += consumed;
                parsedData              = {};
                return Result(true);
            }
        }

        readBytes  = consumed;
        globalLength += consumed;
        parsedData = {};
        state      = State::Parsing;
        if (readBytes > 0)
        {
            return Result(true);
        }
    }

    if (data.sizeInBytes() == 0)
        return Result(true);

    SC_CO_BEGIN(topLevelCoroutine);

    tokenStart  = globalStart;
    tokenLength = 0;
    do
    {
        SC_TRY((process<&HttpMultipartParser::parsePreamble, Token::Boundary>(data, readBytes, parsedData)));
        SC_CO_RETURN(topLevelCoroutine, Result(true));
    } while (state == State::Parsing);

    while (true)
    {
        //------------------------
        // Parse Headers
        //------------------------
        while (true)
        {
            // Parse Header Name
            globalStart += globalLength;
            tokenStart   = globalStart;
            tokenLength  = 0;
            globalLength = 0;
            do
            {
                SC_TRY(
                    (process<&HttpMultipartParser::parseHeaderName, Token::HeaderName>(data, readBytes, parsedData)));
                SC_CO_RETURN(topLevelCoroutine, Result(true));
            } while (state == State::Parsing);

            if (tokenLength == 0)
            {
                break;
            }

            // Parse Header Value
            globalStart += globalLength;
            tokenStart   = globalStart;
            tokenLength  = 0;
            globalLength = 0;
            do
            {
                SC_TRY(
                    (process<&HttpMultipartParser::parseHeaderValue, Token::HeaderValue>(data, readBytes, parsedData)));
                SC_CO_RETURN(topLevelCoroutine, Result(true));
            } while (state == State::Parsing);
        }

        // Emit PartHeaderEnd
        token = Token::PartHeaderEnd;
        state = State::Result;
        SC_TRY(data.sliceStartLength(0, 0, parsedData));
        SC_CO_RETURN(topLevelCoroutine, Result(true));

        //------------------------
        // Parse Body
        //------------------------
        globalStart += globalLength;
        tokenStart         = globalStart;
        tokenLength        = 0;
        globalLength       = 0;
        boundaryMatchIndex = 0; // Reset checking state
        do
        {
            SC_TRY(
                (process<&HttpMultipartParser::parseBodyUntilBoundary, Token::PartBody>(data, readBytes, parsedData)));

            SC_CO_RETURN(topLevelCoroutine, Result(true));
            state = State::Parsing;
        } while (state == State::Parsing);

        if (state == State::Finished)
            break;
        if (token == Token::Finished)
        {
            state = State::Finished;
            break;
        }
    }

    SC_CO_FINISH(topLevelCoroutine);
    return Result(true);
}

template <bool (HttpMultipartParser::*Func)(char), HttpMultipartParser::Token currentResult>
Result HttpMultipartParser::process(Span<const char>& data, size_t& readBytes, Span<const char>& parsedData)
{
    const auto initialStart  = tokenStart;
    const auto initialLength = tokenLength;

    token     = currentResult;
    state     = State::Parsing;
    readBytes = 0;

    for (auto c : data)
    {
        tokenLength++;
        SC_TRY((this->*Func)(c));

        if (boundaryMatchIndex == 1)
        {
            if (tokenLength > 0)
            {
                tokenLength--; // Backtrack for boundary check
            }
            state = State::Result;
            break;
        }

        readBytes++;
        if (state == State::Result)
        {
            break;
        }
    }

    globalLength += readBytes;
    const auto startDelta  = tokenStart - initialStart;
    const auto lengthDelta = (tokenLength >= initialLength) ? (tokenLength - initialLength) : 0;
    if (state == State::Result)
    {
        SC_TRY(data.sliceStartLength(startDelta, lengthDelta, parsedData));
        SC_TRY(data.sliceStart(readBytes, data));
        nestedParserCoroutine = 0;
    }
    else
    {
        SC_TRY(data.sliceStartLength(startDelta, lengthDelta, parsedData));
    }
    return Result(true);
}

// -----------------------------------------------------------------------
// Specific Parsers
// -----------------------------------------------------------------------

bool HttpMultipartParser::parsePreamble(char currentChar)
{
    SC_CO_BEGIN(nestedParserCoroutine);
    while (true)
    {
        // Search for --boundary (at start) or \r\n--boundary (anywhere)
        if (globalStart == 0)
        {
            matchIndex = 0;
            if (currentChar == '-')
            {
                SC_CO_RETURN(nestedParserCoroutine, true);
                if (currentChar == '-')
                {
                    while (matchIndex < boundary.sizeInBytes())
                    {
                        SC_CO_RETURN(nestedParserCoroutine, true);
                        if (currentChar == boundary.bytesWithoutTerminator()[matchIndex])
                        {
                            matchIndex++;
                        }
                        else
                        {
                            goto search_crlf;
                        }
                    }
                    goto first_boundary_found;
                }
            }
        }

    search_crlf:
        while (currentChar != '\r')
        {
            SC_CO_RETURN(nestedParserCoroutine, true);
        }
        SC_CO_RETURN(nestedParserCoroutine, true);
        if (currentChar != '\n')
            continue;
        SC_CO_RETURN(nestedParserCoroutine, true);
        if (currentChar != '-')
            continue;
        SC_CO_RETURN(nestedParserCoroutine, true);
        if (currentChar != '-')
            continue;

        matchIndex = 0;
        while (matchIndex < boundary.sizeInBytes())
        {
            SC_CO_RETURN(nestedParserCoroutine, true);
            if (currentChar == boundary.bytesWithoutTerminator()[matchIndex])
            {
                matchIndex++;
            }
            else
            {
                goto search_crlf;
            }
        }
        goto first_boundary_found;
    }

first_boundary_found:
    // Match suffix CRLF or --
    SC_CO_RETURN(nestedParserCoroutine, true);
    if (currentChar == '\r')
    {
        SC_CO_RETURN(nestedParserCoroutine, true);
        if (currentChar == '\n')
        {
            state = State::Result;
        }
        else
            goto search_crlf;
    }
    else if (currentChar == '-')
    {
        SC_CO_RETURN(nestedParserCoroutine, true);
        if (currentChar == '-')
        {
            state = State::Finished;
        }
        else
            goto search_crlf;
    }
    else
        goto search_crlf;

    crReset(nestedParserCoroutine);
    SC_CO_FINISH(nestedParserCoroutine);
    return true;
}

bool HttpMultipartParser::parseBoundaryLine(char currentChar)
{
    // Simplified parser for initial boundary: --boundary\r\n
    // Or just --boundary if check
    static constexpr char dashDash[] = "--";
    SC_CO_BEGIN(nestedParserCoroutine);

    matchIndex = 0;
    // Match --
    while (matchIndex < 2)
    {
        if (currentChar == dashDash[matchIndex])
        {
            matchIndex++;
            SC_CO_RETURN(nestedParserCoroutine, true);
        }
        else
        {
            return false; // Error: Expected -- at start
        }
    }

    // Match boundary
    matchIndex = 0;
    while (matchIndex < boundary.sizeInBytes())
    {
        if (currentChar == boundary.bytesWithoutTerminator()[matchIndex])
        {
            matchIndex++;
            SC_CO_RETURN(nestedParserCoroutine, true);
        }
        else
        {
            return false;
        }
    }

    // Match CRLF
    if (currentChar == '\r')
    {
        SC_CO_RETURN(nestedParserCoroutine, true);
        if (currentChar == '\n')
        {
            state = State::Result;
        }
        else
            return false;
    }
    else
        return false;

    SC_CO_FINISH(nestedParserCoroutine);
    return true;
}

bool HttpMultipartParser::parseHeaderName(char currentChar)
{
    SC_CO_BEGIN(nestedParserCoroutine);

    if (currentChar == '\r')
    {
        if (tokenLength > 0)
            tokenLength--;
        SC_CO_RETURN(nestedParserCoroutine, true);
        if (currentChar == '\n')
        {
            if (tokenLength > 0)
                tokenLength--;
            state = State::Result;
        }
        else
            return false;
    }
    else
    {
        while (currentChar != ':')
        {
            if (currentChar == '\r')
                return false;
            SC_CO_RETURN(nestedParserCoroutine, true);
        }
        if (tokenLength > 0)
            tokenLength--;
        state = State::Result;
    }
    SC_CO_FINISH(nestedParserCoroutine);
    return true;
}

bool HttpMultipartParser::parseHeaderValue(char currentChar)
{
    SC_CO_BEGIN(nestedParserCoroutine);

    // Skip leading spaces
    while (currentChar == ' ')
    {
        if (tokenLength > 0)
            tokenLength--;
        tokenStart++;
        SC_CO_RETURN(nestedParserCoroutine, true);
    }

    while (true)
    {
        if (currentChar == '\r')
        {
            if (tokenLength > 0)
                tokenLength--; // Exclude \r
            SC_CO_RETURN(nestedParserCoroutine, true);
            if (currentChar == '\n')
            {
                if (tokenLength > 0)
                    tokenLength--;
                state = State::Result;
                break;
            }
            return false;
        }
        SC_CO_RETURN(nestedParserCoroutine, true);
    }
    SC_CO_FINISH(nestedParserCoroutine);
    return true;
}

bool HttpMultipartParser::parseBodyUntilBoundary(char currentChar)
{
    // If we just failed a boundary check, we must consume the current char
    // (which was the start of the mismatch sequence, e.g. '\r')
    if (boundaryMatchIndex == 2)
    {
        boundaryMatchIndex = 0;
        return true;
    }

    if (currentChar == '\r')
    {
        // Start checking boundary
        boundaryMatchIndex = 1;
        return true;
    }

    return true;
}
} // namespace SC
