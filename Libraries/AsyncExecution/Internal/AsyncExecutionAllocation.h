// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include <stdlib.h>
namespace SC
{
namespace detail
{
inline void* AsyncExecutionReallocate(void* memory, size_t bytes) noexcept { return ::realloc(memory, bytes); }
inline void  AsyncExecutionRelease(void* memory) noexcept { ::free(memory); }
} // namespace detail
} // namespace SC
