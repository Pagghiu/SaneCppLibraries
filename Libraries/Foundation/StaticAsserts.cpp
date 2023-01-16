// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Language.h"
#include "Limits.h"

// system includes
#include <float.h>  // FLT_MAX / DBL_MAX
#include <stdlib.h> // *_MAX (integer)
#if SC_MSVC
#include <BaseTsd.h>
#include <stdint.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h> // ssize_t
#endif
namespace SC
{
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

static_assert(IsSame<uint8_t, ::uint8_t>::value, "uint8_t");
static_assert(IsSame<uint16_t, ::uint16_t>::value, "uint16_t");
static_assert(IsSame<uint32_t, ::uint32_t>::value, "uint32_t");
static_assert(IsSame<uint64_t, ::uint64_t>::value, "uint64_t");

static_assert(IsSame<int8_t, ::int8_t>::value, "int8_t");
static_assert(IsSame<int16_t, ::int16_t>::value, "int16_t");
static_assert(IsSame<int32_t, ::int32_t>::value, "int32_t");
static_assert(IsSame<int64_t, ::int64_t>::value, "int64_t");

static_assert(IsSame<size_t, ::size_t>::value, "size_t");
static_assert(IsSame<ssize_t, ::ssize_t>::value, "size_t");
} // namespace SC
