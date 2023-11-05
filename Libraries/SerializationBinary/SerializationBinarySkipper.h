// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Vector.h"
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
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
    size_t numberOfOperations = 0;

    [[nodiscard]] bool serializeBytes(const void* object, size_t numBytes)
    {
        return serializeBytes(Span<const uint8_t>::reinterpret_bytes(object, numBytes));
    }

    [[nodiscard]] bool serializeBytes(void* object, size_t numBytes)
    {
        return serializeBytes(Span<uint8_t>::reinterpret_bytes(object, numBytes));
    }

    [[nodiscard]] bool serializeBytes(Span<const uint8_t> object)
    {
        numberOfOperations++;
        return buffer.append(object);
    }

    [[nodiscard]] bool serializeBytes(Span<uint8_t> object)
    {
        if (index + object.sizeInBytes() > buffer.size())
            return false;
        numberOfOperations++;
        memcpy(object.data(), &buffer[index], object.sizeInBytes());
        index += object.sizeInBytes();
        return true;
    }

    [[nodiscard]] bool advanceBytes(size_t numBytes)
    {
        if (index + numBytes > buffer.size())
            return false;
        index += numBytes;
        return true;
    }
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
            return sourceObject.advanceBytes(sourceProperty.sizeInBytes);
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
            SC_TRY(sourceObject.advanceBytes(structSourceProperty.sizeInBytes));
        }
        else
        {
            for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceProperty.numSubAtoms); ++idx)
            {
                sourceTypeIndex = structSourceTypeIndex + idx + 1;
                if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());
                SC_TRY(skip());
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
            SC_TRY(sourceObject.serializeBytes(Span<uint8_t>::reinterpret_object(sourceNumBytes)));
        }

        const bool isPacked = sourceProperties.data()[sourceTypeIndex].isPrimitiveOrRecursivelyPacked();
        if (isPacked)
        {
            return sourceObject.advanceBytes(static_cast<size_t>(sourceNumBytes));
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
                SC_TRY(skip());
            }
            return true;
        }
    }
};

} // namespace Serialization
} // namespace SC
