// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Libraries/Common/Result.h"

namespace SC
{
/// @brief Stable failures owned by the SC-build tool. Values are append-only.
enum class BuildError : uint32_t
{
    HelpOutputFailed = 1,
    ParseErrorOutputFailed,
    InvalidOptionValue,
    AmbiguousOptionValue,
    InvalidOptionCombination,
    InvalidShortOptionGroup,
    UnsupportedPlatform,
    InvalidArguments,
    UnexpectedForwardedArguments,
    MissingLongPathPolicyValue,
    UnknownConfigureOption,
    DocumentationCommandFailed,
    UnsupportedAction,
};

static constexpr ResultCategory BuildResultCategory = ResultCategory(21);
} // namespace SC
