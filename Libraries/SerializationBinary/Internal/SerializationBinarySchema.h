// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SerializationBinarySkipper.h"

namespace SC
{

//! @addtogroup group_serialization_binary
//! @{

/// @brief Holds Schema of serialized binary data
struct SerializationSchema
{
    Span<const Reflection::TypeInfo> sourceTypes;

    SerializationSchema(const Span<const Reflection::TypeInfo> typeInfos) : sourceTypes(typeInfos) {}

    /// @brief Controls compatibility options for versioned deserialization
    struct Options
    {
        bool allowFloatToIntTruncation    = true; ///< allow truncating a float to get an integer value
        bool allowDropEccessArrayItems    = true; ///< drop array items in source data if destination array is smaller
        bool allowDropEccessStructMembers = true; ///< drop fields that have no matching memberTag in destination struct
    };
    Options options; ///< Options for versioned deserialization

    uint32_t sourceTypeIndex = 0; ///< Currently active type in sourceTypes Span

    // TODO: All the following methods should go in detail and out of public API

    constexpr Reflection::TypeInfo current() const { return sourceTypes.data()[sourceTypeIndex]; }

    constexpr void advance() { sourceTypeIndex++; }

    constexpr void resolveLink()
    {
        if (sourceTypes.data()[sourceTypeIndex].hasValidLinkIndex())
            sourceTypeIndex = static_cast<uint32_t>(sourceTypes.data()[sourceTypeIndex].getLinkIndex());
    }

    template <typename BinaryStream>
    [[nodiscard]] constexpr bool skipCurrent(BinaryStream& stream)
    {
        detail::SerializationBinarySkipper<BinaryStream> skipper(stream, sourceTypeIndex);
        skipper.sourceTypes = sourceTypes;
        return skipper.skip();
    }
};

//! @}

} // namespace SC
