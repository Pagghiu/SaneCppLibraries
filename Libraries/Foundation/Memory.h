// Copyright (c) 2022, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Types.h"
namespace SC
{
// Garanteed alignment at least 8
void* memoryAllocate(size_t numBytes);
void* memoryReallocate(void* allocatedMemory, size_t numBytes);
void  memoryRelease(void* allocatedMemory);
} // namespace SC
