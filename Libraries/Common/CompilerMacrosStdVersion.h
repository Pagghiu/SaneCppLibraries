// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_STD_VERSION_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_STD_VERSION_DEFINITION_H != 1
#error "CompilerMacrosStdVersion.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_STD_VERSION_DEFINITION_H 1 // Increment to indicate a new version of the file

/// `SC_LANGUAGE_FORCE_STANDARD_CPP` == `14`, `17` or `20` to force a specific C++ standard version.
#if defined(SC_LANGUAGE_FORCE_STANDARD_CPP)
#if SC_LANGUAGE_FORCE_STANDARD_CPP == 14
#define SC_LANGUAGE_CPP_VERSION 201402L
#elif SC_LANGUAGE_FORCE_STANDARD_CPP == 17
#define SC_LANGUAGE_CPP_VERSION 201703L
#elif SC_LANGUAGE_FORCE_STANDARD_CPP == 20
#define SC_LANGUAGE_CPP_VERSION 202002L
#else
#error "SC_LANGUAGE_FORCE_STANDARD_CPP has invalid value"
#endif
#else

/// Macro to check `__cplusplus` properly on MSVC
#if defined(_MSC_VER) && !defined(__clang__)
#define SC_LANGUAGE_CPP_VERSION _MSVC_LANG
#else
#define SC_LANGUAGE_CPP_VERSION __cplusplus
#endif

#endif

#if SC_LANGUAGE_CPP_VERSION >= 202002L

#define SC_LANGUAGE_CPP_AT_LEAST_20 1
#define SC_LANGUAGE_CPP_AT_LEAST_17 1
#define SC_LANGUAGE_CPP_AT_LEAST_14 1

#elif SC_LANGUAGE_CPP_VERSION >= 201703L

#define SC_LANGUAGE_CPP_AT_LEAST_20 0
#define SC_LANGUAGE_CPP_AT_LEAST_17 1
#define SC_LANGUAGE_CPP_AT_LEAST_14 1

#elif SC_LANGUAGE_CPP_VERSION >= 201402L

#define SC_LANGUAGE_CPP_AT_LEAST_20 0
#define SC_LANGUAGE_CPP_AT_LEAST_17 0
#define SC_LANGUAGE_CPP_AT_LEAST_14 1

#else

#define SC_LANGUAGE_CPP_AT_LEAST_20 0 ///< True (1) if C++ standard is <= C++ 20
#define SC_LANGUAGE_CPP_AT_LEAST_17 0 ///< True (1) if C++ standard is <= C++ 17
#define SC_LANGUAGE_CPP_AT_LEAST_14 0 ///< True (1) if C++ standard is <= C++ 14

#endif

#undef SC_LANGUAGE_CPP_VERSION

#if SC_LANGUAGE_CPP_AT_LEAST_20
#define SC_LANGUAGE_LIKELY   [[likely]]   ///<  Use `[[likely]]` if available
#define SC_LANGUAGE_UNLIKELY [[unlikely]] ///<  Use `[[unlikely]]` if available
#else
#define SC_LANGUAGE_LIKELY
#define SC_LANGUAGE_UNLIKELY
#endif

#if SC_LANGUAGE_CPP_AT_LEAST_17
#define SC_LANGUAGE_IF_CONSTEXPR constexpr
#else
#define SC_LANGUAGE_IF_CONSTEXPR
#endif

#endif // SC_FOUNDATION_COMPILER_MACROS_STD_VERSION_DEFINITION_H
