// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Language/Compiler.h" // SC_BREAK_DEBUGGER
#include "../Language/Language.h" // SC_UNLIKELY
#include "../Language/LibC.h"     // exit
#include "../Language/Platform.h" // SC_DEBUG

namespace SC
{
#if SC_MSVC
[[noreturn]] __forceinline void SC_UNREACHABLE() { __assume(false); }
#else
[[noreturn]] SC_ALWAYS_INLINE void SC_UNREACHABLE() { __builtin_unreachable(); }
#endif
void printAssertion(const char* expression, const char* filename, const char* functionName, int lineNumber);
[[nodiscard]] bool printBacktrace();
SC_NO_RETURN(void exit(int code));

} // namespace SC

#define SC_RELEASE_ASSERT(e)                                                                                           \
    if (!(e))                                                                                                          \
        SC_UNLIKELY                                                                                                    \
        {                                                                                                              \
            SC::printAssertion(#e, __FILE__, __func__, __LINE__);                                                      \
            (void)SC::printBacktrace();                                                                                \
            SC_BREAK_DEBUGGER;                                                                                         \
            SC::exit(-1);                                                                                              \
        }
#if SC_DEBUG
#define SC_DEBUG_ASSERT(e) SC_RELEASE_ASSERT(e)
#else
#define SC_DEBUG_ASSERT(e) (void)0
#endif
