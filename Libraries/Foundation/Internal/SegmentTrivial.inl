// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Segment.inl"

void SC::detail::SegmentTrivial::destruct(Span<void>) {}

void SC::detail::SegmentTrivial::copyConstructAs(Span<void> data, Span<const void> value)
{
    if (value.sizeInBytes() == 1)
    {
        int intValue = 0;
        ::memcpy(&intValue, value.data(), 1);
        ::memset(data.data(), intValue, data.sizeInBytes());
    }
    else
    {
        const size_t valueSize   = value.sizeInBytes();
        const size_t numBytes    = data.sizeInBytes();
        char*        destData    = data.reinterpret_as_array_of<char>().data();
        const void*  sourceValue = value.data();
        for (size_t idx = 0; idx < numBytes; idx += valueSize)
        {
            ::memcpy(destData + idx, sourceValue, valueSize);
        }
    }
}

void SC::detail::SegmentTrivial::copyConstruct(Span<void> data, const void* src)
{
    ::memmove(data.data(), src, data.sizeInBytes());
}

void SC::detail::SegmentTrivial::copyAssign(Span<void> data, const void* src)
{
    ::memcpy(data.data(), src, data.sizeInBytes());
}

void SC::detail::SegmentTrivial::copyInsert(Span<void> data, Span<const void> values)
{
    ::memmove(data.reinterpret_as_array_of<char>().data() + values.sizeInBytes(), data.data(), data.sizeInBytes());
    ::memmove(data.data(), values.data(), values.sizeInBytes());
}

void SC::detail::SegmentTrivial::moveConstruct(Span<void> data, void* src)
{
    ::memcpy(data.data(), src, data.sizeInBytes());
}

void SC::detail::SegmentTrivial::moveAssign(Span<void> data, void* src)
{
    ::memcpy(data.data(), src, data.sizeInBytes());
}

void SC::detail::SegmentTrivial::remove(Span<void> data, size_t numElements)
{
    ::memmove(data.data(), data.reinterpret_as_array_of<char>().data() + numElements, numElements);
}
