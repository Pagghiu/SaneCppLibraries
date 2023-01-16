// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Types.h"
namespace SC
{
struct OS;
}

struct SC::OS
{
    [[nodiscard]] static bool   printBacktrace();
    [[nodiscard]] static bool   printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes);
    [[nodiscard]] static size_t captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                                 size_t backtraceBufferSizeInBytes, uint32_t* hash);
};
