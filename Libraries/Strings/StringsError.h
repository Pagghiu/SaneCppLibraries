// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_strings
//! @{

/// @brief Stable error codes returned by the Strings library.
enum class StringsError : uint32_t
{
    InvalidArgumentCount = 1,
    InsufficientArgumentStorage,
};

/// @brief Stable category assigned to errors owned by the Strings library.
static constexpr ResultCategory StringsResultCategory = ResultCategory(2);

//! @}
} // namespace SC
