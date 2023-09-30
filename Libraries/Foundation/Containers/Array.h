// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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
        SC_COMPILER_UNUSED(numNewBytes);
        SC_COMPILER_UNUSED(pself);
        oldHeader->initDefaults();
        return oldHeader;
    }

    static void release(SegmentHeader* oldHeader) { SC_COMPILER_UNUSED(oldHeader); }

    template <typename T>
    static T* getItems(SegmentHeader* header)
    {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(header) + sizeof(SegmentHeader));
    }
    template <typename T>
    static const T* getItems(const SegmentHeader* header)
    {
        return reinterpret_cast<const T*>(reinterpret_cast<const char*>(header) + sizeof(SegmentHeader));
    }
};

namespace SC
{
template <typename T, int N>
using Array = Segment<ArrayAllocator, T, N>;
} // namespace SC
