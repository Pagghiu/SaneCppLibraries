// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/PrimitiveTypes.h"
namespace SC
{
//! @addtogroup group_foundation_utility
//! @{
struct Memory
{
    /// @brief Allocates numBytes bytes of memory
    /// @param numBytes Number of bytes to allocate
    /// @return Raw pointer to allocated memory, to be freed with Memory::release
    SC_COMPILER_EXPORT static void* allocate(size_t numBytes);

    /// @brief Change size of already allocated memory block. Existing contents of input buffer will be copied over.
    /// @param allocatedMemory pointer to memory previously allocated by Memory::allocate or Memory::Reallocate
    /// @param numBytes new size of the reallocated blck
    /// @return A new pointer of memory with size numBytes, to be freed with Memory::release
    SC_COMPILER_EXPORT static void* reallocate(void* allocatedMemory, size_t numBytes);

    /// @brief Free memory allocated by Memory::allocate and / or reallocated by Memory::reallocate
    /// @param allocatedMemory Memory to release / deallocate
    SC_COMPILER_EXPORT static void release(void* allocatedMemory);
};
//! @}
} // namespace SC
