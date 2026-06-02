// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_LIFETIME_BOUND_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_LIFETIME_BOUND_DEFINITION_H != 1
#error "CompilerMacrosLifetimeBound.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_LIFETIME_BOUND_DEFINITION_H 1 // Increment to indicate a new version of the file

#ifndef __has_cpp_attribute
#define SC_LANGUAGE_LIFETIME_BOUND
#elif __has_cpp_attribute(msvc::lifetimebound)
#define SC_LANGUAGE_LIFETIME_BOUND [[msvc::lifetimebound]]
#elif __has_cpp_attribute(clang::lifetimebound)
#define SC_LANGUAGE_LIFETIME_BOUND [[clang::lifetimebound]]
#elif __has_cpp_attribute(lifetimebound)
#define SC_LANGUAGE_LIFETIME_BOUND [[lifetimebound]]
#else
#define SC_LANGUAGE_LIFETIME_BOUND
#endif

#endif // SC_FOUNDATION_COMPILER_MACROS_LIFETIME_BOUND_DEFINITION_H
