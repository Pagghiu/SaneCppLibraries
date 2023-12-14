// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
namespace SC
{

//! @addtogroup group_serialization_binary
//! @{

/// @brief Conversion options for the binary versioned deserializer
struct SerializationBinaryOptions
{
    bool allowFloatToIntTruncation    = true; ///< Can truncate a float to get an integer value
    bool allowDropEccessArrayItems    = true; ///< Can drop array items if destination array is smaller
    bool allowDropEccessStructMembers = true; ///< Can drop fields not matching any memberTag in destination struct
};

//! @}

} // namespace SC
