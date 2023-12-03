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
    return copySourceSink(Span<const uint8_t>::reinterpret_object(sourceConverted), sinkObject);
}

template <typename T>
[[nodiscard]] static bool tryReadPrimitiveValue(SerializationBinary::Buffer* sourceObject,
                                                const Reflection::TypeInfo& sinkType, Span<uint8_t>& sinkObject)
{
    T sourceValue;
    if (not sourceObject->serializeBytes(Span<uint8_t>::reinterpret_object(sourceValue)))
        return false;
    switch (sinkType.type)
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

[[nodiscard]] static bool tryPrimitiveConversion(const SerializationBinaryTypeErased::VersionSchema::Options& options,
                                                 const Reflection::TypeInfo&  sourceType,
                                                 SerializationBinary::Buffer* sourceObject,
                                                 const Reflection::TypeInfo& sinkType, Span<uint8_t>& sinkObject)
{
    switch (sourceType.type)
    {
    case Reflection::TypeCategory::TypeUINT8: return tryReadPrimitiveValue<uint8_t>(sourceObject, sinkType, sinkObject);
    case Reflection::TypeCategory::TypeUINT16:
        return tryReadPrimitiveValue<uint16_t>(sourceObject, sinkType, sinkObject);
    case Reflection::TypeCategory::TypeUINT32:
        return tryReadPrimitiveValue<uint32_t>(sourceObject, sinkType, sinkObject);
    case Reflection::TypeCategory::TypeUINT64:
        return tryReadPrimitiveValue<uint64_t>(sourceObject, sinkType, sinkObject);
    case Reflection::TypeCategory::TypeINT8: return tryReadPrimitiveValue<int8_t>(sourceObject, sinkType, sinkObject);
    case Reflection::TypeCategory::TypeINT16: return tryReadPrimitiveValue<int16_t>(sourceObject, sinkType, sinkObject);
    case Reflection::TypeCategory::TypeINT32: return tryReadPrimitiveValue<int32_t>(sourceObject, sinkType, sinkObject);
    case Reflection::TypeCategory::TypeINT64: return tryReadPrimitiveValue<int64_t>(sourceObject, sinkType, sinkObject);
    case Reflection::TypeCategory::TypeFLOAT32: //
    {
        if (sinkType.type == Reflection::TypeCategory::TypeDOUBLE64 or options.allowFloatToIntTruncation)
        {
            return tryReadPrimitiveValue<float>(sourceObject, sinkType, sinkObject);
        }
        break;
    }
    case Reflection::TypeCategory::TypeDOUBLE64: //
    {
        if (sinkType.type == Reflection::TypeCategory::TypeFLOAT32 or options.allowFloatToIntTruncation)
        {
            return tryReadPrimitiveValue<double>(sourceObject, sinkType, sinkObject);
        }
        break;
    }
    default: break;
    }
    return false;
}
} // namespace SerializationBinaryTypeErased
} // namespace SC

bool SC::SerializationBinaryTypeErased::ReadVersioned::read()
{
    sinkType   = sinkTypes.data()[sinkTypeIndex];
    sourceType = sourceTypes.data()[sourceTypeIndex];
    if (sourceType.isPrimitiveType())
    {
        if (sinkType.type == sourceType.type)
        {
            if (sinkObject.sizeInBytes() >= sourceType.sizeInBytes)
            {
                return sourceObject->serializeBytes(Span<uint8_t>{sinkObject.data(), sourceType.sizeInBytes});
            }
        }
        else
        {
            return tryPrimitiveConversion(options, sourceType, sourceObject, sinkType, sinkObject);
        }
    }
    else if (sourceType.type == Reflection::TypeCategory::TypeStruct)
    {
        return readStruct();
    }
    else if (sourceType.type == Reflection::TypeCategory::TypeArray ||
             sourceType.type == Reflection::TypeCategory::TypeVector)
    {
        return readArrayVector();
    }
    return false;
}

