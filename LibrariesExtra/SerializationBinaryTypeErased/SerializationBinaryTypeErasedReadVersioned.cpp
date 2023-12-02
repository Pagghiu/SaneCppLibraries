// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "SerializationBinaryTypeErasedReadVersioned.h"

namespace SC
{
namespace SerializationBinaryTypeErased
{

[[nodiscard]] static bool copySourceSink(Span<const uint8_t> source, Span<uint8_t> other)
{
    if (other.sizeInBytes() >= source.sizeInBytes())
    {
        memcpy(other.data(), source.data(), source.sizeInBytes());
        return true;
    }
    return false;
}

template <typename SourceType, typename SinkType>
[[nodiscard]] static bool tryWritingPrimitiveValueToSink(SourceType sourceValue, Span<uint8_t>& sinkObject)
{
    SinkType sourceConverted = static_cast<SinkType>(sourceValue);
    SC_TRY(copySourceSink(Span<const uint8_t>::reinterpret_object(sourceConverted), sinkObject));
    return true;
}

template <typename T>
[[nodiscard]] static bool tryReadPrimitiveValue(SerializationBinary::BinaryBuffer* sourceObject,
                                                const Reflection::TypeInfo& sinkProperty, Span<uint8_t>& sinkObject)
{
    T sourceValue;
    SC_TRY(sourceObject->serializeBytes(Span<uint8_t>::reinterpret_object(sourceValue)));
    switch (sinkProperty.type)
    {
    case Reflection::TypeCategory::TypeUINT8:
        return tryWritingPrimitiveValueToSink<T, uint8_t>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeUINT16:
        return tryWritingPrimitiveValueToSink<T, uint16_t>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeUINT32:
        return tryWritingPrimitiveValueToSink<T, uint32_t>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeUINT64:
        return tryWritingPrimitiveValueToSink<T, uint64_t>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeINT8: return tryWritingPrimitiveValueToSink<T, int8_t>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeINT16:
        return tryWritingPrimitiveValueToSink<T, int16_t>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeINT32:
        return tryWritingPrimitiveValueToSink<T, int32_t>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeINT64:
        return tryWritingPrimitiveValueToSink<T, int64_t>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeFLOAT32:
        return tryWritingPrimitiveValueToSink<T, float>(sourceValue, sinkObject);
    case Reflection::TypeCategory::TypeDOUBLE64:
        return tryWritingPrimitiveValueToSink<T, double>(sourceValue, sinkObject);
    default: return false;
    }
}

[[nodiscard]] static bool tryPrimitiveConversion(const SerializerReadVersioned::Options& options,
                                                 const Reflection::TypeInfo&             sourceProperty,
                                                 SerializationBinary::BinaryBuffer*      sourceObject,
                                                 const Reflection::TypeInfo& sinkProperty, Span<uint8_t>& sinkObject)
{
    switch (sourceProperty.type)
    {
    case Reflection::TypeCategory::TypeUINT8:
        return tryReadPrimitiveValue<uint8_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::TypeCategory::TypeUINT16:
        return tryReadPrimitiveValue<uint16_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::TypeCategory::TypeUINT32:
        return tryReadPrimitiveValue<uint32_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::TypeCategory::TypeUINT64:
        return tryReadPrimitiveValue<uint64_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::TypeCategory::TypeINT8:
        return tryReadPrimitiveValue<int8_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::TypeCategory::TypeINT16:
        return tryReadPrimitiveValue<int16_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::TypeCategory::TypeINT32:
        return tryReadPrimitiveValue<int32_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::TypeCategory::TypeINT64:
        return tryReadPrimitiveValue<int64_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::TypeCategory::TypeFLOAT32: //
    {
        if (sinkProperty.type == Reflection::TypeCategory::TypeDOUBLE64 or options.allowFloatToIntTruncation)
        {
            return tryReadPrimitiveValue<float>(sourceObject, sinkProperty, sinkObject);
        }
        break;
    }
    case Reflection::TypeCategory::TypeDOUBLE64: //
    {
        if (sinkProperty.type == Reflection::TypeCategory::TypeFLOAT32 or options.allowFloatToIntTruncation)
        {
            return tryReadPrimitiveValue<double>(sourceObject, sinkProperty, sinkObject);
        }
        break;
    }
    default: break;
    }
    return false;
}
} // namespace SerializationBinaryTypeErased
} // namespace SC

