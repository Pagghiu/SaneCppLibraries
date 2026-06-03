// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Compiler.h"
#ifndef SC_EXPORT_LIBRARY_MEMORY
#define SC_EXPORT_LIBRARY_MEMORY 0
#endif
#define SC_MEMORY_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_MEMORY)

#include "../Common/Assert.h"
#include "../Foundation/PrimitiveTypes.h"
#if SC_COMPILER_FILC
#include <stdfil.h>
#endif
namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(MemoryAssert, SC_MEMORY_EXPORT);

#define SC_MEMORY_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::MemoryAssert, e)
#define SC_MEMORY_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::MemoryAssert, e)
#define SC_MEMORY_TRUST_RESULT(expression) SC_MEMORY_ASSERT_RELEASE(expression)

struct SC_MEMORY_EXPORT Memory;
struct SC_MEMORY_EXPORT MemoryAllocator;
struct SC_MEMORY_EXPORT FixedAllocator;
} // namespace SC
//! @addtogroup group_memory
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

    /// @brief Move numBytes bytes of memory from src to dst. The memory areas may overlap.
    static void move(void* dst, const void* src, size_t numBytes);

    /// @brief Set numBytes bytes of memory at dst to the value c
    static void set(void* dst, int c, size_t numBytes);

    /// @brief Copy numBytes bytes of memory from src to dst. The memory areas must not overlap.
    static void copy(void* dst, const void* src, size_t numBytes);
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
    T* create(U&&... u)
    {
        void* rawMemory = allocate(nullptr, sizeof(T), alignof(T));
        if (rawMemory)
        {
#if SC_COMPILER_FILC
            const size_t pointerAlignment = sizeof(void*);
            const size_t address          = reinterpret_cast<size_t>(rawMemory);
            if ((address & (pointerAlignment - 1)) == 0 and (sizeof(T) & (pointerAlignment - 1)) == 0)
            {
                zsetcap(rawMemory, rawMemory, sizeof(T));
            }
#endif
            T* t = reinterpret_cast<T*>(rawMemory);
            placementNew(*t, forward<U>(u)...);
            return t;
        }
        return nullptr;
    }

    /// @brief Allocates numBytes bytes of memory
    /// @param owner Memory address of the object that "owns" this allocation.
    /// @param numBytes Number of bytes to allocate
    /// @param alignment Alignment of the allocated block of memory
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
    FixedAllocator(void* memory, size_t capacityBytes);

    const void* data() const { return memory; }

    size_t size() const { return position; }
    size_t capacity() const { return capacityBytes; }

  protected:
    // Not using a span here to avoid depending on Span<T> in this header
    void*  memory        = nullptr;
    size_t capacityBytes = 0;

    void*  lastAllocation    = nullptr;
    size_t lastAllocatedSize = 0;
    size_t position          = 0;

    virtual void* allocateImpl(const void* owner, size_t numBytes, size_t alignment) override;
    virtual void* reallocateImpl(void* memory, size_t numBytes) override;
    virtual void  releaseImpl(void* memory) override;
};

//! @}
