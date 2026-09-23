// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../AsyncStreams/AsyncStreams.h"
#include "../../Common/StringSpan.h"
#include "../HttpError.h"
#include "../HttpExport.h"
#include "../HttpParser.h"

namespace SC
{
struct SC_HTTP_EXPORT HttpParsedHeaders
{
    struct TokenOffset
    {
        HttpParser::Token token  = HttpParser::Token::Method;
        uint32_t          start  = 0;
        uint32_t          length = 0;
    };

    static constexpr size_t MaxNumTokens = 64;

    void reset(HttpParser::Type type, Span<char> memory);

    [[nodiscard]] bool   getHeader(StringSpan headerName, StringSpan& value) const;
    [[nodiscard]] bool   findParserToken(HttpParser::Token token, StringSpan& value) const;
    [[nodiscard]] size_t getHeadersLength() const;

    template <typename OnParserResult>
    Result writeHeaders(uint32_t maxHeaderSize, Span<const char> readData, AsyncReadableStream& stream,
                        AsyncBufferView::ID bufferID, bool stopAtHeadersEnd, bool unshiftPendingBodyToStream,
                        OnParserResult&& onParserResult)
    {
        if (not failure)
        {
            return failure;
        }
        if (headersEndReceived)
        {
            return Result(true);
        }

        size_t bytesToCopy     = readData.sizeInBytes();
        bool   foundHeadersEnd = false;
        Result result          = scanHeadersEnd(readData, bytesToCopy, foundHeadersEnd);
        if (not result)
        {
            failure = result;
            return result;
        }

        if (bytesToCopy > 0)
        {
            result = copyHeaderBytes(maxHeaderSize, readData, bytesToCopy);
            if (not result)
            {
                failure = result;
                return result;
            }
        }

        if (not foundHeadersEnd)
        {
            return Result(true);
        }

        Span<const char> headerData = {readHeaders.data(), readHeaders.sizeInBytes()};
        size_t           readBytes  = 0;
        while (parser.state != HttpParser::State::Finished)
        {
            Span<const char> parsedData;
            result = parser.parse(headerData, readBytes, parsedData);
            if (not result)
            {
                failure = result;
                return result;
            }

            if (parser.state == HttpParser::State::Result)
            {
                result = pushToken();
                if (not result)
                {
                    failure = result;
                    return result;
                }
                result = onParserResult(parser);
                if (not result)
                {
                    failure = result;
                    return result;
                }
            }

            if (readBytes > 0)
            {
                if (not headerData.sliceStart(readBytes, headerData))
                {
                    failure = Result::Error(HttpResultCategory, HttpError::ParserSpanInvalid);
                    return failure;
                }
            }
            else if (parser.state != HttpParser::State::Finished)
            {
                failure = Result::Error(HttpResultCategory, HttpError::HeaderBlockIncomplete);
                return failure;
            }

            if (stopAtHeadersEnd and parser.token == HttpParser::Token::HeadersEnd)
            {
                break;
            }
        }

        headersEndReceived = true;

        if (unshiftPendingBodyToStream and bytesToCopy < readData.sizeInBytes())
        {
            result = unshiftPendingBody(readData, stream, bufferID, bytesToCopy);
            if (not result)
            {
                failure = result;
                return result;
            }
        }

        return Result(true);
    }

    Span<char> readHeaders;
    Span<char> availableHeader;

    bool    headersEndReceived = false;
    uint8_t headersEndMatch    = 0;

    Result failure = Result(true);

    HttpParser parser;

    TokenOffset tokenOffsets[MaxNumTokens];
    size_t      numTokens = 0;

  private:
    Result scanHeadersEnd(Span<const char> readData, size_t& bytesToCopy, bool& foundHeadersEnd);
    Result copyHeaderBytes(uint32_t maxHeaderSize, Span<const char> readData, size_t bytesToCopy);
    Result pushToken();
    Result unshiftPendingBody(Span<const char> readData, AsyncReadableStream& stream, AsyncBufferView::ID bufferID,
                              size_t bodyOffset);
};
} // namespace SC
