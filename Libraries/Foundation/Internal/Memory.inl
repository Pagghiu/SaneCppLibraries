// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Globals.h"
#include "../../Foundation/Memory.h"

//------------------------------------------------------------------------------------------------------------------
// Memory
//------------------------------------------------------------------------------------------------------------------
// clang-format off
void* SC::Memory::reallocate(void* memory, size_t numBytes) { return Globals::getGlobal().allocator.reallocate(memory, numBytes); }
void* SC::Memory::allocate(size_t numBytes) { return Globals::getGlobal().allocator.allocate(numBytes); }
void  SC::Memory::release(void* allocatedMemory) { return Globals::getGlobal().allocator.release(allocatedMemory); }
// clang-format on

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
