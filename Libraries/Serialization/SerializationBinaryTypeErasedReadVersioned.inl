// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "SerializationBinaryTypeErasedReadVersioned.h"

namespace SC
{
namespace SerializationBinaryTypeErased
{

[[nodiscard]] static bool copySourceSink(SpanVoid<const void> source, SpanVoid<void> other)
{
    if (other.sizeInBytes() >= source.sizeInBytes())
    {
        memcpy(other.data(), source.data(), source.sizeInBytes());
        return true;
    }
    return false;
}

template <typename SourceType, typename SinkType>
[[nodiscard]] static bool tryWritingPrimitiveValueToSink(SourceType sourceValue, SpanVoid<void>& sinkObject)
{
    SinkType sourceConverted = static_cast<SinkType>(sourceValue);
    SC_TRY_IF(copySourceSink(SpanVoid<const void>(&sourceConverted, sizeof(sourceConverted)), sinkObject));
    return true;
}

template <typename T>
[[nodiscard]] static bool tryReadPrimitiveValue(Serialization::BinaryBuffer*      sourceObject,
                                                const Reflection::MetaProperties& sinkProperty,
                                                SpanVoid<void>&                   sinkObject)
{
    T sourceValue;
    SC_TRY_IF(sourceObject->serialize(SpanVoid<void>{&sourceValue, sizeof(T)}));
    SpanVoid<const void> span(&sourceValue, sizeof(T));
    switch (sinkProperty.type)
    {
    case Reflection::MetaType::TypeUINT8: return tryWritingPrimitiveValueToSink<T, uint8_t>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeUINT16: return tryWritingPrimitiveValueToSink<T, uint16_t>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeUINT32: return tryWritingPrimitiveValueToSink<T, uint32_t>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeUINT64: return tryWritingPrimitiveValueToSink<T, uint64_t>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeINT8: return tryWritingPrimitiveValueToSink<T, int8_t>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeINT16: return tryWritingPrimitiveValueToSink<T, int16_t>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeINT32: return tryWritingPrimitiveValueToSink<T, int32_t>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeINT64: return tryWritingPrimitiveValueToSink<T, int64_t>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeFLOAT32: return tryWritingPrimitiveValueToSink<T, float>(sourceValue, sinkObject);
    case Reflection::MetaType::TypeDOUBLE64: return tryWritingPrimitiveValueToSink<T, double>(sourceValue, sinkObject);
    default: return false;
    }
}

[[nodiscard]] static bool tryPrimitiveConversion(const SerializerReadVersioned::Options& options,
                                                 const Reflection::MetaProperties&       sourceProperty,
                                                 Serialization::BinaryBuffer*            sourceObject,
                                                 const Reflection::MetaProperties&       sinkProperty,
                                                 SpanVoid<void>&                         sinkObject)
{
    switch (sourceProperty.type)
    {
    case Reflection::MetaType::TypeUINT8: return tryReadPrimitiveValue<uint8_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::MetaType::TypeUINT16:
        return tryReadPrimitiveValue<uint16_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::MetaType::TypeUINT32:
        return tryReadPrimitiveValue<uint32_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::MetaType::TypeUINT64:
        return tryReadPrimitiveValue<uint64_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::MetaType::TypeINT8: return tryReadPrimitiveValue<int8_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::MetaType::TypeINT16: return tryReadPrimitiveValue<int16_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::MetaType::TypeINT32: return tryReadPrimitiveValue<int32_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::MetaType::TypeINT64: return tryReadPrimitiveValue<int64_t>(sourceObject, sinkProperty, sinkObject);
    case Reflection::MetaType::TypeFLOAT32: //
    {
        if (sinkProperty.type == Reflection::MetaType::TypeDOUBLE64 or options.allowFloatToIntTruncation)
        {
            return tryReadPrimitiveValue<float>(sourceObject, sinkProperty, sinkObject);
        }
        break;
    }
    case Reflection::MetaType::TypeDOUBLE64: //
    {
        if (sinkProperty.type == Reflection::MetaType::TypeFLOAT32 or options.allowFloatToIntTruncation)
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
    sinkProperty   = sinkProperties.data()[sinkTypeIndex];
    sourceProperty = sourceProperties.data()[sourceTypeIndex];
    if (sourceProperty.isPrimitiveType())
    {
        if (sinkProperty.type == sourceProperty.type)
        {
            if (sinkObject.sizeInBytes() >= sourceProperty.sizeInBytes)
            {
                SC_TRY_IF(sourceObject->serialize(SpanVoid<void>{sinkObject.data(), sourceProperty.sizeInBytes}));
                return true;
            }
        }
        else
        {
            return tryPrimitiveConversion(options, sourceProperty, sourceObject, sinkProperty, sinkObject);
        }
    }
    else if (sourceProperty.type == Reflection::MetaType::TypeStruct)
    {
        return readStruct();
    }
    else if (sourceProperty.type == Reflection::MetaType::TypeArray ||
             sourceProperty.type == Reflection::MetaType::TypeVector)
    {
        return readArrayVector();
    }
    return false;
}

bool SC::SerializationBinaryTypeErased::SerializerReadVersioned::readStruct()
{
    if (sinkProperty.type != Reflection::MetaType::TypeStruct)
    {
        return false;
    }

    const auto     structSourceProperty  = sourceProperty;
    const auto     structSourceTypeIndex = sourceTypeIndex;
    const auto     structSinkProperty    = sinkProperty;
    const auto     structSinkTypeIndex   = sinkTypeIndex;
    SpanVoid<void> structSinkObject      = sinkObject;

    for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceProperty.numSubAtoms); ++idx)
    {
        sourceTypeIndex        = structSourceTypeIndex + idx + 1;
        const auto sourceOrder = sourceProperties.data()[sourceTypeIndex].order;
        uint32_t   findIdx;
        for (findIdx = 0; findIdx < static_cast<uint32_t>(structSinkProperty.numSubAtoms); ++findIdx)
        {
            const auto typeIndex = structSinkTypeIndex + findIdx + 1;
            if (sinkProperties.data()[typeIndex].order == sourceOrder)
            {
                break;
            }
        }
        if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
            sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());
        if (findIdx != static_cast<uint32_t>(structSinkProperty.numSubAtoms))
        {
            // Member with same order ordinal has been found
            sinkTypeIndex = structSinkTypeIndex + findIdx + 1;
            SC_TRY_IF(structSinkObject.viewAtBytes(sinkProperties.data()[sinkTypeIndex].offsetInBytes,
                                                   sinkProperties.data()[sinkTypeIndex].sizeInBytes, sinkObject));
            if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
                sinkTypeIndex = static_cast<uint32_t>(sinkProperties.data()[sinkTypeIndex].getLinkIndex());
            SC_TRY_IF(read());
        }
        else
        {
            SC_TRY_IF(options.allowDropEccessStructMembers)
            // We must consume it anyway, discarding its content
            SC_TRY_IF(skipCurrent());
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SerializerReadVersioned::readArrayVector()
{
    if (sinkProperty.type != Reflection::MetaType::TypeArray && sinkProperty.type != Reflection::MetaType::TypeVector)
    {
        return false;
    }
    const auto     arraySourceProperty  = sourceProperty;
    const auto     arraySourceTypeIndex = sourceTypeIndex;
    const auto     arraySinkTypeIndex   = sinkTypeIndex;
    SpanVoid<void> arraySinkObject      = sinkObject;
    const auto     arraySinkProperty    = sinkProperty;

    sourceTypeIndex         = arraySourceTypeIndex + 1;
    uint64_t sourceNumBytes = arraySourceProperty.sizeInBytes;
    if (arraySourceProperty.type == Reflection::MetaType::TypeVector)
    {
        SC_TRY_IF(sourceObject->serialize(SpanVoid<void>{&sourceNumBytes, sizeof(uint64_t)}));
    }

    const bool isPrimitive = sourceProperties.data()[sourceTypeIndex].isPrimitiveType();

    sinkTypeIndex = arraySinkTypeIndex + 1;

    const bool isSameType = sinkProperties.data()[sinkTypeIndex].type == sourceProperties.data()[sourceTypeIndex].type;
    const bool isPacked   = isPrimitive && isSameType;
    const auto sourceItemSize = sourceProperties.data()[sourceTypeIndex].sizeInBytes;
    const auto sinkItemSize   = sinkProperties.data()[sinkTypeIndex].sizeInBytes;

    SpanVoid<void> arraySinkStart;
    if (arraySinkProperty.type == Reflection::MetaType::TypeArray)
    {
        SC_TRY_IF(arraySinkObject.viewAtBytes(0, arraySinkProperty.sizeInBytes, arraySinkStart));
    }
    else
    {
        const auto numWantedBytes = sourceNumBytes / sourceItemSize * sinkItemSize;
        SC_TRY_IF(arrayAccess.resize(arraySinkTypeIndex, arraySinkObject, arraySinkProperty, numWantedBytes,
                                     isPacked ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                     options.allowDropEccessArrayItems ? ArrayAccess::DropEccessItems::Yes
                                                                       : ArrayAccess::DropEccessItems::No));
        SC_TRY_IF(arrayAccess.getSegmentSpan(arraySinkTypeIndex, arraySinkProperty, arraySinkObject, arraySinkStart));
    }
    if (isPacked)
    {
        const auto minBytes = min(static_cast<uint64_t>(arraySinkStart.sizeInBytes()), sourceNumBytes);
        SC_TRY_IF(sourceObject->serialize(SpanVoid<void>{arraySinkStart.data(), static_cast<size_t>(minBytes)}));
        if (sourceNumBytes > static_cast<uint64_t>(arraySinkStart.sizeInBytes()))
        {
            // We must consume these excess bytes anyway, discarding their content
            SC_TRY_IF(options.allowDropEccessArrayItems);
            return sourceObject->advance(sourceNumBytes - minBytes);
        }
    }
    else
    {
        if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
            sinkTypeIndex = static_cast<uint32_t>(sinkProperties.data()[sinkTypeIndex].getLinkIndex());
        if (sinkProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
            sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());
        const uint64_t sinkNumElements     = arraySinkStart.sizeInBytes() / sinkItemSize;
        const uint64_t sourceNumElements   = sourceNumBytes / sourceItemSize;
        const auto     itemSinkTypeIndex   = sinkTypeIndex;
        const auto     itemSourceTypeIndex = sourceTypeIndex;
        const auto     minElements         = min(sinkNumElements, sourceNumElements);
        for (uint64_t idx = 0; idx < minElements; ++idx)
        {
            sinkTypeIndex   = itemSinkTypeIndex;
            sourceTypeIndex = itemSourceTypeIndex;
            SC_TRY_IF(arraySinkStart.viewAtBytes(idx * sinkItemSize, sinkItemSize, sinkObject));
            SC_TRY_IF(read());
        }
        if (sourceNumElements > sinkNumElements)
        {
            // We must consume these excess items anyway, discarding their content
            SC_TRY_IF(options.allowDropEccessArrayItems);
            for (uint32_t idx = 0; idx < sourceNumElements - sinkNumElements; ++idx)
            {
                sourceTypeIndex = itemSourceTypeIndex;
                SC_TRY_IF(skipCurrent());
            }
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SerializerReadVersioned::skipCurrent()
{
    Serialization::BinarySkipper<Serialization::BinaryBuffer> skipper(*sourceObject, sourceTypeIndex);
    skipper.sourceProperties = sourceProperties;
    return skipper.skip();
}