// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SerializationBinarySkipper.h"
#include "SerializationBinaryTypeErasedReadWriteFast.h"

bool SC::SerializationBinaryTypeErased::ArrayAccess::getSegmentSpan(uint32_t                   linkID,
                                                                    Reflection::MetaProperties property,
                                                                    Span<uint8_t> object, Span<uint8_t>& itemBegin)
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

bool SC::SerializationBinaryTypeErased::ArrayAccess::getSegmentSpan(uint32_t                   linkID,
                                                                    Reflection::MetaProperties property,
                                                                    Span<const uint8_t>        object,
                                                                    Span<const uint8_t>&       itemBegin)
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

bool SC::SerializationBinaryTypeErased::ArrayAccess::resize(uint32_t linkID, Span<uint8_t> object,
                                                            Reflection::MetaProperties property, uint64_t sizeInBytes,
                                                            Initialize initialize, DropEccessItems dropEccessItems)
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

bool SC::Serialization::BinaryBuffer::serializeBytes(Span<const uint8_t> object)
{
    numberOfOperations++;
    return buffer.append(object);
}

bool SC::Serialization::BinaryBuffer::serializeBytes(Span<uint8_t> object)
{
    if (index + object.sizeInBytes() > buffer.size())
        return false;
    numberOfOperations++;
    memcpy(object.data(), &buffer[index], object.sizeInBytes());
    index += object.sizeInBytes();
    return true;
}

bool SC::Serialization::BinaryBuffer::advanceBytes(size_t numBytes)
{
    if (index + numBytes > buffer.size())
        return false;
    index += numBytes;
    return true;
}

bool SC::SerializationBinaryTypeErased::SerializerReadWriteFast::write()
{
    sourceProperty = sourceProperties.data()[sourceTypeIndex];
    if (sourceProperty.isPrimitiveType())
    {
        Span<const uint8_t> primitiveSpan;
        SC_TRY(sourceObject.sliceStartLength(0, sourceProperty.sizeInBytes, primitiveSpan));
        SC_TRY(destination.serializeBytes(primitiveSpan));
        return true;
    }
    else if (sourceProperty.type == Reflection::MetaType::TypeStruct)
    {
        return writeStruct();
    }
    else if (sourceProperty.type == Reflection::MetaType::TypeArray ||
             sourceProperty.type == Reflection::MetaType::TypeVector)
    {
        return writeArrayVector();
    }
    return false;
}

