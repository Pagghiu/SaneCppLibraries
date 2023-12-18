// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Compiler.h" // SC_COMPILER_DEBUG_BREAK
#include "../Foundation/Compiler.h" // SC_LANGUAGE_UNLIKELY
#include "../Foundation/LibC.h"     // exit
#include "../Foundation/Platform.h" // SC_CONFIGURATION_DEBUG

namespace SC
{
struct Assert;
}
//! @addtogroup group_foundation_utility
//! @{

/// @brief Functions and macros to assert, exit() or abort() and capture backtraces.
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
    /// @brief Exits current process
    /// @param code Return code for calling process
    [[noreturn]] static void exit(int code);

    /// @brief Prints an assertion to standard output
    /// @param expression The failed assertion converted to string
    /// @param filename Name of the file where the assertion failed
    /// @param functionName Name of the function containing the assertion that failed
    /// @param lineNumber Line number where the assertion is defined
    static void print(const char* expression, const char* filename, const char* functionName, int lineNumber);

    /// @brief Prints an ASCII string to standard output
    /// @param str Pointer to ASCII string (no UTF8)
    static void printAscii(const char* str);

    /// @brief Prints backtrace (call stack) of the caller to standard output
    /// @return `true` if backtrace was correctly captured and print, `false` otherwise
    [[nodiscard]] static bool printBacktrace();

    /// @brief Prints backtrace (call stack) previously captured with captureBacktrace() of the caller to standard
    /// output
    /// @return `true` if backtrace was correctly captured and print, `false` otherwise
    [[nodiscard]] static bool printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes);

    /// @brief Captures backtrace of calling stack
    /// @param framesToSkip Number of call stack frames to skip
    /// @param backtraceBuffer A pre-allocated buffer to hold current backtrace
    /// @param backtraceBufferSizeInBytes Size of the backtraceBuffer
    /// @param hash Hash of current stack trace
    /// @return number of frames captured in backtraceBuffer
    [[nodiscard]] static size_t captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                                 size_t backtraceBufferSizeInBytes, uint32_t* hash);
};
//! @}

//! @addtogroup group_foundation_compiler_macros
//! @{

/// Assert expression `e` to be true. If Failed, prints the failed assertion with backtrace, breaks debugger and exits
/// (-1)
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

/// Assert expression `e` to be true. If Failed, prints the failed assertion with backtrace, breaks debugger and exits
/// (-1). Only active under SC_CONFIGURATION_DEBUG configuration, and defined to empty otherwise
#if SC_CONFIGURATION_DEBUG
#define SC_ASSERT_DEBUG(e) SC_ASSERT_RELEASE(e)
#else
#define SC_ASSERT_DEBUG(e) (void)0
#endif
//! @}
