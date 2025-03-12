// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Buffer.h"
#include "../../Foundation/Internal/Segment.inl"
#include "../../Foundation/Internal/SegmentTrivial.inl"

namespace SC
{
template struct Segment<detail::SegmentBuffer>;
// Explicit instantiation of these methods because they are templated at method level
template bool Segment<detail::SegmentBuffer>::assign<char>(Span<const char>);
template bool Segment<detail::SegmentBuffer>::append<char>(Span<const char>);
} // namespace SC
