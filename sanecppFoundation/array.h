#pragma once
#include "segment.h"

namespace sanecpp
{
struct arrayAllocator;
} // namespace sanecpp

struct sanecpp::arrayAllocator
{
    [[nodiscard]] static segmentHeader* reallocate(segmentHeader* oldHeader, size_t newSize)
    {
        if (newSize <= oldHeader->sizeBytes)
        {
            return oldHeader;
        }
        return nullptr;
    }
    [[nodiscard]] static segmentHeader* allocate(segmentHeader* oldHeader, size_t numNewBytes) { return oldHeader; }

    static void release(segmentHeader* oldHeader) {}
};

namespace sanecpp
{
template <typename T, int N>
using array = segment<arrayAllocator, T, N>;
} // namespace sanecpp
