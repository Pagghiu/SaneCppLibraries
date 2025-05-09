// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// The only reason why this file exists is to test that build system can handle spaces in files
#ifndef SC_SPACES_SPECIFIC_DEFINE
// This is a define specific to this file that has been created in SC-Build.cpp as a tests
#error "SC_SPACES_SPECIFIC_DEFINE should be defined on this file (only)"
#endif

// This causes "unused-parameter" warning on MSVC (4100) and Clang/GCC (-Wunused-parameter)
// but both warnings are being disabled with a per file compile flag inside SC-build.cpp
void function_with_unused_parameter(int a) {}
