// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/Buffer.h"
#include "../../Foundation/Internal/Segment.inl"
#include "../../Foundation/Internal/SegmentTrivial.inl"

namespace SC
{
template struct Segment<detail::SegmentBuffer>;
} // namespace SC
