#pragma once
#include "Segment.h"

namespace SC
{
struct ArrayAllocator;
} // namespace SC

struct SC::ArrayAllocator
{
    [[nodiscard]] static SegmentHeader* reallocate(SegmentHeader* oldHeader, size_t newSize)
    {
        if (newSize <= oldHeader->sizeBytes)
        {
            return oldHeader;
        }
        return nullptr;
    }
    [[nodiscard]] static SegmentHeader* allocate(SegmentHeader* oldHeader, size_t numNewBytes, void* pself)
    {
        oldHeader->initDefaults();
        return oldHeader;
    }

    static void release(SegmentHeader* oldHeader) {}
};

namespace SC
{
template <typename T, int N>
using Array = Segment<ArrayAllocator, T, N>;
} // namespace SC
