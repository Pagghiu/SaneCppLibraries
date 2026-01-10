// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Span.h"
#include "../Memory/VirtualMemory.h"

namespace SC
{
//! @addtogroup group_containers
//! @{

/// @brief An array using a large virtual memory reservation to keep stable addresses.
template <typename T>
struct VirtualArray
{
    VirtualArray() = default;
    VirtualArray(size_t maxCapacity) { SC_ASSERT_RELEASE(reserve(maxCapacity)); }
    ~VirtualArray() { clear(); }

    /// @brief Releases all reserved memory.
    /// @warning Does not call destructors of contained elements (use clear() for that).
    void release()
    {
        virtualMemory.release();
        capacityElements = 0;
        sizeElements     = 0;
    }

    /// @brief Clears the stable array by calling the destructor of each element.
    /// @note Does not release reserved memory (use release() for that).
    void clear()
    {
        T* items = data();
        while (sizeElements > 0)
        {
            sizeElements--;
            items[sizeElements].~T();
        }
    }

    /// @brief Clears the array calling destructors of each element and releases virtual memory.
    [[nodiscard]] bool clearAndDecommit()
    {
        clear();
        return decommit();
    }

    /// @brief De-commits unused-memory, while still keeping the original virtual reservation
    [[nodiscard]] bool decommit() { return virtualMemory.decommit(sizeElements * sizeof(T)); }

    /// @brief Resizes the stable array without initializing or calling destructors.
    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize)
    {
        if (not virtualMemory.commit(newSize * sizeof(T)))
            return false;
        sizeElements = newSize;
        return true;
    }

    /// @brief Resizes the stable array, calling constructors or destructors as needed.
    [[nodiscard]] bool resize(size_t newSize)
    {
        T*           items   = data();
        const size_t oldSize = sizeElements;
        if (newSize < oldSize)
        {
            size_t idx = oldSize;
            do
            {
                items[--idx].~T();
            } while (idx != newSize);
        }
        if (not resizeWithoutInitializing(newSize))
            return false;
        if (newSize > oldSize)
        {
            size_t idx = oldSize;
            do
            {
                placementNew(items[idx++]);
            } while (idx != newSize);
        }
        return true;
    }

    /// @brief Reserves memory for the stable array without initializing elements.
    [[nodiscard]] bool reserve(size_t maxNumElements)
    {
        if (maxNumElements <= capacityElements)
        {
            return true;
        }
        if (virtualMemory.reserve(sizeof(T) * maxNumElements))
        {
            capacityElements = maxNumElements;
            return true;
        }
        return false;
    }

    [[nodiscard]] size_t capacity() const { return capacityElements; }
    [[nodiscard]] size_t size() const { return sizeElements; }

    [[nodiscard]] T*       data() { return static_cast<T*>(virtualMemory.data()); }
    [[nodiscard]] const T* data() const { return static_cast<const T*>(virtualMemory.data); }

    [[nodiscard]] operator Span<T>() { return {data(), sizeElements}; }
    [[nodiscard]] operator Span<const T>() const { return {data(), sizeElements}; }

    [[nodiscard]] Span<T>       toSpan() { return {data(), sizeElements}; }
    [[nodiscard]] Span<const T> toSpan() const { return {data(), sizeElements}; }

  private:
    VirtualMemory virtualMemory;

    size_t sizeElements     = 0;
    size_t capacityElements = 0;
};
//! @}

} // namespace SC
