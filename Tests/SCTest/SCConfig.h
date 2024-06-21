// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Enables auto reflection in
// Issues so far when in C++ 14:
// - C-Arrays are not handled properly (they collapse to a single element)
// - Declaring a constructor on a struct will break the member detection
// Issue so far when in C++ 20:
// - Cannot auto-deduce structs that inherit from other
#if !defined(__clang_major__) || (__clang_major__ > 10)
#define SC_REFLECT_AUTOMATIC 1
#endif

#define SC_LANGUAGE_FORCE_STANDARD_CPP 14
