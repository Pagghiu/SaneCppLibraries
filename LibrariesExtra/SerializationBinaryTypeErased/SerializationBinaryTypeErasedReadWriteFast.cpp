// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "SerializationBinaryTypeErasedReadWriteFast.h"

bool SC::SerializationBinaryTypeErased::detail::ArrayAccess::getSegmentSpan(uint32_t             linkID,
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

bool SC::SerializationBinaryTypeErased::detail::ArrayAccess::getSegmentSpan(uint32_t             linkID,
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

bool SC::SerializationBinaryTypeErased::detail::ArrayAccess::resize(uint32_t linkID, Span<uint8_t> object,
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

bool SC::SerializationBinaryTypeErased::WriteFast::write()
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

bool SC::SerializationBinaryTypeErased::WriteFast::writeStruct()
{
    const auto          structSourceType      = sourceType;
    const auto          structSourceTypeIndex = sourceTypeIndex;
    Span<const uint8_t> structSourceRoot      = sourceObject;

    const bool isPacked = structSourceType.structInfo.isPacked;
    if (isPacked)
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

bool SC::SerializationBinaryTypeErased::WriteFast::writeArrayVector()
{
    const auto          arrayProperty  = sourceType;
    const auto          arrayTypeIndex = sourceTypeIndex;
    Span<const uint8_t> arraySpan;
    uint64_t            numBytes = 0;
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

bool SC::SerializationBinaryTypeErased::ReadFast::read()
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

bool SC::SerializationBinaryTypeErased::ReadFast::readStruct()
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

bool SC::SerializationBinaryTypeErased::ReadFast::readArrayVector()
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
        using ArrayAccess = SerializationBinaryTypeErased::detail::ArrayAccess;
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
