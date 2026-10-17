// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/AsyncExecution/AsyncExecution.h"

// Pure CPU transform used by the conference example. The owning buffer follows the sender pipeline.
inline SC::Result base64EncodeInPlace(SC::AsyncExecutionBuffer& input)
{
    using namespace SC;
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (input.size() > (size_t(-1) / 4) * 3 - 2)
        return Result::Error(AsyncExecutionResultCategory, AsyncExecutionError::FileTooLarge);
    AsyncExecutionBuffer output;
    SC_TRY(output.resize(((input.size() + 2) / 3) * 4));
    size_t written = 0;
    for (size_t i = 0; i < input.size(); i += 3)
    {
        const size_t remaining    = input.size() - i;
        const auto   a            = static_cast<unsigned char>(input.bytes()[i]);
        const auto   b            = remaining > 1 ? static_cast<unsigned char>(input.bytes()[i + 1]) : 0u;
        const auto   c            = remaining > 2 ? static_cast<unsigned char>(input.bytes()[i + 2]) : 0u;
        output.bytes()[written++] = alphabet[a >> 2];
        output.bytes()[written++] = alphabet[((a & 3u) << 4) | (b >> 4)];
        output.bytes()[written++] = remaining > 1 ? alphabet[((b & 15u) << 2) | (c >> 6)] : '=';
        output.bytes()[written++] = remaining > 2 ? alphabet[c & 63u] : '=';
    }
    input = move(output);
    return Result(true);
}

// Checked value for SC::tryThen: errors propagate before any downstream consumer runs.
inline SC::AsyncExecutionValueResult<SC::AsyncExecutionBuffer> base64Encode(SC::AsyncExecutionBuffer input) noexcept
{
    const SC::Result status = base64EncodeInPlace(input);
    return {status, SC::move(input)};
}
