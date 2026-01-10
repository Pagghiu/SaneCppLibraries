// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"
#include "../Foundation/PrimitiveTypes.h"
#include "../Memory/Memory.h" // FixedAllocator
namespace SC
{
struct SC_COMPILER_EXPORT VirtualMemory;
struct SC_COMPILER_EXPORT VirtualAllocator;
} // namespace SC
//! @addtogroup group_memory
//! @{

/// @brief Reserves a contiguous slice of virtual memory committing just a portion of it.
///
/// This class is useful on 64-bit systems where the address space is so large that it's feasible reserving large chunks
/// of memory to commit and de-commit (shrink) as needed. @n
/// Reservation ensures that the returned address will not change and will be sized in multiples of system page size. @n
/// @note Memory must be committed in order to be read or written, occupying physical memory pages.
/// @warning This class has no defined destructor so memory MUST be released calling VirtualMemory::release
///
/// \snippet Tests/Libraries/Memory/VirtualMemoryTest.cpp VirtualMemorySnippet
struct SC::VirtualMemory
{
    VirtualMemory()                                = default;
    VirtualMemory(const VirtualMemory&)            = delete;
    VirtualMemory(VirtualMemory&&)                 = delete;
    VirtualMemory& operator=(const VirtualMemory&) = delete;
    VirtualMemory& operator=(VirtualMemory&&)      = delete;
    ~VirtualMemory() { release(); }

    /// @brief Round up the passed in size to system memory page size
    [[nodiscard]] static size_t roundUpToPageSize(size_t size);

    /// @brief Obtains system memory page size
    [[nodiscard]] static size_t getPageSize();

    /// @brief Reserves a large block of virtual memory of size maxCapacityInBytes
    /// @param maxCapacityInBytes Size of the reserved amount of virtual address space
    /// @note The actual memory reserved will be the rounded upper multiple of VirtualMemory::getPageSize
    [[nodiscard]] bool reserve(size_t maxCapacityInBytes);

    /// @brief Reclaims the entire virtual memory block (reserved with VirtualMemory::reserve)
    void release();

    /// @brief Ensures at least sizeInBytes to be committed / accessible from the large maxCapacityInBytes block
    /// @param sizeInBytes Indicates how much of the reserved virtual address space that must be accessible
    /// @note sizeInBytes must smaller than reservedBytes (previously reserved by VirtualMemory::reserve)
    [[nodiscard]] bool commit(size_t sizeInBytes);

    /// @brief Reclaims all unused pages past sizeInBytes (previously committed with VirtualMemory::commit)
    [[nodiscard]] bool decommit(size_t sizeInBytes);

    /// @brief Returns how many bytes are currently committed / accessible
    [[nodiscard]] size_t size() const { return committedBytes; }

    /// @brief Returns how many bytes are currently reserved
    [[nodiscard]] size_t capacity() const { return reservedBytes; }

    /// @brief Returns a pointer to the start of the reserved virtual memory
    [[nodiscard]] void* data() { return memory; }

    /// @brief Returns a pointer to the start of the reserved virtual memory
    [[nodiscard]] const void* data() const { return memory; }

  private:
    size_t reservedBytes  = 0; ///< Maximum amount of reserved memory that can be committed
    size_t committedBytes = 0; ///< Current amount of committed memory

    void* memory = nullptr; ///< Pointer to start of reserved memory
};

/// @brief A MemoryAllocator implementation based on a growable slice of VirtualMemory
struct SC::VirtualAllocator : public FixedAllocator
{
    VirtualAllocator(VirtualMemory& virtualMemory);

  protected:
    VirtualMemory& virtualMemory;

    virtual void* allocateImpl(const void* owner, size_t numBytes, size_t alignment) override;
    virtual void* reallocateImpl(void* memory, size_t numBytes) override;

    void syncFixedAllocator();
};

//! @}
