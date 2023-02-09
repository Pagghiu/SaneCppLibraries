// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Reflection/ReflectionSC.h"
#include "SerializationBinarySkipper.h"
#include "SerializationTypeErasedCompiler.h"
namespace SC
{
namespace SerializationTypeErased
{
struct BinaryBuffer
{
    size_t              index = 0;
    SC::Vector<uint8_t> buffer;
    int                 numberOfOperations = 0;

    [[nodiscard]] bool serialize(SpanVoid<const void> object)
    {
        Span<const uint8_t> bytes = object.castTo<const uint8_t>();
        numberOfOperations++;
        return buffer.appendCopy(bytes.data(), bytes.sizeInBytes());
    }

    [[nodiscard]] bool serialize(SpanVoid<void> object)
    {
        if (index + object.sizeInBytes() > buffer.size())
            return false;
        numberOfOperations++;
        Span<uint8_t> bytes = object.castTo<uint8_t>();
        memcpy(bytes.data(), &buffer[index], bytes.sizeInBytes());
        index += bytes.sizeInBytes();
        return true;
    }

    [[nodiscard]] bool advance(size_t numBytes)
    {
        if (index + numBytes > buffer.size())
            return false;
        index += numBytes;
        return true;
    }
};

struct ArrayAccess
{
    Span<const Reflection::VectorVTable> vectorVtable;

    [[nodiscard]] bool getSegmentSpan(uint32_t linkID, Reflection::MetaProperties property, SpanVoid<void> object,
                                      SpanVoid<void>& itemBegin)
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
    [[nodiscard]] bool getSegmentSpan(uint32_t linkID, Reflection::MetaProperties property, SpanVoid<const void> object,
                                      SpanVoid<const void>& itemBegin)
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

    typedef Reflection::VectorVTable::DropEccessItems DropEccessItems;
    enum class Initialize
    {
        No,
        Yes
    };

    bool resize(uint32_t linkID, SpanVoid<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
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
};

struct SimpleBinaryWriter
{
    Span<const Reflection::MetaProperties> sourceProperties;
    Span<const SC::ConstexprStringView>    sourceNames;
    BinaryBuffer&                          destination;
    SpanVoid<const void>                   sourceObject;
    int                                    sourceTypeIndex;
    Reflection::MetaProperties             sourceProperty;
    ArrayAccess                            arrayAccess;

    SimpleBinaryWriter(BinaryBuffer& destination) : destination(destination) {}