bool SC::SerializationBinaryTypeErased::SerializerReadVersioned::read()
{
    sinkProperty   = sinkTypes.data()[sinkTypeIndex];
    sourceProperty = sourceTypes.data()[sourceTypeIndex];
    if (sourceProperty.isPrimitiveType())
    {
        if (sinkProperty.type == sourceProperty.type)
        {
            if (sinkObject.sizeInBytes() >= sourceProperty.sizeInBytes)
            {
                SC_TRY(sourceObject->serializeBytes(Span<uint8_t>{sinkObject.data(), sourceProperty.sizeInBytes}));
                return true;
            }
        }
        else
        {
            return tryPrimitiveConversion(options, sourceProperty, sourceObject, sinkProperty, sinkObject);
        }
    }
    else if (sourceProperty.type == Reflection::TypeCategory::TypeStruct)
    {
        return readStruct();
    }
    else if (sourceProperty.type == Reflection::TypeCategory::TypeArray ||
             sourceProperty.type == Reflection::TypeCategory::TypeVector)
    {
        return readArrayVector();
    }
    return false;
}

bool SC::SerializationBinaryTypeErased::SerializerReadVersioned::readStruct()
{
    if (sinkProperty.type != Reflection::TypeCategory::TypeStruct)
    {
        return false;
    }

    const auto    structSourceProperty  = sourceProperty;
    const auto    structSourceTypeIndex = sourceTypeIndex;
    const auto    structSinkProperty    = sinkProperty;
    const auto    structSinkTypeIndex   = sinkTypeIndex;
    Span<uint8_t> structSinkObject      = sinkObject;

    for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceProperty.getNumberOfChildren()); ++idx)
    {
        sourceTypeIndex        = structSourceTypeIndex + idx + 1;
        const auto sourceOrder = sourceTypes.data()[sourceTypeIndex].memberInfo.memberTag;
        uint32_t   findIdx;
        for (findIdx = 0; findIdx < static_cast<uint32_t>(structSinkProperty.getNumberOfChildren()); ++findIdx)
        {
            const auto typeIndex = structSinkTypeIndex + findIdx + 1;
            if (sinkTypes.data()[typeIndex].memberInfo.memberTag == sourceOrder)
            {
                break;
            }
        }
        if (sourceTypes.data()[sourceTypeIndex].hasValidLinkIndex())
            sourceTypeIndex = static_cast<uint32_t>(sourceTypes.data()[sourceTypeIndex].getLinkIndex());
        if (findIdx != static_cast<uint32_t>(structSinkProperty.getNumberOfChildren()))
        {
            // Member with same order ordinal has been found
            sinkTypeIndex = structSinkTypeIndex + findIdx + 1;
            SC_TRY(structSinkObject.sliceStartLength(sinkTypes.data()[sinkTypeIndex].memberInfo.offsetInBytes,
                                                     sinkTypes.data()[sinkTypeIndex].sizeInBytes, sinkObject));
            if (sinkTypes.data()[sinkTypeIndex].hasValidLinkIndex())
                sinkTypeIndex = static_cast<uint32_t>(sinkTypes.data()[sinkTypeIndex].getLinkIndex());
            SC_TRY(read());
        }
        else
        {
            SC_TRY(options.allowDropEccessStructMembers)
            // We must consume it anyway, discarding its content
            SC_TRY(skipCurrent());
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SerializerReadVersioned::readArrayVector()
{
    if (sinkProperty.type != Reflection::TypeCategory::TypeArray &&
        sinkProperty.type != Reflection::TypeCategory::TypeVector)
    {
        return false;
    }
    const auto    arraySourceProperty  = sourceProperty;
    const auto    arraySourceTypeIndex = sourceTypeIndex;
    const auto    arraySinkTypeIndex   = sinkTypeIndex;
    Span<uint8_t> arraySinkObject      = sinkObject;
    const auto    arraySinkProperty    = sinkProperty;

    sourceTypeIndex         = arraySourceTypeIndex + 1;
    uint64_t sourceNumBytes = arraySourceProperty.sizeInBytes;
    if (arraySourceProperty.type == Reflection::TypeCategory::TypeVector)
    {
        SC_TRY(sourceObject->serializeBytes(Span<uint8_t>::reinterpret_object(sourceNumBytes)));
    }

    const bool isPrimitive = sourceTypes.data()[sourceTypeIndex].isPrimitiveType();

    sinkTypeIndex = arraySinkTypeIndex + 1;

    const bool isSameType     = sinkTypes.data()[sinkTypeIndex].type == sourceTypes.data()[sourceTypeIndex].type;
    const bool isPacked       = isPrimitive && isSameType;
    const auto sourceItemSize = sourceTypes.data()[sourceTypeIndex].sizeInBytes;
    const auto sinkItemSize   = sinkTypes.data()[sinkTypeIndex].sizeInBytes;

    Span<uint8_t> arraySinkStart;
    if (arraySinkProperty.type == Reflection::TypeCategory::TypeArray)
    {
        SC_TRY(arraySinkObject.sliceStartLength(0, arraySinkProperty.sizeInBytes, arraySinkStart));
    }
    else
    {
        const auto numWantedBytes = sourceNumBytes / sourceItemSize * sinkItemSize;
        SC_TRY(arrayAccess.resize(arraySinkTypeIndex, arraySinkObject, arraySinkProperty, numWantedBytes,
                                  isPacked ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                  options.allowDropEccessArrayItems ? ArrayAccess::DropEccessItems::Yes
                                                                    : ArrayAccess::DropEccessItems::No));
        SC_TRY(arrayAccess.getSegmentSpan(arraySinkTypeIndex, arraySinkProperty, arraySinkObject, arraySinkStart));
    }
    if (isPacked)
    {
        const auto minBytes = min(static_cast<uint64_t>(arraySinkStart.sizeInBytes()), sourceNumBytes);
        SC_TRY(sourceObject->serializeBytes(Span<uint8_t>{arraySinkStart.data(), static_cast<size_t>(minBytes)}));
        if (sourceNumBytes > static_cast<uint64_t>(arraySinkStart.sizeInBytes()))
        {
            // We must consume these excess bytes anyway, discarding their content
            SC_TRY(options.allowDropEccessArrayItems);
            return sourceObject->advanceBytes(static_cast<size_t>(sourceNumBytes - minBytes));
        }
    }
    else
    {
        if (sinkTypes.data()[sinkTypeIndex].hasValidLinkIndex())
            sinkTypeIndex = static_cast<uint32_t>(sinkTypes.data()[sinkTypeIndex].getLinkIndex());
        if (sinkTypes.data()[sourceTypeIndex].hasValidLinkIndex())
            sourceTypeIndex = static_cast<uint32_t>(sourceTypes.data()[sourceTypeIndex].getLinkIndex());
        const uint64_t sinkNumElements     = arraySinkStart.sizeInBytes() / sinkItemSize;
        const uint64_t sourceNumElements   = sourceNumBytes / sourceItemSize;
        const auto     itemSinkTypeIndex   = sinkTypeIndex;
        const auto     itemSourceTypeIndex = sourceTypeIndex;
        const auto     minElements         = min(sinkNumElements, sourceNumElements);
        for (uint64_t idx = 0; idx < minElements; ++idx)
        {
            sinkTypeIndex   = itemSinkTypeIndex;
            sourceTypeIndex = itemSourceTypeIndex;
            SC_TRY(arraySinkStart.sliceStartLength(static_cast<size_t>(idx * sinkItemSize), sinkItemSize, sinkObject));
            SC_TRY(read());
        }
        if (sourceNumElements > sinkNumElements)
        {
            // We must consume these excess items anyway, discarding their content
            SC_TRY(options.allowDropEccessArrayItems);
            for (uint32_t idx = 0; idx < sourceNumElements - sinkNumElements; ++idx)
            {
                sourceTypeIndex = itemSourceTypeIndex;
                SC_TRY(skipCurrent());
            }
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SerializerReadVersioned::skipCurrent()
{
    Serialization::BinarySkipper<SerializationBinary::BinaryBuffer> skipper(*sourceObject, sourceTypeIndex);
    skipper.sourceTypes = sourceTypes;
    return skipper.skip();
}
