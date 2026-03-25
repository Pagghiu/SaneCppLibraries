// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../AsyncStreams/AsyncStreams.h"
#include "../../Foundation/StringSpan.h"
#include "../HttpParser.h"

namespace SC
{
struct SC_COMPILER_EXPORT HttpParsedHeaders
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
                        AsyncBufferView::ID bufferID, const char* outOfSpaceError, const char* sizeExceededError,
                        bool stopAtHeadersEnd, OnParserResult&& onParserResult)
    {
        if (headersEndReceived)
        {
            return Result(true);
        }

        size_t bytesToCopy     = readData.sizeInBytes();
        bool   foundHeadersEnd = false;
        SC_TRY(scanHeadersEnd(readData, bytesToCopy, foundHeadersEnd));

        if (bytesToCopy > 0)
        {
            SC_TRY(copyHeaderBytes(maxHeaderSize, readData, bytesToCopy, outOfSpaceError, sizeExceededError));
        }

        if (not foundHeadersEnd)
        {
            return Result(true);
        }

        Span<const char> headerData = {readHeaders.data(), readHeaders.sizeInBytes()};
        size_t           readBytes  = 0;
        while (parsedSuccessfully and parser.state != HttpParser::State::Finished)
        {
            Span<const char> parsedData;
            parsedSuccessfully &= parser.parse(headerData, readBytes, parsedData);
            if (not parsedSuccessfully)
            {
                break;
            }

            if (parser.state == HttpParser::State::Result)
            {
                SC_TRY(pushToken());
                SC_TRY(onParserResult(parser));
            }

            if (readBytes > 0)
            {
                parsedSuccessfully &= headerData.sliceStart(readBytes, headerData);
            }
            else if (parser.state != HttpParser::State::Finished)
            {
                parsedSuccessfully = false;
                break;
            }

            if (stopAtHeadersEnd and parser.token == HttpParser::Token::HeadersEnd)
            {
                break;
            }
        }

        SC_TRY(parsedSuccessfully);
        headersEndReceived = true;

        if (bytesToCopy < readData.sizeInBytes())
        {
            SC_TRY(unshiftPendingBody(readData, stream, bufferID, bytesToCopy));
        }

        return Result(true);
    }

    Span<char> readHeaders;
    Span<char> availableHeader;

    bool    headersEndReceived = false;
    bool    parsedSuccessfully = true;
    uint8_t headersEndMatch    = 0;

    HttpParser parser;

    TokenOffset tokenOffsets[MaxNumTokens];
    size_t      numTokens = 0;

  private:
    Result scanHeadersEnd(Span<const char> readData, size_t& bytesToCopy, bool& foundHeadersEnd);
    Result copyHeaderBytes(uint32_t maxHeaderSize, Span<const char> readData, size_t bytesToCopy,
                           const char* outOfSpaceError, const char* sizeExceededError);
    Result pushToken();
    Result unshiftPendingBody(Span<const char> readData, AsyncReadableStream& stream, AsyncBufferView::ID bufferID,
                              size_t bodyOffset);
};
} // namespace SC
