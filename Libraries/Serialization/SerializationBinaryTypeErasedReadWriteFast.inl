// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SerializationBinarySkipper.h"
#include "SerializationBinaryTypeErasedReadWriteFast.h"

bool SC::SerializationBinaryTypeErased::ArrayAccess::getSegmentSpan(uint32_t                   linkID,
                                                                    Reflection::MetaProperties property,
                                                                    SpanVoid<void> object, SpanVoid<void>& itemBegin)
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
                                                                    SpanVoid<const void>       object,
                                                                    SpanVoid<const void>&      itemBegin)
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

bool SC::SerializationBinaryTypeErased::ArrayAccess::resize(uint32_t linkID, SpanVoid<void> object,
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

bool SC::Serialization::BinaryBuffer::serialize(SpanVoid<const void> object)
{
    Span<const uint8_t> bytes = object.castTo<const uint8_t>();
    numberOfOperations++;
    return buffer.appendCopy(bytes.data(), bytes.sizeInBytes());
}

bool SC::Serialization::BinaryBuffer::serialize(SpanVoid<void> object)
{
    if (index + object.sizeInBytes() > buffer.size())
        return false;
    numberOfOperations++;
    Span<uint8_t> bytes = object.castTo<uint8_t>();
    memcpy(bytes.data(), &buffer[index], bytes.sizeInBytes());
    index += bytes.sizeInBytes();
    return true;
}

bool SC::Serialization::BinaryBuffer::advance(size_t numBytes)
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
        SpanVoid<const void> primitiveSpan;
        SC_TRY_IF(sourceObject.viewAtBytes(0, sourceProperty.sizeInBytes, primitiveSpan));
        SC_TRY_IF(destination.serialize(primitiveSpan));
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
    const auto           structSourceProperty  = sourceProperty;
    const auto           structSourceTypeIndex = sourceTypeIndex;
    SpanVoid<const void> structSourceRoot      = sourceObject;

    const bool isPacked = structSourceProperty.getCustomUint32() & Reflection::MetaStructFlags::IsPacked;
    if (isPacked)
    {
        // Bulk Write the entire struct
        SpanVoid<const void> structSpan;
        SC_TRY_IF(sourceObject.viewAtBytes(0, structSourceProperty.sizeInBytes, structSpan));
        SC_TRY_IF(destination.serialize(structSpan));
    }
    else
    {
        for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSourceProperty.numSubAtoms); ++idx)
        {
            sourceTypeIndex = structSourceTypeIndex + idx + 1;
            SC_TRY_IF(structSourceRoot.viewAtBytes(sourceProperties.data()[sourceTypeIndex].offsetInBytes,
                                                   sourceProperties.data()[sourceTypeIndex].sizeInBytes, sourceObject));
            if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());
            SC_TRY_IF(write());
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SerializerReadWriteFast::writeArrayVector()
{
    const auto           arrayProperty  = sourceProperty;
    const auto           arrayTypeIndex = sourceTypeIndex;
    SpanVoid<const void> arraySpan;
    uint64_t             numBytes = 0;
    if (arrayProperty.type == Reflection::MetaType::TypeArray)
    {
        SC_TRY_IF(sourceObject.viewAtBytes(0, arrayProperty.sizeInBytes, arraySpan));
        numBytes = arrayProperty.sizeInBytes;
    }
    else
    {
        SC_TRY_IF(arrayAccess.getSegmentSpan(arrayTypeIndex, arrayProperty, sourceObject, arraySpan));
        numBytes = arraySpan.sizeInBytes();
        SC_TRY_IF(destination.serialize(SpanVoid<const void>(&numBytes, sizeof(numBytes))));
    }
    sourceTypeIndex     = arrayTypeIndex + 1;
    const auto itemSize = sourceProperties.data()[sourceTypeIndex].sizeInBytes;
    if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
        sourceTypeIndex = static_cast<uint32_t>(sourceProperties.data()[sourceTypeIndex].getLinkIndex());

    const bool isPacked = sourceProperties.data()[sourceTypeIndex].isPrimitiveOrRecursivelyPacked();
    if (isPacked)
    {
        SC_TRY_IF(destination.serialize(arraySpan));
    }
    else
    {
        const auto numElements   = numBytes / itemSize;
        const auto itemTypeIndex = sourceTypeIndex;
        for (uint64_t idx = 0; idx < numElements; ++idx)
        {
            sourceTypeIndex = itemTypeIndex;
            SC_TRY_IF(arraySpan.viewAtBytes(idx * itemSize, itemSize, sourceObject));
            SC_TRY_IF(write());
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SimpleBinaryReader::read()
{
    sinkProperty = sinkProperties.data()[sinkTypeIndex];
    if (sinkProperty.isPrimitiveType())
    {
        SpanVoid<void> primitiveSpan;
        SC_TRY_IF(sinkObject.viewAtBytes(0, sinkProperty.sizeInBytes, primitiveSpan));
        SC_TRY_IF(source.serialize(primitiveSpan));
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
    const auto     structSinkProperty  = sinkProperty;
    const auto     structSinkTypeIndex = sinkTypeIndex;
    SpanVoid<void> structSinkObject    = sinkObject;
    const bool     IsPacked            = structSinkProperty.getCustomUint32() & Reflection::MetaStructFlags::IsPacked;

    if (IsPacked)
    {
        // Bulk read the entire struct
        SpanVoid<void> structSpan;
        SC_TRY_IF(sinkObject.viewAtBytes(0, structSinkProperty.sizeInBytes, structSpan));
        SC_TRY_IF(source.serialize(structSpan));
    }
    else
    {
        for (uint32_t idx = 0; idx < static_cast<uint32_t>(structSinkProperty.numSubAtoms); ++idx)
        {
            sinkTypeIndex = structSinkTypeIndex + idx + 1;
            SC_TRY_IF(structSinkObject.viewAtBytes(sinkProperties.data()[sinkTypeIndex].offsetInBytes,
                                                   sinkProperties.data()[sinkTypeIndex].sizeInBytes, sinkObject));
            if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
                sinkTypeIndex = static_cast<uint32_t>(sinkProperties.data()[sinkTypeIndex].getLinkIndex());
            SC_TRY_IF(read());
        }
    }
    return true;
}

bool SC::SerializationBinaryTypeErased::SimpleBinaryReader::readArrayVector()
{
    const auto arraySinkProperty   = sinkProperty;
    const auto arraySinkTypeIndex  = sinkTypeIndex;
    sinkTypeIndex                  = arraySinkTypeIndex + 1;
    SpanVoid<void> arraySinkObject = sinkObject;
    const auto     sinkItemSize    = sinkProperties.data()[sinkTypeIndex].sizeInBytes;
    if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
        sinkTypeIndex = static_cast<uint32_t>(sinkProperties.data()[sinkTypeIndex].getLinkIndex());
    const bool     isBulkReadable = sinkProperties.data()[sinkTypeIndex].isPrimitiveOrRecursivelyPacked();
    SpanVoid<void> arraySinkStart;
    if (arraySinkProperty.type == Reflection::MetaType::TypeArray)
    {
        SC_TRY_IF(arraySinkObject.viewAtBytes(0, arraySinkProperty.sizeInBytes, arraySinkStart));
    }
    else
    {
        uint64_t sinkNumBytes = 0;
        SC_TRY_IF(source.serialize(SpanVoid<void>(&sinkNumBytes, sizeof(sinkNumBytes))));

        SC_TRY_IF(arrayAccess.resize(arraySinkTypeIndex, arraySinkObject, arraySinkProperty, sinkNumBytes,
                                     isBulkReadable ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                     ArrayAccess::DropEccessItems::No));
        SC_TRY_IF(arrayAccess.getSegmentSpan(arraySinkTypeIndex, arraySinkProperty, arraySinkObject, arraySinkStart));
    }
    if (isBulkReadable)
    {
        SC_TRY_IF(source.serialize(arraySinkStart));
    }
    else
    {
        const auto sinkNumElements   = arraySinkStart.sizeInBytes() / sinkItemSize;
        const auto itemSinkTypeIndex = sinkTypeIndex;
        for (uint64_t idx = 0; idx < sinkNumElements; ++idx)
        {
            sinkTypeIndex = itemSinkTypeIndex;
            SC_TRY_IF(arraySinkStart.viewAtBytes(idx * sinkItemSize, sinkItemSize, sinkObject));
            SC_TRY_IF(read());
        }
    }
    return true;
}
