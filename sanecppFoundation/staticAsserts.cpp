#include "console.h"
#include "language.h"
#include "limits.h"

// system includes
#include <float.h>  // FLT_MAX / DBL_MAX
#include <stdlib.h> // *_MAX (integer)
#include <unistd.h> // ssize_t

namespace sanecpp
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

static_assert(is_same<uint8_t, ::uint8_t>::value, "uint8_t");
static_assert(is_same<uint16_t, ::uint16_t>::value, "uint16_t");
static_assert(is_same<uint32_t, ::uint32_t>::value, "uint32_t");
static_assert(is_same<uint64_t, ::uint64_t>::value, "uint64_t");

static_assert(is_same<int8_t, ::int8_t>::value, "int8_t");
static_assert(is_same<int16_t, ::int16_t>::value, "int16_t");
static_assert(is_same<int32_t, ::int32_t>::value, "int32_t");
static_assert(is_same<int64_t, ::int64_t>::value, "int64_t");

static_assert(is_same<size_t, ::size_t>::value, "size_t");
static_assert(is_same<ssize_t, ::ssize_t>::value, "size_t");
} // namespace sanecpp
