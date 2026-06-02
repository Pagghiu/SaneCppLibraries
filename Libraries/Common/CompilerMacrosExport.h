// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_EXPORT_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_EXPORT_DEFINITION_H != 1
#error "CompilerMacrosExport.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_EXPORT_DEFINITION_H 1 // Increment to indicate a new version of the file

/// Define compiler-specific visibility macros.
#if defined(_MSC_VER)
#define SC_COMPILER_SYMBOL_IMPORT __declspec(dllimport)
#define SC_COMPILER_SYMBOL_EXPORT __declspec(dllexport)
#define SC_COMPILER_SYMBOL_HIDDEN
#else
#define SC_COMPILER_SYMBOL_IMPORT
#define SC_COMPILER_SYMBOL_EXPORT __attribute__((visibility("default")))
#define SC_COMPILER_SYMBOL_HIDDEN __attribute__((visibility("hidden")))
#endif

#if defined(SC_PLUGIN_LIBRARY)
#define SC_COMPILER_EXTERN extern
#else
#define SC_COMPILER_EXTERN
#endif

#if defined(SC_PLUGIN_LIBRARY)
#define SC_COMPILER_LIBRARY_EXPORT_0 SC_COMPILER_SYMBOL_IMPORT
#define SC_COMPILER_LIBRARY_EXPORT_1 SC_COMPILER_SYMBOL_IMPORT
#else
#define SC_COMPILER_LIBRARY_EXPORT_0 SC_COMPILER_SYMBOL_HIDDEN
#define SC_COMPILER_LIBRARY_EXPORT_1 SC_COMPILER_SYMBOL_EXPORT
#endif
#define SC_COMPILER_LIBRARY_EXPORT(value)      SC_COMPILER_LIBRARY_EXPORT_IMPL(value)
#define SC_COMPILER_LIBRARY_EXPORT_IMPL(value) SC_COMPILER_LIBRARY_EXPORT_##value

#ifndef SC_EXPORT_LIBRARY_FOUNDATION
#define SC_EXPORT_LIBRARY_FOUNDATION 0
#endif
#define SC_FOUNDATION_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_FOUNDATION)

#endif // SC_FOUNDATION_COMPILER_MACROS_EXPORT_DEFINITION_H
