// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Reflection/Reflection.h"

namespace SC
{
namespace detail
{
template <typename BinaryStream>
struct SerializationBinarySkipper
{
    Span<const Reflection::TypeInfo> sourceTypes;
    Reflection::TypeInfo             sourceType;

    SerializationBinarySkipper(BinaryStream& stream, uint32_t& sourceTypeIndex)
        : sourceObject(stream), sourceTypeIndex(sourceTypeIndex)
    {}

    [[nodiscard]] bool skip()
    {
        sourceType = sourceTypes.data()[sourceTypeIndex];
        if (sourceType.type == Reflection::TypeCategory::TypeStruct)
        {
            return skipStruct();
        }
        else if (sourceType.type == Reflection::TypeCategory::TypeArray ||
                 sourceType.type == Reflection::TypeCategory::TypeVector)
        {
            return skipVectorOrArray();
        }
        else if (sourceType.isPrimitiveType())
        {
            return sourceObject.advanceBytes(sourceType.sizeInBytes);
        }
        return false;
    }

  private:
    BinaryStream& sourceObject;
    uint32_t&     sourceTypeIndex;

    [[nodiscard]] bool skipStruct()
    {
        const auto structSourceType      = sourceType;
        const auto structSourceTypeIndex = sourceTypeIndex;
        const bool isPacked              = sourceTypes.data()[sourceTypeIndex].isPrimitiveOrPackedStruct();

        if (isPacked)
        {
            if (not sourceObject.advanceBytes(structSourceType.sizeInBytes))
                return false;
        }
        else
        {
            for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceType.getNumberOfChildren()); ++idx)
            {
                sourceTypeIndex = structSourceTypeIndex + idx + 1;
                if (sourceTypes.data()[sourceTypeIndex].hasValidLinkIndex())
                    sourceTypeIndex = static_cast<uint32_t>(sourceTypes.data()[sourceTypeIndex].getLinkIndex());
                if (not skip())
                    return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool skipVectorOrArray()
    {
        const auto arraySourceType      = sourceType;
        const auto arraySourceTypeIndex = sourceTypeIndex;

        sourceTypeIndex         = arraySourceTypeIndex + 1;
        uint64_t sourceNumBytes = arraySourceType.sizeInBytes;
        if (arraySourceType.type == Reflection::TypeCategory::TypeVector)
        {
            if (not sourceObject.serializeBytes(Span<uint8_t>::reinterpret_object(sourceNumBytes)))
                return false;
        }

        const bool isPacked = sourceTypes.data()[sourceTypeIndex].isPrimitiveOrPackedStruct();
        if (isPacked)
        {
            return sourceObject.advanceBytes(static_cast<size_t>(sourceNumBytes));
        }
        else
        {
            const auto sourceItemSize      = sourceTypes.data()[sourceTypeIndex].sizeInBytes;
            const auto sourceNumElements   = sourceNumBytes / sourceItemSize;
            const auto itemSourceTypeIndex = sourceTypeIndex;
            for (uint64_t idx = 0; idx < sourceNumElements; ++idx)
            {
                sourceTypeIndex = itemSourceTypeIndex;
                if (sourceTypes.data()[sourceTypeIndex].hasValidLinkIndex())
                    sourceTypeIndex = static_cast<uint32_t>(sourceTypes.data()[sourceTypeIndex].getLinkIndex());
                if (not skip())
                    return false;
            }
            return true;
        }
    }
};
} // namespace detail
} // namespace SC
