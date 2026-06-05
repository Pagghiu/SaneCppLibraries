// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpMultipartParser.h"
#include "../Common/CompilerMinMax.h"
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
struct HttpMultipartInternal
{
    static StringSpan trim(StringSpan value)
    {
        const char* data  = value.bytesWithoutTerminator();
        size_t      start = 0;
        size_t      end   = value.sizeInBytes();
        while (start < end and (data[start] == ' ' or data[start] == '\t'))
        {
            start++;
        }
        while (end > start and (data[end - 1] == ' ' or data[end - 1] == '\t'))
        {
            end--;
        }
        return {{data + start, end - start}, false, value.getEncoding()};
    }

    static bool equalsIgnoreCase(StringSpan left, StringSpan right)
    {
        if (left.sizeInBytes() != right.sizeInBytes())
        {
            return false;
        }
        const char* leftData  = left.bytesWithoutTerminator();
        const char* rightData = right.bytesWithoutTerminator();
        for (size_t idx = 0; idx < left.sizeInBytes(); ++idx)
        {
            char a = leftData[idx];
            char b = rightData[idx];
            if (a >= 'A' and a <= 'Z')
            {
                a = static_cast<char>(a - 'A' + 'a');
            }
            if (b >= 'A' and b <= 'Z')
            {
                b = static_cast<char>(b - 'A' + 'a');
            }
            if (a != b)
            {
                return false;
            }
        }
        return true;
    }

    static StringSpan unquote(StringSpan value)
    {
        if (value.sizeInBytes() >= 2)
        {
            const char* data = value.bytesWithoutTerminator();
            if (data[0] == '"' and data[value.sizeInBytes() - 1] == '"')
            {
                return {{data + 1, value.sizeInBytes() - 2}, false, value.getEncoding()};
            }
        }
        return value;
    }
};

bool HttpMultipartIsSafeFileName(StringSpan fileName)
{
    if (fileName.isEmpty() or fileName == "." or fileName == "..")
    {
        return false;
    }

    const char* data = fileName.bytesWithoutTerminator();
    for (size_t idx = 0; idx < fileName.sizeInBytes(); ++idx)
    {
        const char current = data[idx];
        if (current == '/' or current == '\\' or current == ':' or static_cast<unsigned char>(current) < 0x20)
        {
            return false;
        }
    }
    return true;
}

Result HttpMultipartContentDispositionView::parse(StringSpan headerValue)
{
    *this = {};

    StringSpan   header = HttpMultipartInternal::trim(headerValue);
    const char*  data   = header.bytesWithoutTerminator();
    const size_t length = header.sizeInBytes();
    SC_TRY_MSG(not header.isEmpty(), "Multipart Content-Disposition is empty");

    size_t cursor = 0;
    while (cursor < length and data[cursor] != ';')
    {
        cursor++;
    }
    disposition = HttpMultipartInternal::trim({{data, cursor}, false, header.getEncoding()});
    SC_TRY_MSG(not disposition.isEmpty(), "Multipart Content-Disposition disposition is empty");

    while (cursor < length)
    {
        if (data[cursor] == ';')
        {
            cursor++;
        }
        const size_t itemStart = cursor;
        while (cursor < length and data[cursor] != ';')
        {
            cursor++;
        }
        StringSpan item =
            HttpMultipartInternal::trim({{data + itemStart, cursor - itemStart}, false, header.getEncoding()});
        if (item.isEmpty())
        {
            continue;
        }

        const char* itemData = item.bytesWithoutTerminator();
        size_t      equals   = static_cast<size_t>(-1);
        for (size_t idx = 0; idx < item.sizeInBytes(); ++idx)
        {
            if (itemData[idx] == '=')
            {
                equals = idx;
                break;
            }
        }
        if (equals == static_cast<size_t>(-1))
        {
            continue;
        }

        StringSpan key   = HttpMultipartInternal::trim({{itemData, equals}, false, item.getEncoding()});
        StringSpan value = HttpMultipartInternal::trim(
            {{itemData + equals + 1, item.sizeInBytes() - equals - 1}, false, item.getEncoding()});
        value = HttpMultipartInternal::unquote(value);
        if (HttpMultipartInternal::equalsIgnoreCase(key, "name"))
        {
            name    = value;
            hasName = true;
        }
        else if (HttpMultipartInternal::equalsIgnoreCase(key, "filename"))
        {
            fileName    = value;
            hasFileName = true;
        }
    }

    return Result(true);
}

bool HttpMultipartContentDispositionView::isFormData() const
{
    return HttpMultipartInternal::equalsIgnoreCase(disposition, "form-data");
}

void HttpMultipartPartHeadersView::reset()
{
    contentDisposition = {};
    contentType        = {};
    disposition        = {};
}

Result HttpMultipartPartHeadersView::addHeader(StringSpan name, StringSpan value)
{
    if (HttpMultipartInternal::equalsIgnoreCase(name, "Content-Disposition"))
    {
        contentDisposition = value;
        SC_TRY(disposition.parse(value));
    }
    else if (HttpMultipartInternal::equalsIgnoreCase(name, "Content-Type"))
    {
        contentType = value;
    }
    return Result(true);
}

bool HttpMultipartPartHeadersView::hasSafeFileName() const
{
    return disposition.hasFileName and HttpMultipartIsSafeFileName(disposition.fileName);
}

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
    tokenStart              = 0;
    tokenLength             = 0;
    token                   = Token::Boundary;
    state                   = State::Parsing;
    globalStart             = 0;
    globalLength            = 0;
    topLevelCoroutine       = 0;
    nestedParserCoroutine   = 0;
    matchIndex              = 0;
    boundaryMatchIndex      = 0;
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
        token                   = Token::PartBody;
        state                   = State::Result;
        parsedData              = {boundaryBuffer, boundaryCandidateLength};
        readBytes               = 0;
        emitBoundaryCandidate   = false;
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
                parsedData = {};
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
                parsedData = {};
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
                parsedData = {};
                return Result(true);
            }
        }

        readBytes = consumed;
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