bool SC::SerializationBinaryTypeErased::ReadVersioned::readStruct()
{
    if (sinkType.type != Reflection::TypeCategory::TypeStruct)
    {
        return false;
    }

    const auto    structSourceType      = sourceType;
    const auto    structSourceTypeIndex = sourceTypeIndex;
    const auto    structSinkType        = sinkType;
    const auto    structSinkTypeIndex   = sinkTypeIndex;
    Span<uint8_t> structSinkObject      = sinkObject;

    for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceType.getNumberOfChildren()); ++idx)
    {
        sourceTypeIndex        = structSourceTypeIndex + idx + 1;
        const auto sourceOrder = sourceTypes.data()[sourceTypeIndex].memberInfo.memberTag;
        uint32_t   findIdx;
        for (findIdx = 0; findIdx < static_cast<uint32_t>(structSinkType.getNumberOfChildren()); ++findIdx)
        {
            const auto typeIndex = structSinkTypeIndex + findIdx + 1;
            if (sinkTypes.data()[typeIndex].memberInfo.memberTag == sourceOrder)
            {
                break;
            }
        }
        if (sourceTypes.data()[sourceTypeIndex].hasValidLinkIndex())
            sourceTypeIndex = static_cast<uint32_t>(sourceTypes.data()[sourceTypeIndex].getLinkIndex());
        if (findIdx != static_cast<uint32_t>(structSinkType.getNumberOfChildren()))
        {
            // Member with same order ordinal has been found
            sinkTypeIndex             = structSinkTypeIndex + findIdx + 1;
            const auto sinkMemberType = sinkTypes.data()[sinkTypeIndex];
            if (not structSinkObject.sliceStartLength(sinkMemberType.memberInfo.offsetInBytes,
                                                      sinkMemberType.sizeInBytes, sinkObject))
                return false;
            if (sinkMemberType.hasValidLinkIndex())
                sinkTypeIndex = static_cast<uint32_t>(sinkMemberType.getLinkIndex());
            if (not read())
                return false;
        }
        else
        {
            if (not options.allowDropEccessStructMembers)
                return false;
            // We must consume it anyway, discarding its content
            if (not skipCurrent())
                return false;
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::ReadVersioned::readArrayVector()
{
    if (sinkType.type != Reflection::TypeCategory::TypeArray && sinkType.type != Reflection::TypeCategory::TypeVector)
    {
        return false;
    }
    const auto    arraySourceType      = sourceType;
    const auto    arraySourceTypeIndex = sourceTypeIndex;
    const auto    arraySinkTypeIndex   = sinkTypeIndex;
    Span<uint8_t> arraySinkObject      = sinkObject;
    const auto    arraySinkType        = sinkType;

    sourceTypeIndex         = arraySourceTypeIndex + 1;
    uint64_t sourceNumBytes = arraySourceType.sizeInBytes;
    if (arraySourceType.type == Reflection::TypeCategory::TypeVector)
    {
        if (not sourceObject->serializeBytes(Span<uint8_t>::reinterpret_object(sourceNumBytes)))
            return false;
    }

    const bool isPrimitive = sourceTypes.data()[sourceTypeIndex].isPrimitiveType();

    sinkTypeIndex = arraySinkTypeIndex + 1;

    const bool isSameType     = sinkTypes.data()[sinkTypeIndex].type == sourceTypes.data()[sourceTypeIndex].type;
    const bool isPacked       = isPrimitive && isSameType;
    const auto sourceItemSize = sourceTypes.data()[sourceTypeIndex].sizeInBytes;
    const auto sinkItemSize   = sinkTypes.data()[sinkTypeIndex].sizeInBytes;

    Span<uint8_t> arraySinkStart;
    if (arraySinkType.type == Reflection::TypeCategory::TypeArray)
    {
        if (not arraySinkObject.sliceStartLength(0, arraySinkType.sizeInBytes, arraySinkStart))
            return false;
    }
    else
    {
        using ArrayAccess         = SerializationBinaryTypeErased::detail::ArrayAccess;
        const auto numWantedBytes = sourceNumBytes / sourceItemSize * sinkItemSize;
        if (not arrayAccess.resize(arraySinkTypeIndex, arraySinkObject, arraySinkType, numWantedBytes,
                                   isPacked ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                   options.allowDropEccessArrayItems ? ArrayAccess::DropEccessItems::Yes
                                                                     : ArrayAccess::DropEccessItems::No))
            return false;
        if (not arrayAccess.getSegmentSpan(arraySinkTypeIndex, arraySinkType, arraySinkObject, arraySinkStart))
            return false;
    }
    if (isPacked)
    {
        const auto minBytes = min(static_cast<uint64_t>(arraySinkStart.sizeInBytes()), sourceNumBytes);
        if (not sourceObject->serializeBytes(Span<uint8_t>{arraySinkStart.data(), static_cast<size_t>(minBytes)}))
            return false;
        if (sourceNumBytes > static_cast<uint64_t>(arraySinkStart.sizeInBytes()))
        {
            // We must consume these excess bytes anyway, discarding their content
            if (not options.allowDropEccessArrayItems)
                return false;
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
            if (not arraySinkStart.sliceStartLength(static_cast<size_t>(idx * sinkItemSize), sinkItemSize, sinkObject))
                return false;
            if (not read())
                return false;
        }
        if (sourceNumElements > sinkNumElements)
        {
            // We must consume these excess items anyway, discarding their content
            if (not options.allowDropEccessArrayItems)
                return false;
            for (uint32_t idx = 0; idx < sourceNumElements - sinkNumElements; ++idx)
            {
                sourceTypeIndex = itemSourceTypeIndex;
                if (not skipCurrent())
                    return false;
            }
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::ReadVersioned::skipCurrent()
{
    SerializationBinary::detail::Skipper<SerializationBinary::Buffer> skipper(*sourceObject, sourceTypeIndex);
    skipper.sourceTypes = sourceTypes;
    return skipper.skip();
}
