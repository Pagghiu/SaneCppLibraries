// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Libraries/Common/Result.h"

namespace SC
{
namespace Tools
{
/// @brief Stable failures owned by the common tool runner and formatter.
enum class ToolsError : uint32_t
{
    UnsupportedFormatAction = 1,
    ChildProcessExitedNonzero,
};

static constexpr ResultCategory ToolsResultCategory = ResultCategory(20);
} // namespace Tools
} // namespace SC
