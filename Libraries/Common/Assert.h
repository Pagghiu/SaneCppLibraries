// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_ASSERT_DEFINITION_H
#if SC_FOUNDATION_ASSERT_DEFINITION_H != 1
#error "Assert.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_ASSERT_DEFINITION_H 1 // Increment to indicate a new version of the file

#include "CompilerMacrosDebugBreak.h"
#include "CompilerMacrosExport.h"
#include "CompilerMacrosInline.h"
#include "CompilerMacrosStdVersion.h"
#include "CompilerMacrosType.h"
#include "PlatformMacrosDebug.h"
#include "PlatformMacrosInstructionSet.h"
#include "PlatformMacrosType.h"
#include "PrimitiveDefinitions.h"
#include "Result.h"

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Functions and macros to assert, exit() or abort() and capture backtraces.
struct SC_FOUNDATION_EXPORT Assert
{
    [[noreturn]] SC_COMPILER_FORCE_INLINE static void unreachable()
    {
#if SC_COMPILER_MSVC
        __assume(false);
#else
        __builtin_unreachable();
#endif
    }
    /// @brief Exits current process with the given code
    [[noreturn]] static void exit(int code);

    /// @brief Prints an assertion to standard output
    static void printBacktrace(const char* expression, Result result, const native_char_t* filename,
                               const char* function, int line);
    static void printBacktrace(const char* expression, bool result, const native_char_t* filename, const char* function,
                               int line);

  private:
    struct Internal;
};
//! @}

//! @addtogroup group_foundation_compiler_macros
//! @{
#if SC_PLATFORM_WINDOWS
#define SC_ASSERT_WIDEN2(x)        L##x
#define SC_ASSERT_WIDEN(x)         SC_ASSERT_WIDEN2(x)
#define SC_ASSERT_NATIVE_FILE_NAME SC_ASSERT_WIDEN(__FILE__)
#else
#define SC_ASSERT_NATIVE_FILE_NAME __FILE__
#endif
/// Assert expression `e` to be true. If Failed, prints the failed assertion with backtrace, breaks debugger and exits
/// (-1)
#define SC_ASSERT_RELEASE(e)                                                                                           \
    if (!(e))                                                                                                          \
        SC_LANGUAGE_UNLIKELY                                                                                           \
        {                                                                                                              \
            SC::Assert::printBacktrace(#e, e, SC_ASSERT_NATIVE_FILE_NAME, __func__, __LINE__);                         \
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

/// @brief Asserts that the given result is valid
#define SC_TRUST_RESULT(expression) SC_ASSERT_RELEASE(expression)

//! @}
} // namespace SC

#endif // SC_FOUNDATION_ASSERT_DEFINITION_H
