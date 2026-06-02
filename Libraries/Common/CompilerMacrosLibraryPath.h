// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_LIBRARY_PATH_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_LIBRARY_PATH_DEFINITION_H != 1
#error "CompilerMacrosLibraryPath.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_LIBRARY_PATH_DEFINITION_H 1 // Increment to indicate a new version of the file

#define SC_COMPILER_MACRO_ESCAPE(x)     #x
#define SC_COMPILER_MACRO_TO_LITERAL(x) SC_COMPILER_MACRO_ESCAPE(x)

#if defined(SC_LIBRARY_ROOT)
#define SC_COMPILER_LIBRARY_PATH SC_COMPILER_MACRO_TO_LITERAL(SC_LIBRARY_ROOT)
#else
#define SC_COMPILER_LIBRARY_PATH ""
#endif

#endif // SC_FOUNDATION_COMPILER_MACROS_LIBRARY_PATH_DEFINITION_H
