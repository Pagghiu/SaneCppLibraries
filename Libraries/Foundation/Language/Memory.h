// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Language/Types.h"
namespace SC
{
// Garanteed alignment at least 8
struct Memory
{
    SC_EXPORT_SYMBOL static void* allocate(size_t numBytes);
    SC_EXPORT_SYMBOL static void* reallocate(void* allocatedMemory, size_t numBytes);
    SC_EXPORT_SYMBOL static void  release(void* allocatedMemory);
};
} // namespace SC
