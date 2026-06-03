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

#if SC_COMPILER_MSVC
#define SC_ASSERT_UNREACHABLE_IMPL() __assume(false)
#else
#define SC_ASSERT_UNREACHABLE_IMPL() __builtin_unreachable()
#endif

#define SC_DECLARE_ASSERT_PROVIDER(TypeName, ExportMacro)                                                              \
    struct ExportMacro TypeName                                                                                        \
    {                                                                                                                  \
        [[noreturn]] SC_COMPILER_FORCE_INLINE static void unreachable() { SC_ASSERT_UNREACHABLE_IMPL(); }              \
        [[noreturn]] static void                          exit(int code);                                              \
        static void printBacktrace(const char* expression, Result result, const native_char_t* filename,               \
                                   const char* function, int line);                                                    \
        static void printBacktrace(const char* expression, bool result, const native_char_t* filename,                 \
                                   const char* function, int line);                                                    \
                                                                                                                       \
      private:                                                                                                         \
        struct Internal;                                                                                               \
    }

#if SC_PLATFORM_WINDOWS
#define SC_ASSERT_WIDEN2(x)        L##x
#define SC_ASSERT_WIDEN(x)         SC_ASSERT_WIDEN2(x)
#define SC_ASSERT_NATIVE_FILE_NAME SC_ASSERT_WIDEN(__FILE__)
#else
#define SC_ASSERT_NATIVE_FILE_NAME __FILE__
#endif

#define SC_ASSERT_PROVIDER_RELEASE(Provider, e)                                                                        \
    if (!(e))                                                                                                          \
        SC_LANGUAGE_UNLIKELY                                                                                           \
        {                                                                                                              \
            Provider::printBacktrace(#e, e, SC_ASSERT_NATIVE_FILE_NAME, __func__, __LINE__);                           \
            SC_COMPILER_DEBUG_BREAK;                                                                                   \
            Provider::exit(-1);                                                                                        \
        }                                                                                                              \
    (void)0

#if SC_CONFIGURATION_DEBUG
#define SC_ASSERT_PROVIDER_DEBUG(Provider, e) SC_ASSERT_PROVIDER_RELEASE(Provider, e)
#else
#define SC_ASSERT_PROVIDER_DEBUG(Provider, e) (void)0
#endif

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Functions and macros to assert, exit() or abort() and capture backtraces.
SC_DECLARE_ASSERT_PROVIDER(Assert, SC_FOUNDATION_EXPORT);
//! @}

//! @addtogroup group_foundation_compiler_macros
//! @{
/// Assert expression `e` to be true. If Failed, prints the failed assertion with backtrace, breaks debugger and exits
/// (-1)
#define SC_FOUNDATION_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::Assert, e)

/// Assert expression `e` to be true. If Failed, prints the failed assertion with backtrace, breaks debugger and exits
/// (-1). Only active under SC_CONFIGURATION_DEBUG configuration, and defined to empty otherwise
#define SC_FOUNDATION_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::Assert, e)

/// @brief Asserts that the given result is valid
#define SC_FOUNDATION_TRUST_RESULT(expression) SC_FOUNDATION_ASSERT_RELEASE(expression)

#define SC_ASSERT_RELEASE(e)        SC_FOUNDATION_ASSERT_RELEASE(e)
#define SC_ASSERT_DEBUG(e)          SC_FOUNDATION_ASSERT_DEBUG(e)
#define SC_TRUST_RESULT(expression) SC_FOUNDATION_TRUST_RESULT(expression)

//! @}
} // namespace SC

#endif // SC_FOUNDATION_ASSERT_DEFINITION_H
