#pragma once

// Compiler name
#define SC_CLANG 1
#define SC_GCC   0
#define SC_MSVC  0

// Compiler Attributes
#define SC_ATTRIBUTE_PRINTF(a, b) __attribute__((format(printf, a, b)))
#define SC_BREAK_DEBUGGER         __builtin_debugtrap()
#define SC_ALWAYS_INLINE          __attribute__((always_inline)) inline
