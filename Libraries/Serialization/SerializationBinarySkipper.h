// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "../Foundation/Vector.h"
#include "../Reflection/Reflection.h"

namespace SC
{
namespace Serialization
{
// TODO: BinaryBuffer needs to go with once we transition to streaming interface
struct BinaryBuffer
{
    SC::Vector<uint8_t> buffer;

    size_t index              = 0;
    int    numberOfOperations = 0;

    [[nodiscard]] bool serialize(SpanVoid<const void> object);
    [[nodiscard]] bool serialize(SpanVoid<void> object);
    [[nodiscard]] bool advance(size_t numBytes);
};

// TODO: BinarySkipper should go out of header once we replace BinaryBuffer with a streaming interface
template <typename BinaryStream>
struct BinarySkipper
{
    Span<const Reflection::MetaProperties> sourceProperties;
    Reflection::MetaProperties             sourceProperty;

    BinarySkipper(BinaryStream& stream, uint32_t& sourceTypeIndex)
        : sourceObject(stream), sourceTypeIndex(sourceTypeIndex)
    {}

    [[nodiscard]] bool skip()
    {
        sourceProperty = sourceProperties.data()[sourceTypeIndex];
        if (sourceProperty.type == Reflection::MetaType::TypeStruct)
        {
            return skipStruct();
        }
        else if (sourceProperty.type == Reflection::MetaType::TypeArray ||
                 sourceProperty.type == Reflection::MetaType::TypeVector)
        {
            return skipVectorOrArray();
        }
        else if (sourceProperty.isPrimitiveType())
        {
            return sourceObject.advance(sourceProperty.sizeInBytes);
        }
        return false;
    }

  private:
    BinaryStream& sourceObject;
    uint32_t&     sourceTypeIndex;

    [[nodiscard]] bool skipStruct()
    {
        const auto structSourceProperty  = sourceProperty;
        const auto structSourceTypeIndex = sourceTypeIndex;
        const bool isPacked              = sourceProperties.data()[sourceTypeIndex].isPrimitiveOrRecursivelyPacked();

        if (isPacked)
        {
            SC_TRY_IF(sourceObject.advance(structSourceProperty.sizeInBytes));
        }
        else
        {
            for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceProperty.numSubAtoms); ++idx)
            {
                sourceTypeIndex = structSourceTypeIndex + idx + 1;
                if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());
                SC_TRY_IF(skip());
            }
        }
        return true;
    }

    [[nodiscard]] bool skipVectorOrArray()
    {
        const auto arraySourceProperty  = sourceProperty;
        const auto arraySourceTypeIndex = sourceTypeIndex;

        sourceTypeIndex         = arraySourceTypeIndex + 1;
        uint64_t sourceNumBytes = arraySourceProperty.sizeInBytes;
        if (arraySourceProperty.type == Reflection::MetaType::TypeVector)
        {
            SC_TRY_IF(sourceObject.serialize(SpanVoid<void>(&sourceNumBytes, sizeof(uint64_t))));
        }

        const bool isPacked = sourceProperties.data()[sourceTypeIndex].isPrimitiveOrRecursivelyPacked();
        if (isPacked)
        {
            return sourceObject.advance(sourceNumBytes);
        }
        else
        {
            const auto sourceItemSize      = sourceProperties.data()[sourceTypeIndex].sizeInBytes;
            const auto sourceNumElements   = sourceNumBytes / sourceItemSize;
            const auto itemSourceTypeIndex = sourceTypeIndex;
            for (uint64_t idx = 0; idx < sourceNumElements; ++idx)
            {
                sourceTypeIndex = itemSourceTypeIndex;
                if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());
                SC_TRY_IF(skip());
            }
            return true;
        }
    }
};

} // namespace Serialization
} // namespace SC
