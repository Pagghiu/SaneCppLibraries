// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"
namespace SC
{
struct SC_COMPILER_EXPORT Memory;
struct SC_COMPILER_EXPORT MemoryAllocator;
} // namespace SC
//! @addtogroup group_foundation_utility
//! @{

/// @brief Centralized functions to allocate, reallocate and deallocate memory
struct SC::Memory
{
    /// @brief Allocates numBytes bytes of memory
    /// @param numBytes Number of bytes to allocate
    /// @return Raw pointer to allocated memory, to be freed with Memory::release
    static void* allocate(size_t numBytes);

    /// @brief Change size of already allocated memory block. Existing contents of input buffer will be copied over.
    /// @param memory pointer to memory previously allocated by Memory::allocate or Memory::Reallocate
    /// @param numBytes new size of the reallocated blck
    /// @return A new pointer of memory with size numBytes, to be freed with Memory::release
    static void* reallocate(void* memory, size_t numBytes);

    /// @brief Free memory allocated by Memory::allocate and / or reallocated by Memory::reallocate
    /// @param memory Memory to release / deallocate
    static void release(void* memory);
};

/// @brief Customizable functions to allocate, reallocate and deallocate memory
struct SC::MemoryAllocator
{
    /// @brief Holds Statistics about how many allocations/release have been issued
    struct Statistics
    {
        size_t numAllocate   = 0; ///< How many times MemoryAllocator::allocate has been called
        size_t numReallocate = 0; ///< How many times MemoryAllocator::reallocate has been called
        size_t numRelease    = 0; ///< How many times MemoryAllocator::release has been called
    };
    Statistics statistics; ///< Holds statistics about how many allocations/release have been issued

    /// @brief Allocates numBytes bytes of memory
    /// @param numBytes Number of bytes to allocate
    /// @return Raw pointer to allocated memory, to be freed with MemoryAllocator::release
    void* allocate(size_t numBytes)
    {
        statistics.numAllocate += 1;
        return allocateImpl(numBytes);
    }

    /// @brief Change size of already allocated memory block. Existing contents of input buffer will be copied over.
    /// @param memory pointer to memory previously allocated by Memory::allocate or Memory::Reallocate
    /// @param numBytes new size of the reallocated block
    /// @return A new pointer of memory with size numBytes, to be freed with MemoryAllocator::release
    void* reallocate(void* memory, size_t numBytes)
    {
        statistics.numReallocate += 1;
        return reallocateImpl(memory, numBytes);
    }

    /// @brief Free memory allocated by MemoryAllocator::allocate and / or reallocated by MemoryAllocator::reallocate
    /// @param memory Memory to release / deallocate
    void release(void* memory)
    {
        if (memory != nullptr)
        {
            statistics.numRelease += 1;
        }
        return releaseImpl(memory);
    }

    void* userData = nullptr;

    virtual void* allocateImpl(size_t numBytes)                 = 0;
    virtual void* reallocateImpl(void* memory, size_t numBytes) = 0;
    virtual void  releaseImpl(void* memory)                     = 0;

    virtual ~MemoryAllocator() {}
};

//! @}
