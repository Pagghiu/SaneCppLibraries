// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/String.h"

namespace SC
{
struct SystemDebug;
struct SystemDirectories;
} // namespace SC

struct SC::SystemDebug
{
    [[nodiscard]] static bool   printBacktrace();
    [[nodiscard]] static bool   printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes);
    [[nodiscard]] static size_t captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                                 size_t backtraceBufferSizeInBytes, uint32_t* hash);
};

struct SC::SystemDirectories
{
    static const int StaticPathSize = 1024 * sizeof(utf_char_t);

    SmallString<StaticPathSize> executableFile; // Full path (native encoding) to executable file including extension
    SmallString<StaticPathSize> applicationRootDirectory; // Full path to (native encoding) Application directory

    [[nodiscard]] bool init();
    [[nodiscard]] bool close();
};
