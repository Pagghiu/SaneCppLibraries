// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Testing/Limits.h"

#include <float.h>  // FLT_MAX / DBL_MAX
#include <stdint.h> // Linux *_{MAX | MIN}

#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#else
#include <sys/types.h> // size_t / ssize_t
#endif

namespace SC
{
template <typename T, typename U>
struct CheckSameType
{
    static constexpr bool value = false;
};
template <typename T>
struct CheckSameType<T, T>
{
    static constexpr bool value = true;
};

static_assert(static_cast<float>(MaxValue()) == FLT_MAX, "static_cast<float>(MaxValue)");
static_assert(static_cast<double>(MaxValue()) == DBL_MAX, "static_cast<double>(MaxValue)");

static_assert(static_cast<uint8_t>(MaxValue()) == UINT8_MAX, "static_cast<uint8_t>(MaxValue)");
static_assert(static_cast<uint16_t>(MaxValue()) == UINT16_MAX, "static_cast<uint16_t>(MaxValue)");
static_assert(static_cast<uint32_t>(MaxValue()) == UINT32_MAX, "static_cast<uint32_t>(MaxValue)");
static_assert(static_cast<uint64_t>(MaxValue()) == UINT64_MAX, "static_cast<uint64_t>(MaxValue)");

static_assert(static_cast<int8_t>(MaxValue()) == INT8_MAX, "static_cast<int8_t>(MaxValue)");
static_assert(static_cast<int16_t>(MaxValue()) == INT16_MAX, "static_cast<int16_t>(MaxValue)");
static_assert(static_cast<int32_t>(MaxValue()) == INT32_MAX, "static_cast<int32_t>(MaxValue)");
static_assert(static_cast<int64_t>(MaxValue()) == INT64_MAX, "static_cast<int64_t>(MaxValue)");

static_assert(CheckSameType<uint8_t, ::uint8_t>::value, "uint8_t");
static_assert(CheckSameType<uint16_t, ::uint16_t>::value, "uint16_t");
static_assert(CheckSameType<uint32_t, ::uint32_t>::value, "uint32_t");
static_assert(CheckSameType<uint64_t, ::uint64_t>::value, "uint64_t");

static_assert(CheckSameType<int8_t, ::int8_t>::value, "int8_t");
static_assert(CheckSameType<int16_t, ::int16_t>::value, "int16_t");
static_assert(CheckSameType<int32_t, ::int32_t>::value, "int32_t");
static_assert(CheckSameType<int64_t, ::int64_t>::value, "int64_t");

static_assert(CheckSameType<size_t, ::size_t>::value, "size_t");
static_assert(CheckSameType<ssize_t, ::ssize_t>::value, "size_t");
} // namespace SC
