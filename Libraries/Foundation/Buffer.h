// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Segment.h"

namespace SC
{

//! @addtogroup group_foundation_utility
//! @{

namespace detail
{
struct SegmentBuffer : public SegmentTrivial
{
    using Type                    = char;
    static constexpr bool IsArray = false;
};
} // namespace detail

/// @brief An heap allocated byte buffer that can optionally use an inline buffer.
/// @see SC::SmallBuffer to use an inline buffer that can optionally become heap allocated as needed.
/// @note This class (and SC::SmallBuffer) reduces needs for the header-only SC::Vector (from @ref library_containers).
/// SC::Buffer avoids some compile time / executable size bloat because it's not header only.
///
/// Example:
/// \snippet Libraries/Foundation/Tests/BufferTest.cpp BufferBasicSnippet
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
    SmallBuffer() : Buffer(N) {}
    SmallBuffer(const Buffer& other) : SmallBuffer() { Buffer::operator=(other); }
    SmallBuffer(Buffer&& other) : SmallBuffer() { Buffer::operator=(move(other)); }
    Buffer& operator=(const Buffer& other) { return Buffer::operator=(other); }
    Buffer& operator=(Buffer&& other) { return Buffer::operator=(move(other)); }

    // clang-format off
    SmallBuffer(const SmallBuffer& other) : SmallBuffer() { Buffer::operator=(other); }
    SmallBuffer(SmallBuffer&& other) : SmallBuffer() { Buffer::operator=(move(other)); }
    SmallBuffer& operator=(const SmallBuffer& other) { Buffer::operator=(other); return *this; }
    SmallBuffer& operator=(SmallBuffer&& other) { Buffer::operator=(move(other)); return *this; }
    // clang-format on

  private:
    uint64_t inlineCapacity = N;
    char     inlineBuffer[N];
};

#if SC_COMPILER_MSVC
// Adding the SC_COMPILER_EXPORT on Buffer declaration causes MSVC to issue error C2491
struct SC_COMPILER_EXPORT Buffer;
#endif

//! @}
} // namespace SC
