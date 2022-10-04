#pragma once
#include "types.h"
namespace sanecpp
{
// Garanteed alignment at least 8
void* memoryAllocate(size_t numBytes);
void* memoryReallocate(void* allocatedMemory, size_t numBytes);
void  memoryRelease(void* allocatedMemory);
} // namespace sanecpp
