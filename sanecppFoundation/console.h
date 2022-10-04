#pragma once
#include "compiler.h"
#include "os.h"
#include "types.h"

namespace sanecpp
{
int printf(const char_t* format, ...) SANECPP_PRINTF_LIKE;
} // namespace sanecpp