    template <typename T>
    [[nodiscard]] constexpr bool serialize(const T& object)
    {
        constexpr auto flatSchema      = Reflection::FlatSchemaTypeErased::compile<T>();
        sourceProperties               = flatSchema.propertiesAsSpan();
        sourceNames                    = flatSchema.namesAsSpan();
        arrayAccess.vectorVtable       = {flatSchema.payload.vector.values,
                                          static_cast<size_t>(flatSchema.payload.vector.size)};
        sourceObject                   = SpanVoid<const void>(&object, sizeof(T));
        sourceTypeIndex                = 0;
        destination.numberOfOperations = 0;
        if (sourceProperties.sizeInBytes() == 0 || sourceProperties.data()[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return write();
    }

    [[nodiscard]] constexpr bool write()
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

    [[nodiscard]] constexpr bool writeStruct()
    {
        const auto           structSourceProperty  = sourceProperty;
        const auto           structSourceTypeIndex = sourceTypeIndex;
        SpanVoid<const void> structSourceRoot      = sourceObject;

        const bool isBulkWriteable = structSourceProperty.getCustomUint32() & Reflection::MetaStructFlags::IsPacked;
        if (isBulkWriteable)
        {
            // Bulk Write the entire struct
            SpanVoid<const void> structSpan;
            SC_TRY_IF(sourceObject.viewAtBytes(0, structSourceProperty.sizeInBytes, structSpan));
            SC_TRY_IF(destination.serialize(structSpan));
        }
        else
        {
            for (int16_t idx = 0; idx < structSourceProperty.numSubAtoms; ++idx)
            {
                sourceTypeIndex = structSourceTypeIndex + idx + 1;
                SC_TRY_IF(structSourceRoot.viewAtBytes(sourceProperties.data()[sourceTypeIndex].offsetInBytes,
                                                       sourceProperties.data()[sourceTypeIndex].sizeInBytes,
                                                       sourceObject));
                if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = sourceProperties.data()[sourceTypeIndex].getLinkIndex();
                SC_TRY_IF(write());
            }
        }
        return true;
    }

    [[nodiscard]] constexpr bool writeArrayVector()
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
            sourceTypeIndex = sourceProperties.data()[sourceTypeIndex].getLinkIndex();

        const bool isBulkWriteable = sourceProperties.data()[sourceTypeIndex].isPrimitiveOrRecursivelyPacked();
        if (isBulkWriteable)
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
};

struct SimpleBinaryReader
{
    Span<const Reflection::MetaProperties> sinkProperties;
    Span<const SC::ConstexprStringView>    sinkNames;
    Reflection::MetaProperties             sinkProperty;
    int                                    sinkTypeIndex = 0;
    SpanVoid<void>                         sinkObject;
    BinaryBuffer&                          source;
    ArrayAccess                            arrayAccess;

    SimpleBinaryReader(BinaryBuffer& source) : source(source) {}

    template <typename T>
    [[nodiscard]] bool serialize(T& object)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaTypeErased::compile<T>();

        sinkProperties = flatSchema.propertiesAsSpan();
        sinkNames      = flatSchema.namesAsSpan();
        sinkObject     = SpanVoid<void>(&object, sizeof(T));
        sinkTypeIndex  = 0;

        arrayAccess.vectorVtable = {flatSchema.payload.vector.values,
                                    static_cast<size_t>(flatSchema.payload.vector.size)};

        if (sinkProperties.sizeInBytes() == 0 || sinkProperties.data()[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return read();
    }

    [[nodiscard]] bool read()
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

    [[nodiscard]] bool readStruct()
    {
        const auto     structSinkProperty  = sinkProperty;
        const auto     structSinkTypeIndex = sinkTypeIndex;
        SpanVoid<void> structSinkObject    = sinkObject;
        const bool     IsPacked = structSinkProperty.getCustomUint32() & Reflection::MetaStructFlags::IsPacked;

        if (IsPacked)
        {
            // Bulk read the entire struct
            SpanVoid<void> structSpan;
            SC_TRY_IF(sinkObject.viewAtBytes(0, structSinkProperty.sizeInBytes, structSpan));
            SC_TRY_IF(source.serialize(structSpan));
        }
        else
        {
            for (int16_t idx = 0; idx < structSinkProperty.numSubAtoms; ++idx)
            {
                sinkTypeIndex = structSinkTypeIndex + idx + 1;
                SC_TRY_IF(structSinkObject.viewAtBytes(sinkProperties.data()[sinkTypeIndex].offsetInBytes,
                                                       sinkProperties.data()[sinkTypeIndex].sizeInBytes, sinkObject));
                if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
                    sinkTypeIndex = sinkProperties.data()[sinkTypeIndex].getLinkIndex();
                SC_TRY_IF(read());
            }
        }
        return true;
    }

    [[nodiscard]] bool readArrayVector()
    {
        const auto arraySinkProperty   = sinkProperty;
        const auto arraySinkTypeIndex  = sinkTypeIndex;
        sinkTypeIndex                  = arraySinkTypeIndex + 1;
        SpanVoid<void> arraySinkObject = sinkObject;
        const auto     sinkItemSize    = sinkProperties.data()[sinkTypeIndex].sizeInBytes;
        if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
            sinkTypeIndex = sinkProperties.data()[sinkTypeIndex].getLinkIndex();
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
            SC_TRY_IF(
                arrayAccess.getSegmentSpan(arraySinkTypeIndex, arraySinkProperty, arraySinkObject, arraySinkStart));
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
};

struct VersionSchema
{
    Span<const Reflection::MetaProperties> sourceProperties;
};

template <typename BinaryWriter>
struct SimpleBinaryReaderVersioned
{
    struct Options
    {
        bool allowFloatToIntTruncation    = true;
        bool allowDropEccessArrayItems    = true;
        bool allowDropEccessStructMembers = true;
    };
    Options options;

    Span<const SC::ConstexprStringView> sinkNames;

    ArrayAccess arrayAccess;

    Span<const Reflection::MetaProperties> sinkProperties;
    SpanVoid<void>                         sinkObject;
    Reflection::MetaProperties             sinkProperty;
    int                                    sinkTypeIndex = 0;

    Span<const Reflection::MetaProperties> sourceProperties;
    BinaryWriter*                          sourceObject = nullptr;
    Reflection::MetaProperties             sourceProperty;
    int                                    sourceTypeIndex = 0;

    template <typename T>
    [[nodiscard]] bool serializeVersioned(T& object, BinaryWriter& source, VersionSchema& schema)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaTypeErased::compile<T>();
        sourceProperties          = schema.sourceProperties;
        sinkProperties            = flatSchema.propertiesAsSpan();
        sinkNames                 = flatSchema.namesAsSpan();
        sinkObject                = SpanVoid<void>(&object, sizeof(T));
        sourceObject              = &source;
        sinkTypeIndex             = 0;
        sourceTypeIndex           = 0;
        arrayAccess.vectorVtable  = {flatSchema.payload.vector.values,
                                     static_cast<size_t>(flatSchema.payload.vector.size)};

        if (sourceProperties.sizeInBytes() == 0 ||
            sourceProperties.data()[0].type != Reflection::MetaType::TypeStruct || sinkProperties.sizeInBytes() == 0 ||
            sinkProperties.data()[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return read();
    }

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
    [[nodiscard]] bool tryWritingPrimitiveValueToSink(SourceType sourceValue)
    {
        SinkType sourceConverted = static_cast<SinkType>(sourceValue);
        SC_TRY_IF(copySourceSink(SpanVoid<const void>(&sourceConverted, sizeof(sourceConverted)), sinkObject));
        return true;
    }

    template <typename T>
    [[nodiscard]] bool tryReadPrimitiveValue()
    {
        T sourceValue;
        SC_TRY_IF(sourceObject->serialize(SpanVoid<void>{&sourceValue, sizeof(T)}));
        SpanVoid<const void> span(&sourceValue, sizeof(T));
        switch (sinkProperty.type)
        {
        case Reflection::MetaType::TypeUINT8: return tryWritingPrimitiveValueToSink<T, uint8_t>(sourceValue);
        case Reflection::MetaType::TypeUINT16: return tryWritingPrimitiveValueToSink<T, uint16_t>(sourceValue);
        case Reflection::MetaType::TypeUINT32: return tryWritingPrimitiveValueToSink<T, uint32_t>(sourceValue);
        case Reflection::MetaType::TypeUINT64: return tryWritingPrimitiveValueToSink<T, uint64_t>(sourceValue);
        case Reflection::MetaType::TypeINT8: return tryWritingPrimitiveValueToSink<T, int8_t>(sourceValue);
        case Reflection::MetaType::TypeINT16: return tryWritingPrimitiveValueToSink<T, int16_t>(sourceValue);
        case Reflection::MetaType::TypeINT32: return tryWritingPrimitiveValueToSink<T, int32_t>(sourceValue);
        case Reflection::MetaType::TypeINT64: return tryWritingPrimitiveValueToSink<T, int64_t>(sourceValue);
        case Reflection::MetaType::TypeFLOAT32: return tryWritingPrimitiveValueToSink<T, float>(sourceValue);
        case Reflection::MetaType::TypeDOUBLE64: return tryWritingPrimitiveValueToSink<T, double>(sourceValue);
        default: return false;
        }
    }

    [[nodiscard]] bool tryPrimitiveConversion()
    {
        switch (sourceProperty.type)
        {
        case Reflection::MetaType::TypeUINT8: return tryReadPrimitiveValue<uint8_t>();
        case Reflection::MetaType::TypeUINT16: return tryReadPrimitiveValue<uint16_t>();
        case Reflection::MetaType::TypeUINT32: return tryReadPrimitiveValue<uint32_t>();
        case Reflection::MetaType::TypeUINT64: return tryReadPrimitiveValue<uint64_t>();
        case Reflection::MetaType::TypeINT8: return tryReadPrimitiveValue<int8_t>();
        case Reflection::MetaType::TypeINT16: return tryReadPrimitiveValue<int16_t>();
        case Reflection::MetaType::TypeINT32: return tryReadPrimitiveValue<int32_t>();
        case Reflection::MetaType::TypeINT64: return tryReadPrimitiveValue<int64_t>();
        case Reflection::MetaType::TypeFLOAT32: //
        {
            if (sinkProperty.type == Reflection::MetaType::TypeDOUBLE64 or options.allowFloatToIntTruncation)
            {
                return tryReadPrimitiveValue<float>();
            }
            break;
        }
        case Reflection::MetaType::TypeDOUBLE64: //
        {
            if (sinkProperty.type == Reflection::MetaType::TypeFLOAT32 or options.allowFloatToIntTruncation)
            {
                return tryReadPrimitiveValue<double>();
            }
            break;
        }
        default: break;
        }
        return false;
    }

    [[nodiscard]] bool read()
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
                return tryPrimitiveConversion();
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

    [[nodiscard]] bool readStruct()
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

        for (int16_t idx = 0; idx < structSourceProperty.numSubAtoms; ++idx)
        {
            sourceTypeIndex        = structSourceTypeIndex + idx + 1;
            const auto sourceOrder = sourceProperties.data()[sourceTypeIndex].order;
            int16_t    findIdx     = structSinkProperty.numSubAtoms;
            for (findIdx = 0; findIdx < structSinkProperty.numSubAtoms; ++findIdx)
            {
                const auto typeIndex = structSinkTypeIndex + findIdx + 1;
                if (sinkProperties.data()[typeIndex].order == sourceOrder)
                {
                    break;
                }
            }
            if (sourceProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = sourceProperties.data()[sourceTypeIndex].getLinkIndex();
            if (findIdx != structSinkProperty.numSubAtoms)
            {
                // Member with same order ordinal has been found
                sinkTypeIndex = structSinkTypeIndex + findIdx + 1;
                SC_TRY_IF(structSinkObject.viewAtBytes(sinkProperties.data()[sinkTypeIndex].offsetInBytes,
                                                       sinkProperties.data()[sinkTypeIndex].sizeInBytes, sinkObject));
                if (sinkProperties.data()[sinkTypeIndex].getLinkIndex() >= 0)
                    sinkTypeIndex = sinkProperties.data()[sinkTypeIndex].getLinkIndex();
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

    [[nodiscard]] bool readArrayVector()
    {
        if (sinkProperty.type != Reflection::MetaType::TypeArray &&
            sinkProperty.type != Reflection::MetaType::TypeVector)
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

        const bool isSameType =
            sinkProperties.data()[sinkTypeIndex].type == sourceProperties.data()[sourceTypeIndex].type;
        const bool isMemcpyable   = isPrimitive && isSameType;
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
                                         isMemcpyable ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                         options.allowDropEccessArrayItems ? ArrayAccess::DropEccessItems::Yes
                                                                           : ArrayAccess::DropEccessItems::No));
            SC_TRY_IF(
                arrayAccess.getSegmentSpan(arraySinkTypeIndex, arraySinkProperty, arraySinkObject, arraySinkStart));
        }
        if (isMemcpyable)
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
                sinkTypeIndex = sinkProperties.data()[sinkTypeIndex].getLinkIndex();
            if (sinkProperties.data()[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = sourceProperties.data()[sourceTypeIndex].getLinkIndex();
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

    [[nodiscard]] bool skipCurrent()
    {
        Serialization::BinarySkipper<BinaryWriter> skipper(*sourceObject, sourceTypeIndex);
        skipper.sourceProperties = sourceProperties;
        return skipper.skip();
    }
};
} // namespace SerializationTypeErased
} // namespace SC
