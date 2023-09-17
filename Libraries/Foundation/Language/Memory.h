// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Language/Types.h"
namespace SC
{
// Garanteed alignment at least 8
SC_EXPORT_SYMBOL void* memoryAllocate(size_t numBytes);
SC_EXPORT_SYMBOL void* memoryReallocate(void* allocatedMemory, size_t numBytes);
SC_EXPORT_SYMBOL void  memoryRelease(void* allocatedMemory);
} // namespace SC
