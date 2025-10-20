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

GrowableBuffer<Buffer>::GrowableBuffer(Buffer& buffer) noexcept
    : IGrowableBuffer(&GrowableBuffer::tryGrowTo), buffer(buffer)
{
    IGrowableBuffer::directAccess = {buffer.size(), buffer.capacity(), buffer.data()};
}

GrowableBuffer<Buffer>::~GrowableBuffer() noexcept { finalize(); }

void GrowableBuffer<Buffer>::finalize() noexcept
{
    if (buffer.size() != IGrowableBuffer::directAccess.sizeInBytes)
    {
        (void)buffer.resizeWithoutInitializing(IGrowableBuffer::directAccess.sizeInBytes);
    }
}

bool GrowableBuffer<Buffer>::tryGrowTo(IGrowableBuffer& gb, size_t newSize) noexcept
{
    GrowableBuffer& self = static_cast<GrowableBuffer&>(gb);
    // ensure size is correct before trying to grow to avoid losing data
    (void)self.buffer.resizeWithoutInitializing(self.directAccess.sizeInBytes);
    const bool result = self.buffer.resizeWithoutInitializing(newSize);
    self.directAccess = {self.buffer.size(), self.buffer.capacity(), self.buffer.data()};
    return result;
}

} // namespace SC
