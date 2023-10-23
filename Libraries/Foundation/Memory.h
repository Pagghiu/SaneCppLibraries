// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/PrimitiveTypes.h"
namespace SC
{
struct Memory
{
    SC_COMPILER_EXPORT static void* allocate(size_t numBytes);
    SC_COMPILER_EXPORT static void* reallocate(void* allocatedMemory, size_t numBytes);
    SC_COMPILER_EXPORT static void  release(void* allocatedMemory);
};
} // namespace SC
