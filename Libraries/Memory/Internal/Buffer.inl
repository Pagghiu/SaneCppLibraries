// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Memory/Buffer.h"
#include "../../Memory/Internal/Segment.inl"
#include "../../Memory/Internal/SegmentTrivial.inl"

namespace SC
{
template struct Segment<detail::SegmentBuffer>;
// Explicit instantiation of these methods because they are templated at method level
template bool Segment<detail::SegmentBuffer>::assign<char>(Span<const char>) noexcept;
template bool Segment<detail::SegmentBuffer>::append<char>(Span<const char>) noexcept;

GrowableBuffer<Buffer>::GrowableBuffer(Buffer& buffer) : buffer(buffer)
{
    IGrowableBuffer::directAccess = {buffer.size(), buffer.capacity(), buffer.data()};
}

GrowableBuffer<Buffer>::~GrowableBuffer()
{
    if (buffer.size() != IGrowableBuffer::directAccess.sizeInBytes)
    {
        (void)buffer.resizeWithoutInitializing(IGrowableBuffer::directAccess.sizeInBytes);
    }
}

bool GrowableBuffer<Buffer>::tryGrowTo(size_t newSize)
{
    const bool result             = buffer.resizeWithoutInitializing(newSize);
    IGrowableBuffer::directAccess = {buffer.size(), buffer.capacity(), buffer.data()};
    return result;
}

} // namespace SC
