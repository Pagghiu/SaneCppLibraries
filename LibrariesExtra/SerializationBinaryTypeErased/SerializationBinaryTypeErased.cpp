// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Libraries/SerializationBinary/Internal/SerializationBinaryBuffer.h"
#include "Internal/SerializationBinaryTypeErasedReadVersioned.h"
#include "Internal/SerializationBinaryTypeErasedReadWriteExact.h"

namespace SC
{
namespace detail
{
[[nodiscard]] static bool tryPrimitiveConversion(const SerializationBinaryOptions& options,
                                                 const Reflection::TypeInfo&       sourceType,
                                                 SerializationBinaryBufferReader*  sourceObject,
                                                 const Reflection::TypeInfo& sinkType, Span<uint8_t>& sinkObject);
}
} // namespace SC

//-------------------------------------------------------------------------------------------------
// SerializationBinaryTypeErasedReadVersioned
//-------------------------------------------------------------------------------------------------
bool SC::SerializationBinaryTypeErasedReadVersioned::read()
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
            return SC::detail::tryPrimitiveConversion(options, sourceType, sourceObject, sinkType, sinkObject);
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

bool SC::SerializationBinaryTypeErasedReadVersioned::readStruct()
{
    if (sinkType.type != Reflection::TypeCategory::TypeStruct)
    {
        return false;
    }

    Span<uint8_t> structSinkObject = sinkObject;

    const auto structSourceType      = sourceType;
    const auto structSourceTypeIndex = sourceTypeIndex;
    const auto structSinkType        = sinkType;
    const auto structSinkTypeIndex   = sinkTypeIndex;

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

bool SC::SerializationBinaryTypeErasedReadVersioned::readArrayVector()
{
    if (sinkType.type != Reflection::TypeCategory::TypeArray && sinkType.type != Reflection::TypeCategory::TypeVector)
    {
        return false;
    }
    Span<uint8_t> arraySinkObject = sinkObject;

    const auto arraySourceType      = sourceType;
    const auto arraySourceTypeIndex = sourceTypeIndex;
    const auto arraySinkTypeIndex   = sinkTypeIndex;
    const auto arraySinkType        = sinkType;

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
        using ArrayAccess         = detail::SerializationBinaryTypeErasedArrayAccess;
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

bool SC::SerializationBinaryTypeErasedReadVersioned::skipCurrent()
{
    SC::detail::SerializationBinarySkipper<SerializationBinaryBufferReader> skipper(*sourceObject, sourceTypeIndex);
    skipper.sourceTypes = sourceTypes;
    return skipper.skip();
}

//-------------------------------------------------------------------------------------------------
// SerializationBinaryTypeErasedWriteExact
//-------------------------------------------------------------------------------------------------

bool SC::SerializationBinaryTypeErasedWriteExact::write()
{
    sourceType = sourceTypes.data()[sourceTypeIndex];
    if (sourceType.isPrimitiveType())
    {
        Span<const uint8_t> primitiveSpan;
        if (not sourceObject.sliceStartLength(0, sourceType.sizeInBytes, primitiveSpan))
            return false;
        if (not destination->serializeBytes(primitiveSpan))
            return false;
        return true;
    }
    else if (sourceType.type == Reflection::TypeCategory::TypeStruct)
    {
        return writeStruct();
    }
    else if (sourceType.type == Reflection::TypeCategory::TypeArray ||
             sourceType.type == Reflection::TypeCategory::TypeVector)
    {
        return writeArrayVector();
    }
    return false;
}

bool SC::SerializationBinaryTypeErasedWriteExact::writeStruct()
{
    Span<const uint8_t> structSourceRoot = sourceObject;

    const auto structSourceType      = sourceType;
    const auto structSourceTypeIndex = sourceTypeIndex;
    if (structSourceType.structInfo.isPacked)
    {
        // Bulk Write the entire struct
        Span<const uint8_t> structSpan;
        if (not sourceObject.sliceStartLength(0, structSourceType.sizeInBytes, structSpan))
            return false;
        if (not destination->serializeBytes(structSpan))
            return false;
    }
    else
    {
        for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceType.getNumberOfChildren()); ++idx)
        {
            sourceTypeIndex             = structSourceTypeIndex + idx + 1;
            const auto sourceMemberType = sourceTypes.data()[sourceTypeIndex];
            if (not structSourceRoot.sliceStartLength(sourceMemberType.memberInfo.offsetInBytes,
                                                      sourceMemberType.sizeInBytes, sourceObject))
                return false;
            if (sourceMemberType.hasValidLinkIndex())
                sourceTypeIndex = static_cast<uint32_t>(sourceMemberType.getLinkIndex());
            if (not write())
                return false;
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErasedWriteExact::writeArrayVector()
{
    Span<const uint8_t> arraySpan;

    const auto arrayProperty  = sourceType;
    const auto arrayTypeIndex = sourceTypeIndex;
    uint64_t   numBytes       = 0;
    if (arrayProperty.type == Reflection::TypeCategory::TypeArray)
    {
        if (not sourceObject.sliceStartLength(0, arrayProperty.sizeInBytes, arraySpan))
            return false;
        numBytes = arrayProperty.sizeInBytes;
    }
    else
    {
        if (not arrayAccess.getSegmentSpan(arrayTypeIndex, arrayProperty, sourceObject, arraySpan))
            return false;
        numBytes = arraySpan.sizeInBytes();
        if (not destination->serializeBytes(Span<const uint8_t>::reinterpret_object(numBytes)))
            return false;
    }
    sourceTypeIndex     = arrayTypeIndex + 1;
    const auto itemSize = sourceTypes.data()[sourceTypeIndex].sizeInBytes;
    if (sourceTypes.data()[sourceTypeIndex].hasValidLinkIndex())
        sourceTypeIndex = static_cast<uint32_t>(sourceTypes.data()[sourceTypeIndex].getLinkIndex());

    const bool isPacked = sourceTypes.data()[sourceTypeIndex].isPrimitiveOrPackedStruct();
    if (isPacked)
    {
        if (not destination->serializeBytes(arraySpan))
            return false;
    }
    else
    {
        const auto numElements   = numBytes / itemSize;
        const auto itemTypeIndex = sourceTypeIndex;
        for (uint64_t idx = 0; idx < numElements; ++idx)
        {
            sourceTypeIndex = itemTypeIndex;
            if (not arraySpan.sliceStartLength(static_cast<size_t>(idx * itemSize), itemSize, sourceObject))
                return false;
            if (not write())
                return false;
        }
    }
    return true;
}

//-------------------------------------------------------------------------------------------------
// SerializationBinaryTypeErasedReadExact
//-------------------------------------------------------------------------------------------------

bool SC::SerializationBinaryTypeErasedReadExact::read()
{
    sinkType = sinkTypes.data()[sinkTypeIndex];
    if (sinkType.isPrimitiveType())
    {
        Span<uint8_t> primitiveSpan;
        if (not sinkObject.sliceStartLength(0, sinkType.sizeInBytes, primitiveSpan))
            return false;
        return source->serializeBytes(primitiveSpan);
    }
    else if (sinkType.type == Reflection::TypeCategory::TypeStruct)
    {
        return readStruct();
    }
    else if (sinkType.type == Reflection::TypeCategory::TypeArray ||
             sinkType.type == Reflection::TypeCategory::TypeVector)
    {
        return readArrayVector();
    }
    return false;
}

bool SC::SerializationBinaryTypeErasedReadExact::readStruct()
{
    const auto    structSinkType      = sinkType;
    const auto    structSinkTypeIndex = sinkTypeIndex;
    Span<uint8_t> structSinkObject    = sinkObject;

    if (structSinkType.structInfo.isPacked)
    {
        // Bulk read the entire struct
        Span<uint8_t> structSpan;
        if (not sinkObject.sliceStartLength(0, structSinkType.sizeInBytes, structSpan))
            return false;
        if (not source->serializeBytes(structSpan))
            return false;
    }
    else
    {
        for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSinkType.getNumberOfChildren()); ++idx)
        {
            sinkTypeIndex             = structSinkTypeIndex + idx + 1;
            const auto sinkMemberType = sinkTypes.data()[sinkTypeIndex];
            if (not structSinkObject.sliceStartLength(sinkMemberType.memberInfo.offsetInBytes,
                                                      sinkMemberType.sizeInBytes, sinkObject))
                return false;
            if (sinkMemberType.hasValidLinkIndex())
                sinkTypeIndex = static_cast<uint32_t>(sinkMemberType.getLinkIndex());
            if (not read())
                return false;
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErasedReadExact::readArrayVector()
{
    const auto arraySinkType      = sinkType;
    const auto arraySinkTypeIndex = sinkTypeIndex;
    sinkTypeIndex                 = arraySinkTypeIndex + 1;
    Span<uint8_t> arraySinkObject = sinkObject;
    const auto    sinkItemSize    = sinkTypes.data()[sinkTypeIndex].sizeInBytes;
    if (sinkTypes.data()[sinkTypeIndex].hasValidLinkIndex())
        sinkTypeIndex = static_cast<uint32_t>(sinkTypes.data()[sinkTypeIndex].getLinkIndex());
    const bool    isBulkReadable = sinkTypes.data()[sinkTypeIndex].isPrimitiveOrPackedStruct();
    Span<uint8_t> arraySinkStart;
    if (arraySinkType.type == Reflection::TypeCategory::TypeArray)
    {
        if (not arraySinkObject.sliceStartLength(0, arraySinkType.sizeInBytes, arraySinkStart))
            return false;
    }
    else
    {
        uint64_t sinkNumBytes = 0;
        if (not source->serializeBytes(Span<uint8_t>::reinterpret_object(sinkNumBytes)))
            return false;
        using ArrayAccess = detail::SerializationBinaryTypeErasedArrayAccess;
        if (not arrayAccess.resize(arraySinkTypeIndex, arraySinkObject, arraySinkType, sinkNumBytes,
                                   isBulkReadable ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                   ArrayAccess::DropEccessItems::No))
            return false;
        if (not arrayAccess.getSegmentSpan(arraySinkTypeIndex, arraySinkType, arraySinkObject, arraySinkStart))
            return false;
    }
    if (isBulkReadable)
    {
        if (not source->serializeBytes(arraySinkStart))
            return false;
    }
    else
    {
        const auto sinkNumElements   = arraySinkStart.sizeInBytes() / sinkItemSize;
        const auto itemSinkTypeIndex = sinkTypeIndex;
        for (uint64_t idx = 0; idx < sinkNumElements; ++idx)
        {
            sinkTypeIndex = itemSinkTypeIndex;
            if (not arraySinkStart.sliceStartLength(static_cast<size_t>(idx * sinkItemSize), sinkItemSize, sinkObject))
                return false;
            if (not read())
                return false;
        }
    }
    return true;
}

//-------------------------------------------------------------------------------------------------
// SerializationBinaryTypeErasedArrayAccess
//-------------------------------------------------------------------------------------------------

bool SC::detail::SerializationBinaryTypeErasedArrayAccess::getSegmentSpan(uint32_t             linkID,
                                                                          Reflection::TypeInfo property,
                                                                          Span<uint8_t>        object,
                                                                          Span<uint8_t>&       itemBegin)
{
    for (uint32_t index = 0; index < vectorVtable.sizeInBytes(); ++index)
    {
        if (vectorVtable.data()[index].linkID == linkID)
        {
            return vectorVtable.data()[index].getSegmentSpan(property, object, itemBegin);
        }
    }
    return false;
}

bool SC::detail::SerializationBinaryTypeErasedArrayAccess::getSegmentSpan(uint32_t             linkID,
                                                                          Reflection::TypeInfo property,
                                                                          Span<const uint8_t>  object,
                                                                          Span<const uint8_t>& itemBegin)
{
    for (uint32_t index = 0; index < vectorVtable.sizeInBytes(); ++index)
    {
        if (vectorVtable.data()[index].linkID == linkID)
        {
            return vectorVtable.data()[index].getSegmentSpanConst(property, object, itemBegin);
        }
    }
    return false;
}

bool SC::detail::SerializationBinaryTypeErasedArrayAccess::resize(uint32_t linkID, Span<uint8_t> object,
                                                                  Reflection::TypeInfo property, uint64_t sizeInBytes,
                                                                  Initialize      initialize,
                                                                  DropEccessItems dropEccessItems)
{
    for (uint32_t index = 0; index < vectorVtable.sizeInBytes(); ++index)
    {
        if (vectorVtable.data()[index].linkID == linkID)
        {
            if (initialize == Initialize::Yes)
            {
                return vectorVtable.data()[index].resize(object, property, sizeInBytes, dropEccessItems);
            }
            else
            {
                return vectorVtable.data()[index].resizeWithoutInitialize(object, property, sizeInBytes,
                                                                          dropEccessItems);
            }
        }
    }
    return false;
}
//-------------------------------------------------------------------------------------------------
// SerializationBinaryTypeErasedArrayAccess
//-------------------------------------------------------------------------------------------------

namespace SC
{
namespace detail
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
[[nodiscard]] static bool tryReadPrimitiveValue(SerializationBinaryBufferReader* sourceObject,
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

[[nodiscard]] static bool tryPrimitiveConversion(const SerializationBinaryOptions& options,
                                                 const Reflection::TypeInfo&       sourceType,
                                                 SerializationBinaryBufferReader*  sourceObject,
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
} // namespace detail
} // namespace SC
