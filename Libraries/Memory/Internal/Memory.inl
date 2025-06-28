// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Memory/Globals.h"
#include "../../Memory/Memory.h"
#include <stdint.h> // uintptr_t

//------------------------------------------------------------------------------------------------------------------
// Memory
//------------------------------------------------------------------------------------------------------------------
// clang-format off
void* SC::Memory::reallocate(void* memory, size_t numBytes) { return Globals::get(Globals::Global).allocator.reallocate(memory, numBytes); }
void* SC::Memory::allocate(size_t numBytes, size_t alignment) { return Globals::get(Globals::Global).allocator.allocate(nullptr, numBytes,alignment); }
void  SC::Memory::release(void* allocatedMemory) { return Globals::get(Globals::Global).allocator.release(allocatedMemory); }
// clang-format on

//------------------------------------------------------------------------------------------------------------------
// FixedAllocator
//------------------------------------------------------------------------------------------------------------------
SC::FixedAllocator::FixedAllocator(void* memory, size_t capacityBytes) : memory(memory), capacityBytes(capacityBytes) {}

void* SC::FixedAllocator::allocateImpl(const void* owner, size_t numBytes, size_t alignment)
{
    if (owner != nullptr and (owner < memory or owner >= static_cast<char*>(memory) + capacityBytes))
        return nullptr;
    if (position + numBytes <= capacityBytes)
    {
        auto alignPointer = [](void* ptr, size_t alignment)
        {
            uintptr_t addr = (uintptr_t)ptr;
            // Round up to the next multiple of alignment
            uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
            return (void*)aligned_addr;
        };
        char* nextAllocation = static_cast<char*>(memory) + position;
        lastAllocation       = alignPointer(nextAllocation, alignment);
        size_t alignmentDiff = static_cast<size_t>(static_cast<char*>(lastAllocation) - nextAllocation);
        position += numBytes + alignmentDiff;
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
        else if (position + numBytes - lastAllocatedSize <= capacityBytes)
        {
            position += numBytes - lastAllocatedSize;
            lastAllocatedSize = numBytes;
            return allocatedMemory;
        }
    }
    size_t alignment = 1;
    if ((size_t(allocatedMemory) & (8 - 1)) == 0)
        alignment = 8;
    else if ((size_t(allocatedMemory) & (4 - 1)) == 0)
        alignment = 4;
    else if ((size_t(allocatedMemory) & (2 - 1)) == 0)
        alignment = 2;
    return allocate(allocatedMemory, numBytes, alignment);
}

void SC::FixedAllocator::releaseImpl(void*) {}
