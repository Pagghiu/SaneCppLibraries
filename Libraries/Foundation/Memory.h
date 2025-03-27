// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"
namespace SC
{
struct SC_COMPILER_EXPORT Memory;
struct SC_COMPILER_EXPORT MemoryAllocator;
struct SC_COMPILER_EXPORT FixedAllocator;
} // namespace SC
//! @addtogroup group_foundation_utility
//! @{

/// @brief Centralized functions to allocate, reallocate and deallocate memory
struct SC::Memory
{
    /// @brief Allocates numBytes bytes of memory
    /// @param numBytes Number of bytes to allocate
    /// @param alignment Alignment of the returned memory
    /// @return Raw pointer to allocated memory, to be freed with Memory::release
    static void* allocate(size_t numBytes, size_t alignment);

    /// @brief Change size of already allocated memory block. Existing contents of input buffer will be copied over.
    /// @param memory pointer to memory previously allocated by Memory::allocate or Memory::Reallocate
    /// @param numBytes new size of the reallocated block
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

    /// @brief Allocate and construct an object of type `T` using this allocator
    template <typename T, typename... U>
    T* allocate(U&&... u)
    {
        T* t = reinterpret_cast<T*>(allocate(nullptr, sizeof(T), alignof(T)));
        if (t)
        {
            placementNew(*t, forward<U>(u)...);
        }
        return t;
    }

    /// @brief Allocates numBytes bytes of memory
    /// @param numBytes Number of bytes to allocate
    /// @return Raw pointer to allocated memory, to be freed with MemoryAllocator::release
    void* allocate(const void* owner, size_t numBytes, size_t alignment)
    {
        statistics.numAllocate += 1;
        return allocateImpl(owner, numBytes, alignment);
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

    /// @brief Allocate virtual function to be reimplemented
    /// @param owner Can be `nullptr` or an address belonging to a previous allocation of this allocator
    /// @param numBytes How many bytes to allocate
    /// @param alignment Alignment of the allocation
    /// @return `false` if the passed not-null owner doesn't belong to this allocator or the allocation itself fails
    virtual void* allocateImpl(const void* owner, size_t numBytes, size_t alignment) = 0;

    /// @brief Re-allocate virtual function to be reimplemented
    virtual void* reallocateImpl(void* memory, size_t numBytes) = 0;

    /// @brief Release virtual function to be reimplemented
    virtual void releaseImpl(void* memory) = 0;

    virtual ~MemoryAllocator() {}
};

/// @brief A MemoryAllocator implementation using a finite slice of memory
struct SC::FixedAllocator : public MemoryAllocator
{
    FixedAllocator(void* memory, size_t sizeInBytes);

  protected:
    // Not using a span here to avoid depending on Span<T> in this header
    void*  memory      = nullptr;
    size_t sizeInBytes = 0;

    void*  lastAllocation    = nullptr;
    size_t lastAllocatedSize = 0;
    size_t position          = 0;

    virtual void* allocateImpl(const void* owner, size_t numBytes, size_t alignment) override;
    virtual void* reallocateImpl(void* memory, size_t numBytes) override;
    virtual void  releaseImpl(void* memory) override;
};

//! @}
