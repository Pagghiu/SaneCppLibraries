// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Memory.h"

//------------------------------------------------------------------------------------------------------------------
// Memory
//------------------------------------------------------------------------------------------------------------------
void* SC::Memory::reallocate(void* memory, size_t numBytes) { return ::realloc(memory, numBytes); }
void* SC::Memory::allocate(size_t numBytes) { return ::malloc(numBytes); }
void  SC::Memory::release(void* allocatedMemory) { return ::free(allocatedMemory); }
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
void SC::Memory::registerGlobals()
{
    ::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // ::_CrtSetBreakAlloc();
}
#else
void SC::Memory::registerGlobals() {}
#endif
//------------------------------------------------------------------------------------------------------------------
// FixedAllocator
//------------------------------------------------------------------------------------------------------------------
SC::FixedAllocator::FixedAllocator(void* memory, size_t sizeInBytes) : memory(memory), sizeInBytes(sizeInBytes) {}

void* SC::FixedAllocator::allocateImpl(size_t numBytes)
{
    // TODO: Consider alignment before returning memory
    if (position + numBytes <= sizeInBytes)
    {
        lastAllocation = static_cast<char*>(memory) + position;
        position += numBytes;
        lastAllocatedSize = numBytes;
        return lastAllocation;
    }
    return nullptr;
}

void* SC::FixedAllocator::reallocateImpl(void* allocatedMemory, size_t numBytes)
{
    // TODO: Consider alignment before returning memory
    if (allocatedMemory == lastAllocation)
    {
        if (numBytes < lastAllocatedSize)
        {
            position -= lastAllocatedSize - numBytes;
            lastAllocatedSize = numBytes;
            return allocatedMemory;
        }
        else if (position + numBytes - lastAllocatedSize <= sizeInBytes)
        {
            position += numBytes - lastAllocatedSize;
            lastAllocatedSize = numBytes;
            return allocatedMemory;
        }
    }
    return allocate(numBytes);
}

void SC::FixedAllocator::releaseImpl(void*) {}