bool SC::SerializationBinaryTypeErased::SerializerReadWriteFast::writeStruct()
{
    const auto          structSourceProperty  = sourceProperty;
    const auto          structSourceTypeIndex = sourceTypeIndex;
    Span<const uint8_t> structSourceRoot      = sourceObject;

    const bool isPacked = structSourceProperty.getCustomUint32() & Reflection::MetaStructFlags::IsPacked;
    if (isPacked)
    {
        // Bulk Write the entire struct
        Span<const uint8_t> structSpan;
        SC_TRY(sourceObject.sliceStartLength(0, structSourceProperty.sizeInBytes, structSpan));
        SC_TRY(destination.serializeBytes(structSpan));
    }
    else
    {
        for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceProperty.numSubAtoms); ++idx)
        {
            sourceTypeIndex = structSourceTypeIndex + idx + 1;
            SC_TRY(structSourceRoot.sliceStartLength(sourceProperties.data()[sourceTypeIndex].offsetInBytes,
                                                     sourceProperties.data()[sourceTypeIndex].sizeInBytes,
                                                     sourceObject));
            if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());
            SC_TRY(write());
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SerializerReadWriteFast::writeArrayVector()
{
    const auto          arrayProperty  = sourceProperty;
    const auto          arrayTypeIndex = sourceTypeIndex;
    Span<const uint8_t> arraySpan;
    uint64_t            numBytes = 0;
    if (arrayProperty.type == Reflection::MetaType::TypeArray)
    {
        SC_TRY(sourceObject.sliceStartLength(0, arrayProperty.sizeInBytes, arraySpan));
        numBytes = arrayProperty.sizeInBytes;
    }
    else
    {
        SC_TRY(arrayAccess.getSegmentSpan(arrayTypeIndex, arrayProperty, sourceObject, arraySpan));
        numBytes = arraySpan.sizeInBytes();
        SC_TRY(destination.serializeBytes(Span<const uint8_t>::reinterpret_object(numBytes)));
    }
    sourceTypeIndex     = arrayTypeIndex + 1;
    const auto itemSize = sourceProperties.data()[sourceTypeIndex].sizeInBytes;
    if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
        sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());

    const bool isPacked = sourceProperties.data()[sourceTypeIndex].isPrimitiveOrRecursivelyPacked();
    if (isPacked)
    {
        SC_TRY(destination.serializeBytes(arraySpan));
    }
    else
    {
        const auto numElements   = numBytes / itemSize;
        const auto itemTypeIndex = sourceTypeIndex;
        for (uint64_t idx = 0; idx < numElements; ++idx)
        {
            sourceTypeIndex = itemTypeIndex;
            SC_TRY(arraySpan.sliceStartLength(static_cast<size_t>(idx * itemSize), itemSize, sourceObject));
            SC_TRY(write());
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SimpleBinaryReader::read()
{
    sinkProperty = sinkProperties.data()[sinkTypeIndex];
    if (sinkProperty.isPrimitiveType())
    {
        Span<uint8_t> primitiveSpan;
        SC_TRY(sinkObject.sliceStartLength(0, sinkProperty.sizeInBytes, primitiveSpan));
        SC_TRY(source.serializeBytes(primitiveSpan));
        return true;
    }
    else if (sinkProperty.type == Reflection::MetaType::TypeStruct)
    {
        return readStruct();
    }
    else if (sinkProperty.type == Reflection::MetaType::TypeArray ||
             sinkProperty.type == Reflection::MetaType::TypeVector)
    {
        return readArrayVector();
    }
    return false;
}

bool SC::SerializationBinaryTypeErased::SimpleBinaryReader::readStruct()
{
    const auto    structSinkProperty  = sinkProperty;
    const auto    structSinkTypeIndex = sinkTypeIndex;
    Span<uint8_t> structSinkObject    = sinkObject;
    const bool    IsPacked            = structSinkProperty.getCustomUint32() & Reflection::MetaStructFlags::IsPacked;

    if (IsPacked)
    {
        // Bulk read the entire struct
        Span<uint8_t> structSpan;
        SC_TRY(sinkObject.sliceStartLength(0, structSinkProperty.sizeInBytes, structSpan));
        SC_TRY(source.serializeBytes(structSpan));
    }
    else
    {
        for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSinkProperty.numSubAtoms); ++idx)
        {
            sinkTypeIndex = structSinkTypeIndex + idx + 1;
            SC_TRY(structSinkObject.sliceStartLength(sinkProperties.data()[sinkTypeIndex].offsetInBytes,
                                                     sinkProperties.data()[sinkTypeIndex].sizeInBytes, sinkObject));
            if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
                sinkTypeIndex = static_cast<uint32_t>(sinkProperties.data()[sinkTypeIndex].getLinkIndex());
            SC_TRY(read());
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SimpleBinaryReader::readArrayVector()
{
    const auto arraySinkProperty  = sinkProperty;
    const auto arraySinkTypeIndex = sinkTypeIndex;
    sinkTypeIndex                 = arraySinkTypeIndex + 1;
    Span<uint8_t> arraySinkObject = sinkObject;
    const auto    sinkItemSize    = sinkProperties.data()[sinkTypeIndex].sizeInBytes;
    if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
        sinkTypeIndex = static_cast<uint32_t>(sinkProperties.data()[sinkTypeIndex].getLinkIndex());
    const bool    isBulkReadable = sinkProperties.data()[sinkTypeIndex].isPrimitiveOrRecursivelyPacked();
    Span<uint8_t> arraySinkStart;
    if (arraySinkProperty.type == Reflection::MetaType::TypeArray)
    {
        SC_TRY(arraySinkObject.sliceStartLength(0, arraySinkProperty.sizeInBytes, arraySinkStart));
    }
    else
    {
        uint64_t sinkNumBytes = 0;
        SC_TRY(source.serializeBytes(Span<uint8_t>::reinterpret_object(sinkNumBytes)));

        SC_TRY(arrayAccess.resize(arraySinkTypeIndex, arraySinkObject, arraySinkProperty, sinkNumBytes,
                                  isBulkReadable ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                  ArrayAccess::DropEccessItems::No));
        SC_TRY(arrayAccess.getSegmentSpan(arraySinkTypeIndex, arraySinkProperty, arraySinkObject, arraySinkStart));
    }
    if (isBulkReadable)
    {
        SC_TRY(source.serializeBytes(arraySinkStart));
    }
    else
    {
        const auto sinkNumElements   = arraySinkStart.sizeInBytes() / sinkItemSize;
        const auto itemSinkTypeIndex = sinkTypeIndex;
        for (uint64_t idx = 0; idx < sinkNumElements; ++idx)
        {
            sinkTypeIndex = itemSinkTypeIndex;
            SC_TRY(arraySinkStart.sliceStartLength(static_cast<size_t>(idx * sinkItemSize), sinkItemSize, sinkObject));
            SC_TRY(read());
        }
    }
    return true;
}
