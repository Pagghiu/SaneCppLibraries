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
        oldHeader->initDefaults();
        return oldHeader;
    }

    static void release(SegmentHeader* oldHeader) {}

    template <typename T>
    static T* getItems(SegmentHeader* header)
    {
        return static_cast<T*>(
            static_cast<void*>(static_cast<char_t*>(static_cast<void*>(header)) + sizeof(SegmentHeader)));
    }
    template <typename T>
    static const T* getItems(const SegmentHeader* header)
    {
        return static_cast<T*>(static_cast<const void*>(static_cast<const char_t*>(static_cast<const void*>(header)) +
                                                        sizeof(SegmentHeader)));
    }
};

namespace SC
{
template <typename T, int N>
using Array = Segment<ArrayAllocator, T, N>;
} // namespace SC
