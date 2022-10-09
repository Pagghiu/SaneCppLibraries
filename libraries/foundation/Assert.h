#pragma once
#include "Compiler.h" // SC_BREAK_DEBUGGER
#include "LibC.h"     // exit
#include "OS.h"       // printBacktrace
#include "Platform.h" // SC_DEBUG

namespace SC
{
[[noreturn]] inline __attribute__((always_inline)) void SC_UNREACHABLE() { __builtin_unreachable(); }
void printAssertion(const char_t* expression, const char_t* filename, const char_t* functionName, int lineNumber);
} // namespace SC
#define SC_RELEASE_ASSERT(e)                                                                                           \
    if (!(e)) [[unlikely]]                                                                                             \
    {                                                                                                                  \
        SC::printAssertion(#e, __FILE__, __func__, __LINE__);                                                          \
        (void)SC::OS::printBacktrace();                                                                                \
        SC_BREAK_DEBUGGER;                                                                                             \
        exit(-1);                                                                                                      \
    }
#if SC_DEBUG
#define SC_DEBUG_ASSERT(e) SC_RELEASE_ASSERT(e)
#else
#define SC_DEBUG_ASSERT(e) (void)0
#endif
