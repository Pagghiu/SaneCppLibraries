#pragma once

// Compiler name
#define SC_CLANG 1
#define SC_GCC   0
#define SC_MSVC  0

// Compiler Attributes
#define SC_PRINTF_LIKE_FREE __attribute__((format(printf, 1, 2)))
#define SC_PRINTF_LIKE_MEM  __attribute__((format(printf, 2, 3)))
#define SC_BREAK_DEBUGGER   __builtin_debugtrap()
#define SC_ALWAYS_INLINE    __attribute__((always_inline)) inline
