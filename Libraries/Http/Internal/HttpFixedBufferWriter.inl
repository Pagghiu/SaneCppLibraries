// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpFixedBufferWriter.h"

#include <stdio.h>
#include <string.h>

namespace SC
{
Result HttpFixedBufferWriter::append(Span<const char> value, HttpError outOfSpaceError)
{
    if (size > buffer.sizeInBytes() or value.sizeInBytes() > buffer.sizeInBytes() - size)
    {
        return Result::Error(HttpResultCategory, outOfSpaceError);
    }
    if (not value.empty())
    {
        ::memcpy(buffer.data() + size, value.data(), value.sizeInBytes());
    }
    size += value.sizeInBytes();
    return Result(true);
}

Result HttpFixedBufferWriter::appendHeader(StringSpan name, StringSpan value, HttpError outOfSpaceError)
{
    SC_TRY(append(name, outOfSpaceError));
    SC_TRY(appendLiteral(": ", outOfSpaceError));
    SC_TRY(append(value, outOfSpaceError));
    SC_TRY(appendLiteral("\r\n", outOfSpaceError));
    return Result(true);
}

Result HttpFixedBufferWriter::appendContentLength(uint64_t value, HttpError outOfSpaceError)
{
    char      lengthBuffer[32];
    const int len = ::snprintf(lengthBuffer, sizeof(lengthBuffer), "%llu", static_cast<unsigned long long>(value));
    if (len <= 0)
    {
        return Result::Error(HttpResultCategory, HttpError::ContentLengthFormattingFailed);
    }
    return appendHeader("Content-Length", {{lengthBuffer, static_cast<size_t>(len)}, false, StringEncoding::Ascii},
                        outOfSpaceError);
}
} // namespace SC
