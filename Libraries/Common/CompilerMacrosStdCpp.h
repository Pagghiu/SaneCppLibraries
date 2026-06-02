// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#if defined(SC_FOUNDATION_COMPILER_MACROS_STD_CPP_DEFINITION_H)
#if SC_FOUNDATION_COMPILER_MACROS_STD_CPP_DEFINITION_H != 1
#error "CompilerMacrosStdCpp.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_MACROS_STD_CPP_DEFINITION_H 1 // Increment to indicate a new version of the file

#if defined(SC_COMPILER_ENABLE_STD_CPP)
#error                                                                                                                 \
    "SC_COMPILER_ENABLE_STD_CPP has been replaced. Standard C/C++ headers and C++ runtime linkage are enabled by default; define SC_INCLUDE_STD_CPP=0 and SC_PROVIDE_CPP_RUNTIME_SHIMS=1 to request the old no-stdlib mode."
#endif

#if !defined(SC_INCLUDE_STD_CPP)
#define SC_INCLUDE_STD_CPP 1
#elif SC_INCLUDE_STD_CPP != 0 && SC_INCLUDE_STD_CPP != 1
#error "SC_INCLUDE_STD_CPP must be 0 or 1."
#endif

#if !defined(SC_PROVIDE_CPP_RUNTIME_SHIMS)
#define SC_PROVIDE_CPP_RUNTIME_SHIMS 0
#elif SC_PROVIDE_CPP_RUNTIME_SHIMS != 0 && SC_PROVIDE_CPP_RUNTIME_SHIMS != 1
#error "SC_PROVIDE_CPP_RUNTIME_SHIMS must be 0 or 1."
#endif

#endif // SC_FOUNDATION_COMPILER_MACROS_STD_CPP_DEFINITION_H
