// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"
namespace SC
{
struct Memory;
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
    SC_COMPILER_EXPORT static void* allocate(size_t numBytes);

    /// @brief Change size of already allocated memory block. Existing contents of input buffer will be copied over.
    /// @param memory pointer to memory previously allocated by Memory::allocate or Memory::Reallocate
    /// @param numBytes new size of the reallocated blck
    /// @return A new pointer of memory with size numBytes, to be freed with Memory::release
    SC_COMPILER_EXPORT static void* reallocate(void* memory, size_t numBytes);

    /// @brief Free memory allocated by Memory::allocate and / or reallocated by Memory::reallocate
    /// @param memory Memory to release / deallocate
    SC_COMPILER_EXPORT static void release(void* memory);

    /// @brief Registers globals for the memory systems
    SC_COMPILER_EXPORT static void registerGlobals();
};

/// @brief Customizable functions to allocate, reallocate and deallocate memory
struct SC::MemoryAllocator
{
    /// @brief Allocates numBytes bytes of memory
    /// @param numBytes Number of bytes to allocate
    /// @return Raw pointer to allocated memory, to be freed with MemoryAllocator::release
    virtual void* allocate(size_t numBytes) { return Memory::allocate(numBytes); }

    /// @brief Change size of already allocated memory block. Existing contents of input buffer will be copied over.
    /// @param memory pointer to memory previously allocated by Memory::allocate or Memory::Reallocate
    /// @param numBytes new size of the reallocated block
    /// @return A new pointer of memory with size numBytes, to be freed with MemoryAllocator::release
    virtual void* reallocate(void* memory, size_t numBytes) { return Memory::reallocate(memory, numBytes); }

    /// @brief Free memory allocated by MemoryAllocator::allocate and / or reallocated by MemoryAllocator::reallocate
    /// @param memory Memory to release / deallocate
    virtual void release(void* memory) { Memory::release(memory); }

    virtual ~MemoryAllocator() {}
};

//! @}
