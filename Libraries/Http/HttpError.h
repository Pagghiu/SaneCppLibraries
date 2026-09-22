// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_http
//! @{

/// @brief Stable portable failures owned by Http. Values are append-only.
/// @details Transport and caller failures preserve their original category and value.
enum class HttpError : uint32_t
{
    MalformedPercentEscape = 1,
    DecodedOutputTooSmall,
    RequestTargetEmpty,
    RequestTargetWhitespace,
    UnsupportedRequestTargetForm,
    MalformedURL,
    UnsupportedProtocol,
    InvalidURLPath,
    InvalidURLHost,
    InvalidURLUserInfo,
    InvalidIPv6Host,
    InvalidURLPort,
};

/// @brief Stable category assigned to Http-owned errors.
static constexpr ResultCategory HttpResultCategory = ResultCategory(17);

//! @}
} // namespace SC
