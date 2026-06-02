// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/Result.h"
#include "../../Foundation/Span.h"
#include "../../Foundation/StringSpan.h"
#include "../HttpExport.h"

namespace SC
{
struct SC_HTTP_EXPORT HttpFixedBufferWriter
{
    void reset(Span<char> memory)
    {
        buffer = memory;
        size   = 0;
    }

    [[nodiscard]] Span<char>       written() { return {buffer.data(), size}; }
    [[nodiscard]] Span<const char> written() const { return {buffer.data(), size}; }
    [[nodiscard]] size_t           writtenBytes() const { return size; }
    [[nodiscard]] size_t           capacity() const { return buffer.sizeInBytes(); }

    Result append(Span<const char> value, const char* outOfSpaceError);
    Result append(StringSpan value, const char* outOfSpaceError) { return append(value.toCharSpan(), outOfSpaceError); }

    template <size_t N>
    Result appendLiteral(const char (&value)[N], const char* outOfSpaceError)
    {
        return append({value, N - 1}, outOfSpaceError);
    }

    Result appendHeader(StringSpan name, StringSpan value, const char* outOfSpaceError);
    Result appendContentLength(uint64_t value, const char* outOfSpaceError, const char* formatError);

  private:
    Span<char> buffer;
    size_t     size = 0;
};
} // namespace SC
