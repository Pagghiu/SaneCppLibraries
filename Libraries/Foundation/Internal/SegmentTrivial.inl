// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Segment.inl"

inline void SC::SegmentTrivial::destruct(SegmentHeader&, size_t, size_t) {}

inline void SC::SegmentTrivial::copyConstructSingle(SegmentHeader& header, size_t offsetBytes, const void* value,
                                                    size_t numBytes, size_t valueSize)
{
    if (valueSize == 1)
    {
        int intValue = 0;
        ::memcpy(&intValue, value, 1);
        ::memset(header.getData<char>() + offsetBytes, intValue, numBytes);
    }
    else
    {
        char* data = header.getData<char>();
        for (size_t idx = offsetBytes; idx < offsetBytes + numBytes; idx += valueSize)
        {
            ::memcpy(data + idx, value, valueSize);
        }
    }
}

inline void SC::SegmentTrivial::copyConstruct(SegmentHeader& header, size_t offsetBytes, const void* src,
                                              size_t numBytes)
{
    ::memmove(header.getData<char>() + offsetBytes, src, numBytes);
}

inline void SC::SegmentTrivial::copyAssign(SegmentHeader& dest, size_t bytesOffset, const void* src, size_t numBytes)
{
    ::memcpy(dest.getData<char>() + bytesOffset, src, numBytes);
}

inline void SC::SegmentTrivial::copyInsert(SegmentHeader& dest, size_t bytesOffset, const void* src, size_t numBytes)
{
    char* data = dest.getData<char>();
    ::memmove(data + bytesOffset + numBytes, data + bytesOffset, dest.sizeBytes - bytesOffset);
    ::memmove(data + bytesOffset, src, numBytes);
}

inline void SC::SegmentTrivial::moveConstruct(SegmentHeader& dest, size_t bytesOffset, void* src, size_t numBytes)
{
    ::memcpy(dest.getData<char>() + bytesOffset, src, numBytes);
}

inline void SC::SegmentTrivial::moveAssign(SegmentHeader& dest, size_t bytesOffset, void* src, size_t numBytes)
{
    ::memcpy(dest.getData<char>() + bytesOffset, src, numBytes);
}

inline void SC::SegmentTrivial::remove(SegmentHeader& dest, size_t fromBytesOffset, size_t toBytesOffset)
{
    char* data = dest.getData<char>();
    ::memmove(data + fromBytesOffset, data + toBytesOffset, dest.sizeBytes - toBytesOffset);
}

inline SC::SegmentHeader* SC::SegmentAllocator::allocateNewHeader(size_t newCapacityInBytes)
{
    return reinterpret_cast<SegmentHeader*>(Memory::allocate(sizeof(SegmentHeader) + newCapacityInBytes));
}

inline SC::SegmentHeader* SC::SegmentAllocator::reallocateExistingHeader(SegmentHeader& src, size_t newCapacityInBytes)
{
    return reinterpret_cast<SegmentHeader*>(Memory::reallocate(&src, sizeof(SegmentHeader) + newCapacityInBytes));
}

inline void SC::SegmentAllocator::destroyHeader(SegmentHeader& header) { Memory::release(&header); }
