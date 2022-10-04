#pragma once
#include "compiler.h" // SANECPP_BREAK_DEBUGGER
#include "libc.h"     // exit
#include "os.h"       // printBacktrace
#include "platform.h" // SANECPP_DEBUG

namespace sanecpp
{
[[noreturn]] inline __attribute__((always_inline)) void SANECPP_UNREACHABLE() { __builtin_unreachable(); }
void printAssertion(const char_t* expression, const char_t* filename, const char_t* functionName, int lineNumber);
} // namespace sanecpp
#define SANECPP_RELEASE_ASSERT(e)                                                                                      \
    if (!(e)) [[unlikely]]                                                                                             \
    {                                                                                                                  \
        sanecpp::printAssertion(#e, __FILE__, __func__, __LINE__);                                                     \
        (void)sanecpp::os::printBacktrace();                                                                           \
        SANECPP_BREAK_DEBUGGER;                                                                                        \
        exit(-1);                                                                                                      \
    }
#if SANECPP_DEBUG
#define SANECPP_DEBUG_ASSERT(e) SANECPP_RELEASE_ASSERT(e)
#else
#define SANECPP_DEBUG_ASSERT(e) (void)0
#endif
