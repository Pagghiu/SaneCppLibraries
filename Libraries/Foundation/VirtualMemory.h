// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Memory.h" // FixedAllocator
#include "../Foundation/PrimitiveTypes.h"
namespace SC
{
struct SC_COMPILER_EXPORT VirtualMemory;
struct SC_COMPILER_EXPORT VirtualAllocator;
} // namespace SC
//! @addtogroup group_foundation_utility
//! @{

/// @brief Reserves a contiguous slice of virtual memory committing just a portion of it.
///
/// This class is useful on 64-bit systems where the address space is so large that it's feasible reserving large chunks
/// of memory to commit and de-commit (shrink) as needed. @n
/// Reservation ensures that the returned address will not change and will be sized in multiples of system page size. @n
/// @note Memory must be committed in order to be read or written, occupying physical memory pages.
/// @warning This class has no defined destructor so memory MUST be released calling VirtualMemory::release
///
/// \snippet Libraries/Foundation/Tests/VirtualMemoryTest.cpp VirtualMemorySnippet
struct SC::VirtualMemory
{
    size_t reservedCapacityBytes  = 0; ///< Maximum amount of reserved memory that can be committed
    size_t committedCapacityBytes = 0; ///< Current amount of committed memory

    void* memory = nullptr; ///< Pointer to start of reserved memory

    /// @brief Round up the passed in size to system memory page size
    [[nodiscard]] static size_t roundUpToPageSize(size_t size);

    /// @brief Obtains system memory page size
    [[nodiscard]] static size_t getSystemPageSize();

    /// @brief Reserves a large block of virtual memory of size maxCapacityInBytes
    /// @param maxCapacityInBytes Size of the reserved amount of virtual address space
    /// @note The actual memory reserved will be the rounded upper multiple of VirtualMemory::getSystemPageSize
    [[nodiscard]] bool reserve(size_t maxCapacityInBytes);

    /// @brief Reclaims the entire virtual memory block (reserved with VirtualMemory::reserve)
    [[nodiscard]] bool release();

    /// @brief Ensures at least newCapacityBytes to be committed / accessible from the large maxCapacityInBytes block
    /// @param newCapacityBytes Indicates how much of the reserved virtual address space that must be accessible
    /// @note newCapacityBytes must smaller than reservedCapacityBytes (previously reserved by VirtualMemory::reserve)
    [[nodiscard]] bool commit(size_t newCapacityBytes);

    /// @brief Reclaims all unused pages past newCapacityBytes (previously committed with VirtualMemory::commit)
    [[nodiscard]] bool shrink(size_t newCapacityBytes);
};

/// @brief A MemoryAllocator implementation based on a growable slice of VirtualMemory
struct SC::VirtualAllocator : public FixedAllocator
{
    VirtualAllocator(VirtualMemory& virtualMemory);

  protected:
    VirtualMemory& virtualMemory;

    virtual void* allocateImpl(size_t numBytes) override;
    virtual void* reallocateImpl(void* memory, size_t numBytes) override;

    void syncFixedAllocator();
};

//! @}
