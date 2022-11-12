#pragma once
#include "Reflection.h"

namespace SC
{
namespace Serialization
{
template <typename BinaryStream>
struct BinarySkipper
{
    Span<const Reflection::MetaProperties> sourceProperties;
    Reflection::MetaProperties             sourceProperty;

    BinarySkipper(BinaryStream& stream, int& sourceTypeIndex) : sourceObject(stream), sourceTypeIndex(sourceTypeIndex)
    {}

    [[nodiscard]] bool skip()
    {
        sourceProperty = sourceProperties.data[sourceTypeIndex];
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
            return sourceObject.advance(sourceProperty.size);
        }
        return false;
    }

  private:
    BinaryStream& sourceObject;
    int&          sourceTypeIndex;

    [[nodiscard]] bool skipStruct()
    {
        const auto structSourceProperty  = sourceProperty;
        const auto structSourceTypeIndex = sourceTypeIndex;
        // TODO: If we save the "isPacked" into some atom flags we coud skip entire struct
        for (int16_t idx = 0; idx < structSourceProperty.numSubAtoms; ++idx)
        {
            sourceTypeIndex = structSourceTypeIndex + idx + 1;
            if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
            SC_TRY_IF(skip());
        }
        return true;
    }

    [[nodiscard]] bool skipVectorOrArray()
    {
        const auto arraySourceProperty  = sourceProperty;
        const auto arraySourceTypeIndex = sourceTypeIndex;

        sourceTypeIndex         = arraySourceTypeIndex + 1;
        uint64_t sourceNumBytes = arraySourceProperty.size;
        if (arraySourceProperty.type == Reflection::MetaType::TypeVector)
        {
            SC_TRY_IF(sourceObject.readAndAdvance(sourceNumBytes));
        }

        // TODO: If we save the "isRecursivelyPacked" into some atom flags we coud skip entire array
        const bool isPrimitive = sourceProperties.data[sourceTypeIndex].isPrimitiveType();

        if (isPrimitive)
        {
            return sourceObject.advance(sourceNumBytes);
        }
        else
        {
            const auto sourceItemSize      = sourceProperties.data[sourceTypeIndex].size;
            const auto sourceNumElements   = sourceNumBytes / sourceItemSize;
            const auto itemSourceTypeIndex = sourceTypeIndex;
            for (uint64_t idx = 0; idx < sourceNumElements; ++idx)
            {
                sourceTypeIndex = itemSourceTypeIndex;
                if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
                SC_TRY_IF(skip());
            }
            return true;
        }
    }
};

} // namespace Serialization
} // namespace SC
