// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Compiler.h" // SC_COMPILER_DEBUG_BREAK
#include "../Base/Compiler.h" // SC_LANGUAGE_UNLIKELY
#include "../Base/LibC.h"     // exit
#include "../Base/Platform.h" // SC_CONFIGURATION_DEBUG

namespace SC
{
struct Assert;
}

struct SC::Assert
{
    [[noreturn]] SC_COMPILER_FORCE_INLINE static void unreachable()
    {
#if SC_COMPILER_MSVC
        __assume(false);
#else
        __builtin_unreachable();
#endif
    }
    [[noreturn]] static void exit(int code);
    static void print(const char* expression, const char* filename, const char* functionName, int lineNumber);
    static void printAscii(const char* str);
    [[nodiscard]] static bool   printBacktrace();
    [[nodiscard]] static bool   printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes);
    [[nodiscard]] static size_t captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                                 size_t backtraceBufferSizeInBytes, uint32_t* hash);
};

#define SC_ASSERT_RELEASE(e)                                                                                           \
    if (!(e))                                                                                                          \
        SC_LANGUAGE_UNLIKELY                                                                                           \
        {                                                                                                              \
            SC::Assert::print(#e, __FILE__, __func__, __LINE__);                                                       \
            (void)SC::Assert::printBacktrace();                                                                        \
            SC_COMPILER_DEBUG_BREAK;                                                                                   \
            SC::Assert::exit(-1);                                                                                      \
        }                                                                                                              \
    (void)0

#if SC_CONFIGURATION_DEBUG
#define SC_ASSERT_DEBUG(e) SC_ASSERT_RELEASE(e)
#else
#define SC_ASSERT_DEBUG(e) (void)0
#endif
