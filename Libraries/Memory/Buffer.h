// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Memory/Segment.h"

namespace SC
{

//! @addtogroup group_memory
//! @{

namespace detail
{
struct SegmentBuffer : public SegmentTrivial<char>, public SegmentSelfRelativePointer<char>
{
    static constexpr bool IsArray = false;
};
} // namespace detail

/// @brief An heap allocated byte buffer that can optionally use an inline buffer.
/// @see SC::SmallBuffer to use an inline buffer that can optionally become heap allocated as needed.
/// @note This class (and SC::SmallBuffer) reduces needs for the header-only SC::Vector (from @ref library_containers).
/// SC::Buffer avoids some compile time / executable size bloat because it's not header only.
///
/// Example:
/// \snippet Tests/Libraries/Memory/BufferTest.cpp BufferBasicSnippet
struct Buffer : public Segment<detail::SegmentBuffer>
{
    using Segment::Segment;
};

/// @brief A SC::Buffer with a dedicated custom inline buffer to avoid heap allocation.
/// @note You can pass a SmallBuffer everywhere a reference to a Buffer is requested.
/// SC::Buffer will fallback to heap allocation once the inline buffer size is exceeded.
template <int N>
struct SmallBuffer : public Buffer
{
    SmallBuffer(SegmentAllocator allocator = SegmentAllocator::Global) noexcept : Buffer(N, allocator) {}
    SmallBuffer(const Buffer& other) noexcept : SmallBuffer() { Buffer::operator=(other); }
    SmallBuffer(Buffer&& other) noexcept : SmallBuffer() { Buffer::operator=(move(other)); }
    Buffer& operator=(const Buffer& other) noexcept { return Buffer::operator=(other); }
    Buffer& operator=(Buffer&& other) noexcept { return Buffer::operator=(move(other)); }

    // clang-format off
    SmallBuffer(const SmallBuffer& other) noexcept : SmallBuffer() { Buffer::operator=(other); }
    SmallBuffer(SmallBuffer&& other) noexcept : SmallBuffer() { Buffer::operator=(move(other)); }
    SmallBuffer& operator=(const SmallBuffer& other) noexcept { Buffer::operator=(other); return *this; }
    SmallBuffer& operator=(SmallBuffer&& other) noexcept { Buffer::operator=(move(other)); return *this; }
    // clang-format on

  protected:
    SmallBuffer(int num, SegmentAllocator allocator) : Buffer(N, allocator) { (void)num; }

  private:
    uint64_t inlineCapacity = N;
    char     inlineBuffer[N];
};

using BufferTL = detail::SegmentCustom<Buffer, Buffer, 0, SegmentAllocator::ThreadLocal>;
template <int N>
using SmallBufferTL = detail::SegmentCustom<SmallBuffer<N>, Buffer, N, SegmentAllocator::ThreadLocal>;

#if SC_COMPILER_MSVC
// Adding the SC_COMPILER_EXPORT on Buffer declaration causes MSVC to issue error C2491
struct SC_COMPILER_EXPORT Buffer;
#endif

//! @}
} // namespace SC
