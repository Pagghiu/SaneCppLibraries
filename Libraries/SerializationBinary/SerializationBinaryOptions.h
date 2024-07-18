// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
namespace SC
{

//! @addtogroup group_serialization_binary
//! @{

/// @brief Conversion options for the binary versioned deserializer
struct SerializationBinaryOptions
{
    bool allowFloatToIntTruncation    = true; ///< Can truncate a float to get an integer value
    bool allowDropExcessArrayItems    = true; ///< Can drop array items if destination array is smaller
    bool allowDropExcessStructMembers = true; ///< Can drop fields not matching any memberTag in destination struct
    bool allowBoolConversions         = true; ///< Can convert bool to and from other primitive types (int / floats)
};

//! @}

} // namespace SC
