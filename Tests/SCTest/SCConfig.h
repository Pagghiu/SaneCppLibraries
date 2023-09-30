// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

// Enables auto reflection in
// Issues so far when in C++ 14:
// - C-Arrays are not handled properly (they collapse to a single element)
// - Declaring a constructor on a struct will break the member detection
// Issue so far when in C++ 20:
// - Cannot auto-deduce structs that inherit from other
#define SC_META_ENABLE_AUTO_REFLECTION 1
#define SC_LANGUAGE_FORCE_STANDARD_CPP 14
