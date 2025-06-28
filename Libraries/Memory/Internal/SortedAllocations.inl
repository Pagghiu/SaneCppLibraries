#pragma once
#include "../../Foundation/LibC.h"

namespace SC
{
struct SortedAllocations
{
    struct Allocation
    {
        void*  allocation;
        size_t allocationSize;
    };

    size_t count;
    size_t capacity;
#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200) // zero-sized arrays are not-standard
#endif
    Allocation items[];
#if _MSC_VER
#pragma warning(pop)
#endif
    SortedAllocations(size_t capacity) : count(0), capacity(capacity) {}

    // Initialize the Span memory to hold SortedAllocations.
    static void init(void* data, size_t dataSizeInBytes)
    {
        const size_t       capacity    = (dataSizeInBytes - sizeof(SortedAllocations)) / sizeof(Allocation);
        SortedAllocations& allocations = *reinterpret_cast<SortedAllocations*>(data);
        placementNew(allocations, capacity);
        placementNewArray(allocations.items, capacity);
    }

    // Binary search: find the index of the allocation with the largest starting address
    // that is <= the given pointer. Returns false if none is found.
    bool findIndex(void* ptr, size_t& index)
    {
        if (count == 0)
            return false;

        bool found = false;

        size_t lo = 0;
        size_t hi = count - 1;
        while (lo <= hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (items[mid].allocation <= ptr)
            {
                found = true;
                index = mid;
                lo    = mid + 1;
            }
            else
            {
                hi = mid - 1;
            }
        }
        return found;
    }

    // Insert an allocation record while maintaining sorted order.
    bool insertSorted(Allocation allocation)
    {
        if (count >= capacity)
            return false; // no space

        // Find position to insert such that the allocations remains sorted.
        size_t pos = count;
        for (size_t i = 0; i < count; i++)
        {
            if (items[i].allocation > allocation.allocation)
            {
                pos = i;
                break;
            }
        }
        // Shift items right to make space
        ::memmove(items + pos + 1, items + pos, (count - pos) * sizeof(Allocation));
        items[pos] = allocation;
        count++;
        return true;
    }

    // Remove an allocation record (by exact pointer match) and maintain sorted order.
    bool removeSorted(void* allocationPointer)
    {
        if (count == 0)
            return false;
        // Binary search to find an entry with the matching pointer
        size_t lo = 0;
        size_t hi = count - 1;
        while (lo <= hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (items[mid].allocation == allocationPointer)
            {
                // Found: shift left to remove
                ::memmove(items + mid, items + mid + 1, (count - mid - 1) * sizeof(Allocation));
                count--;
                return true;
            }
            else if (items[mid].allocation < allocationPointer)
            {
                lo = mid + 1;
            }
            else
            {
                hi = mid - 1;
            }
        }
        return false; // not found
    }
};
} // namespace SC
