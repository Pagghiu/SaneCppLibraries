// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

// Enables Auto reflection in C++14
// Issues so far:
// - C-Arrays are not handled properly (they collapse to a single element)
// - Declaring a constructor on a struct will break the member detection
// #define SC_META_ENABLE_CPP14_AUTO_REFLECTION 1

// #define SC_CPP_STANDARD_FORCE                14
#define SC_ENABLE_STD_CPP_LIBRARY 0
