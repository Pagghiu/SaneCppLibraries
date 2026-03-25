// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpParsedHeaders.h"
#include "HttpStringIterator.h"

#include <string.h>

namespace SC
{
void HttpParsedHeaders::reset(HttpParser::Type type, Span<char> memory)
{
    readHeaders        = {memory.data(), 0};
    availableHeader    = memory;
    headersEndReceived = false;
    parsedSuccessfully = true;
    headersEndMatch    = 0;
    parser             = {};
    parser.type        = type;
    numTokens          = 0;
}

bool HttpParsedHeaders::getHeader(StringSpan headerName, StringSpan& value) const
{
    for (size_t idx = 0; idx < numTokens; ++idx)
    {
        const TokenOffset& header = tokenOffsets[idx];
        if (header.token == HttpParser::Token::HeaderName)
        {
            StringSpan name({readHeaders.data() + header.start, header.length}, false, StringEncoding::Ascii);
            if (idx + 1 < numTokens and tokenOffsets[idx + 1].token == HttpParser::Token::HeaderValue and
                HttpStringIterator::equalsIgnoreCase(name, headerName))
            {
                const TokenOffset& valueHeader = tokenOffsets[idx + 1];
                value = StringSpan({readHeaders.data() + valueHeader.start, valueHeader.length}, false,
                                   StringEncoding::Ascii);
                return true;
            }
        }
    }
    return false;
}

bool HttpParsedHeaders::findParserToken(HttpParser::Token token, StringSpan& value) const
{
    for (size_t idx = 0; idx < numTokens; ++idx)
    {
        const TokenOffset& header = tokenOffsets[idx];
        if (header.token == token)
        {
            value = StringSpan({readHeaders.data() + header.start, header.length}, false, StringEncoding::Ascii);
            return true;
        }
    }
    return false;
}

size_t HttpParsedHeaders::getHeadersLength() const
{
    if (numTokens > 0)
    {
        const TokenOffset& last = tokenOffsets[numTokens - 1];
        if (last.token == HttpParser::Token::HeadersEnd)
        {
            return static_cast<size_t>(last.start + last.length);
        }
    }
    return 0;
}

Result HttpParsedHeaders::scanHeadersEnd(Span<const char> readData, size_t& bytesToCopy, bool& foundHeadersEnd)
{
    bytesToCopy     = readData.sizeInBytes();
    foundHeadersEnd = false;
    for (size_t idx = 0; idx < readData.sizeInBytes(); ++idx)
    {
        const char current = readData.data()[idx];
        switch (headersEndMatch)
        {
        case 0: headersEndMatch = static_cast<uint8_t>(current == '\r' ? 1 : 0); break;
        case 1:
            if (current == '\n')
                headersEndMatch = 2;
            else if (current == '\r')
                headersEndMatch = 1;
            else
                headersEndMatch = 0;
            break;
        case 2: headersEndMatch = static_cast<uint8_t>(current == '\r' ? 3 : 0); break;
        case 3:
            if (current == '\n')
            {
                headersEndMatch = 4;
            }
            else if (current == '\r')
            {
                headersEndMatch = 1;
            }
            else
            {
                headersEndMatch = 0;
            }
            break;
        default: headersEndMatch = 0; break;
        }

        if (headersEndMatch == 4)
        {
            foundHeadersEnd = true;
            bytesToCopy     = idx + 1;
            headersEndMatch = 0;
            break;
        }
    }
    return Result(true);
}

Result HttpParsedHeaders::copyHeaderBytes(uint32_t maxHeaderSize, Span<const char> readData, size_t bytesToCopy,
                                          const char* outOfSpaceError, const char* sizeExceededError)
{
    if (bytesToCopy > availableHeader.sizeInBytes())
    {
        return Result::FromStableCharPointer(outOfSpaceError);
    }
    if (readHeaders.sizeInBytes() + bytesToCopy > maxHeaderSize)
    {
        return Result::FromStableCharPointer(sizeExceededError);
    }

    const size_t previousHeaderSize = readHeaders.sizeInBytes();
    ::memcpy(availableHeader.data(), readData.data(), bytesToCopy);
    readHeaders = {readHeaders.data(), previousHeaderSize + bytesToCopy};

    if (not availableHeader.sliceStart(bytesToCopy, availableHeader))
    {
        parsedSuccessfully = false;
        return Result::FromStableCharPointer(outOfSpaceError);
    }
    return Result(true);
}

Result HttpParsedHeaders::pushToken()
{
    TokenOffset token;
    token.token  = parser.token;
    token.start  = static_cast<uint32_t>(parser.tokenStart);
    token.length = static_cast<uint32_t>(parser.tokenLength);
    if (numTokens < MaxNumTokens)
    {
        tokenOffsets[numTokens++] = token;
        return Result(true);
    }
    parsedSuccessfully = false;
    return Result(false);
}

Result HttpParsedHeaders::unshiftPendingBody(Span<const char> readData, AsyncReadableStream& stream,
                                             AsyncBufferView::ID bufferID, size_t bodyOffset)
{
    const size_t        bodyLength = readData.sizeInBytes() - bodyOffset;
    AsyncBufferView::ID childID;
    SC_TRY(stream.getBuffersPool().createChildView(bufferID, bodyOffset, bodyLength, childID));
    SC_TRY(stream.unshift(childID));
    stream.getBuffersPool().unrefBuffer(childID);
    return Result(true);
}
} // namespace SC
