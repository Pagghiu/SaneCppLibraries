// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../SerializationBinaryOptions.h"
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

    SerializationBinaryOptions options; ///< Options for versioned deserialization

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
