// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Extra/Experimental/AsyncExecution/AsyncExecution.h"

// Input and output must be disjoint. Validate all capacity arithmetic before writing; no terminator is added.
inline SC::Result base64Encode(SC::Span<const char> input, SC::Span<char> output,
                               SC::Span<const char>& written) noexcept
{
    using namespace SC;
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t   groups     = input.sizeInBytes() / 3 + (input.sizeInBytes() % 3 != 0);
    if (groups > size_t(-1) / 4 or groups * 4 > output.sizeInBytes())
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge);
    size_t used = 0;
    for (size_t i = 0; i < input.sizeInBytes();)
    {
        const size_t remaining = input.sizeInBytes() - i;
        const auto   a         = static_cast<unsigned char>(input[i++]);
        const auto   b         = remaining > 1 ? static_cast<unsigned char>(input[i++]) : 0u;
        const auto   c         = remaining > 2 ? static_cast<unsigned char>(input[i++]) : 0u;
        output[used++]         = alphabet[a >> 2];
        output[used++]         = alphabet[((a & 3u) << 4) | (b >> 4)];
        output[used++]         = remaining > 1 ? alphabet[((b & 15u) << 2) | (c >> 6)] : '=';
        output[used++]         = remaining > 2 ? alphabet[c & 63u] : '=';
    }
    written = {output.data(), used};
    return Result(true);
}

inline SC::AsyncExecutionValueResult<SC::Span<const char>> base64Encode(SC::Span<const char> input,
                                                                        SC::Span<char>       output) noexcept
{
    SC::Span<const char> written;
    const SC::Result     status = base64Encode(input, output, written);
    return {status, written};
}
