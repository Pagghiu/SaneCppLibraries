#pragma once

// Compiler name
#define SANECPP_CLANG 1
#define SANECPP_GCC   0
#define SANECPP_MSVC  0

// Compiler Attributes
#define SANECPP_PRINTF_LIKE    __attribute__((format(printf, 1, 2)))
#define SANECPP_BREAK_DEBUGGER __builtin_debugtrap()
#define SANECPP_ALWAYS_INLINE  __attribute__((always_inline)) inline
