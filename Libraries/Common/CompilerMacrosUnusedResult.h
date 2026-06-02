// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_UNUSED_RESULT_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_UNUSED_RESULT_DEFINITION_H != 1
#error "CompilerMacrosUnusedResult.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_UNUSED_RESULT_DEFINITION_H 1 // Increment to indicate a new version of the file

/// Disables `unused-result` warning (due to ignoring a return value marked as `[[nodiscard]]`)
#if defined(__clang__)
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT                                                                         \
    _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wunused-result\"")                           \
        _Pragma("clang diagnostic ignored \"-Wunused-value\"")
#define SC_COMPILER_WARNING_POP_UNUSED_RESULT _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT                                                                         \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wunused-result\"")                               \
        _Pragma("GCC diagnostic ignored \"-Wunused-value\"")
#define SC_COMPILER_WARNING_POP_UNUSED_RESULT _Pragma("GCC diagnostic pop")
#else
#define SC_COMPILER_WARNING_PUSH_UNUSED_RESULT _Pragma("warning(push)") _Pragma("warning(disable : 4834 6031)")
#define SC_COMPILER_WARNING_POP_UNUSED_RESULT  _Pragma("warning(pop)")
#endif

#endif // SC_FOUNDATION_COMPILER_MACROS_UNUSED_RESULT_DEFINITION_H
