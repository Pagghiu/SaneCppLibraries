// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Compiler.h" // SC_BREAK_DEBUGGER
#include "../Base/Language.h" // SC_UNLIKELY
#include "../Base/LibC.h"     // exit
#include "../Base/Platform.h" // SC_DEBUG

namespace SC
{
struct Assert;
}

struct SC::Assert
{
    [[noreturn]] SC_ALWAYS_INLINE static void unreachable()
    {
#if SC_COMPILER_MSVC
        __assume(false);
#else
        __builtin_unreachable();
#endif
    }
    static void print(const char* expression, const char* filename, const char* functionName, int lineNumber);
    static void printAscii(const char* str);
    [[nodiscard]] static bool   printBacktrace();
    [[nodiscard]] static bool   printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes);
    [[nodiscard]] static size_t captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                                 size_t backtraceBufferSizeInBytes, uint32_t* hash);
    SC_NO_RETURN(static void exit(int code));
};

#define SC_RELEASE_ASSERT(e)                                                                                           \
    if (!(e))                                                                                                          \
        SC_UNLIKELY                                                                                                    \
        {                                                                                                              \
            SC::Assert::print(#e, __FILE__, __func__, __LINE__);                                                       \
            (void)SC::Assert::printBacktrace();                                                                        \
            SC_BREAK_DEBUGGER;                                                                                         \
            SC::Assert::exit(-1);                                                                                      \
        }                                                                                                              \
    (void)0

#if SC_DEBUG
#define SC_DEBUG_ASSERT(e) SC_RELEASE_ASSERT(e)
#else
#define SC_DEBUG_ASSERT(e) (void)0
#endif
