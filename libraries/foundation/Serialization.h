#pragma once
#include "FlatSchemaCompiler.h"
#include "ReflectionSC.h"

namespace SC
{
namespace Serialization
{
struct BinaryBuffer
{
    size_t              index = 0;
    SC::Vector<uint8_t> buffer;
    int                 numberOfOperations = 0;

    [[nodiscard]] bool readFrom(Span<const void> object)
    {
        Span<const uint8_t> bytes = object.castTo<const uint8_t>();
        numberOfOperations++;
        return buffer.appendCopy(bytes.data, bytes.size);
    }

    [[nodiscard]] bool writeTo(Span<void> object)
    {
        if (index + object.size > buffer.size())
            return false;
        numberOfOperations++;
        Span<uint8_t> bytes = object.castTo<uint8_t>();
        memcpy(bytes.data, &buffer[index], bytes.size);
        index += bytes.size;
        return true;
    }

    [[nodiscard]] bool advance(size_t numBytes)
    {
        if (index + numBytes > buffer.size())
            return false;
        index += numBytes;
        return true;
    }

    template <typename T>
    [[nodiscard]] constexpr bool readAndAdvance(T& value)
    {
        return writeTo(Span<void>{&value, sizeof(T)});
    }

    [[nodiscard]] constexpr bool writeAndAdvance(Span<void> other, size_t length)
    {
        if (other.size >= length)
        {
            return writeTo(Span<void>{other.data, length});
        }
        return false;
    }
};
struct SCArrayAccess
{
    typedef decltype(SegmentHeader::sizeBytes)          SizeType;
    [[nodiscard]] static constexpr Reflection::MetaType getType() { return Reflection::MetaType::TypeSCArray; }
    template <typename T>
    [[nodiscard]] static constexpr bool getSegmentSpan(Reflection::MetaProperties property, Span<T> object,
                                                       Span<T>& itemBegin)
    {
        SizeType size       = 0;
        auto     sizeReader = object;
        SC_TRY_IF(sizeReader.advance(SC_OFFSET_OF(SegmentHeader, sizeBytes)));
        SC_TRY_IF(sizeReader.readAndAdvance(size));
        return object.viewAt(sizeof(SegmentHeader), size, itemBegin);
    }

    [[nodiscard]] static constexpr bool resize(Span<void> object, Reflection::MetaProperties property,
                                               uint64_t sizeInBytes, bool initialize, bool dropEccessItems)
    {
        const SizeType availableSpace = static_cast<SizeType>(property.size - sizeof(SegmentHeader));
        const SizeType wantedSize     = dropEccessItems ? min(static_cast<SizeType>(sizeInBytes), availableSpace)
                                                        : static_cast<SizeType>(sizeInBytes);
        if ((object.size >= availableSpace) and (availableSpace >= wantedSize))
        {
            if (initialize)
            {
                auto& arrayByte = *static_cast<SC::Array<uint8_t, 1>*>(object.data);
                SC_TRY_IF(arrayByte.resize(wantedSize));
            }
            else
            {
                Span<void> sizeSpan;
                SC_TRY_IF(object.viewAt(SC_OFFSET_OF(SegmentHeader, sizeBytes), sizeof(SizeType), sizeSpan));
                SC_TRY_IF(Span<const void>(&wantedSize, sizeof(SizeType)).copyTo(sizeSpan));
            }
            return true;
        }
        else
        {
            return false;
        }
    }
};

struct SCVectorAccess
{
    [[nodiscard]] static constexpr Reflection::MetaType getType() { return Reflection::MetaType::TypeSCVector; }

    template <typename T>
    [[nodiscard]] static constexpr bool getSegmentSpan(Reflection::MetaProperties property, Span<T> object,
                                                       Span<T>& itemBegin)
    {
        if (object.size >= sizeof(void*))
        {
            typedef typename SameConstnessAs<T, SC::Vector<uint8_t>>::type VectorType;
            auto& vectorByte = *static_cast<VectorType*>(object.data);
            itemBegin        = Span<T>(vectorByte.data(), vectorByte.size());
            return true;
        }
        else
        {
            return false;
        }
    }

    [[nodiscard]] static constexpr bool resize(Span<void> object, Reflection::MetaProperties property,
                                               uint64_t sizeInBytes, bool initialize)
    {
        if (object.size >= sizeof(void*))
        {
            auto& vectorByte = *static_cast<SC::Vector<uint8_t>*>(object.data);
            if (initialize)
            {
                return vectorByte.resize(sizeInBytes);
            }
            else
            {
                return vectorByte.resizeWithoutInitializing(sizeInBytes);
            }
        }
        else
        {
            return false;
        }
    }
};

struct ArrayAccess
{
    template <typename T>
    [[nodiscard]] static constexpr bool getSegmentSpan(Reflection::MetaProperties property, Span<T> object,
                                                       Span<T>& itemBegin)
    {
        if (property.type == SCArrayAccess::getType())
            return SCArrayAccess::getSegmentSpan(property, object, itemBegin);
        else if (property.type == SCVectorAccess::getType())
            return SCVectorAccess::getSegmentSpan(property, object, itemBegin);
        return false;
    }

    enum class Initialize
    {
        No,
        Yes
    };
    enum class DropEccessItems
    {
        No,
        Yes
    };
    [[nodiscard]] static bool resize(Span<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
                                     Initialize initialize, DropEccessItems dropEccessItems)
    {
        if (property.type == SCArrayAccess::getType())
            return SCArrayAccess::resize(object, property, sizeInBytes, initialize == Initialize::Yes,
                                         dropEccessItems == DropEccessItems::Yes);
        else if (property.type == SCVectorAccess::getType())
        {
            return SCVectorAccess::resize(object, property, sizeInBytes, initialize == Initialize::Yes);
        }
        return false;
    }
};

[[nodiscard]] inline constexpr bool IsPrimitiveOrRecursivelyPacked(Reflection::MetaProperties properties)
{
    if (properties.isPrimitiveType())
        return true;
    if (properties.type == Reflection::MetaType::TypeStruct)
    {
        if (properties.getCustomUint32() &
            static_cast<uint32_t>(Reflection::FlatSchemaCompiler::MetaStructFlags::IsRecursivelyPacked))
        {
            return true;
        }
    }
    return false;
}

struct SimpleBinaryWriter
{
    Span<const Reflection::MetaProperties> sourceProperties;
    Span<const SC::ConstexprStringView>    sourceNames;
    BinaryBuffer&                          destination;
    Span<const void>                       sourceObject;
    int                                    sourceTypeIndex;
    Reflection::MetaProperties             sourceProperty;

    SimpleBinaryWriter(BinaryBuffer& destination) : destination(destination) {}

    template <typename T>
    [[nodiscard]] constexpr bool serialize(const T& object)
    {
        constexpr auto flatSchema      = Reflection::FlatSchemaCompiler::compile<T>();
        sourceProperties               = flatSchema.propertiesAsSpan();
        sourceNames                    = flatSchema.namesAsSpan();
        sourceObject                   = Span<const void>(&object, sizeof(T));
        sourceTypeIndex                = 0;
        destination.numberOfOperations = 0;
        if (sourceProperties.size == 0 || sourceProperties.data[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return write();
    }

    [[nodiscard]] constexpr bool write()
    {
        sourceProperty = sourceProperties.data[sourceTypeIndex];
        switch (sourceProperty.type)
        {
        case Reflection::MetaType::TypeInvalid: return false;
        case Reflection::MetaType::TypeUINT8:
        case Reflection::MetaType::TypeUINT16:
        case Reflection::MetaType::TypeUINT32:
        case Reflection::MetaType::TypeUINT64:
        case Reflection::MetaType::TypeINT8:
        case Reflection::MetaType::TypeINT16:
        case Reflection::MetaType::TypeINT32:
        case Reflection::MetaType::TypeINT64:
        case Reflection::MetaType::TypeFLOAT32:
        case Reflection::MetaType::TypeDOUBLE64: //
        {
            Span<const void> primitiveSpan;
            SC_TRY_IF(sourceObject.viewAt(0, sourceProperty.size, primitiveSpan));
            SC_TRY_IF(destination.readFrom(primitiveSpan));

            return true;
        }
        case Reflection::MetaType::TypeStruct: //
        {
            return writeStruct();
        }
        case Reflection::MetaType::TypeArray:
        case Reflection::MetaType::TypeSCArray:
        case Reflection::MetaType::TypeSCVector: //
        {
            return writeArray();
        }
        }
        return true;
    }

    [[nodiscard]] constexpr bool writeStruct()
    {
        const auto       structSourceProperty  = sourceProperty;
        const auto       structSourceTypeIndex = sourceTypeIndex;
        Span<const void> structSourceRoot      = sourceObject;

        const bool isBulkWriteable =
            structSourceProperty.getCustomUint32() &
            static_cast<uint32_t>(Reflection::FlatSchemaCompiler::MetaStructFlags::IsRecursivelyPacked);
        if (isBulkWriteable)
        {
            // Bulk Write the entire struct
            Span<const void> structSpan;
            SC_TRY_IF(sourceObject.viewAt(0, structSourceProperty.size, structSpan));
            SC_TRY_IF(destination.readFrom(structSpan));
        }
        else
        {
            for (int16_t idx = 0; idx < structSourceProperty.numSubAtoms; ++idx)
            {
                sourceTypeIndex = structSourceTypeIndex + idx + 1;
                SC_TRY_IF(structSourceRoot.viewAt(sourceProperties.data[sourceTypeIndex].offset,
                                                  sourceProperties.data[sourceTypeIndex].size, sourceObject));
                if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
                SC_TRY_IF(write());
            }
        }
        return true;
    }

    [[nodiscard]] constexpr bool writeArray()
    {
        const auto       arrayProperty  = sourceProperty;
        const auto       arrayTypeIndex = sourceTypeIndex;
        Span<const void> arraySpan;
        uint64_t         numBytes = 0;
        if (arrayProperty.type == Reflection::MetaType::TypeArray)
        {
            SC_TRY_IF(sourceObject.viewAt(0, arrayProperty.size, arraySpan));
            numBytes = arrayProperty.size;
        }
        else
        {
            SC_TRY_IF(ArrayAccess::getSegmentSpan(arrayProperty, sourceObject, arraySpan));
            numBytes = arraySpan.size;
            SC_TRY_IF(destination.readFrom(Span<const void>(&numBytes, sizeof(numBytes))));
        }
        sourceTypeIndex     = arrayTypeIndex + 1;
        const auto itemSize = sourceProperties.data[sourceTypeIndex].size;
        if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
            sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();

        const bool isBulkWriteable = IsPrimitiveOrRecursivelyPacked(sourceProperties.data[sourceTypeIndex]);
        if (isBulkWriteable)
        {
            SC_TRY_IF(destination.readFrom(arraySpan));
        }
        else
        {
            const auto numElements   = numBytes / itemSize;
            const auto itemTypeIndex = sourceTypeIndex;
            for (uint64_t idx = 0; idx < numElements; ++idx)
            {
                sourceTypeIndex = itemTypeIndex;
                SC_TRY_IF(arraySpan.viewAt(idx * itemSize, itemSize, sourceObject));
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
    Span<void>                             sinkObject;
    BinaryBuffer&                          source;

    SimpleBinaryReader(BinaryBuffer& source) : source(source) {}

    template <typename T>
    [[nodiscard]] bool serialize(T& object)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaCompiler::compile<T>();

        sinkProperties = flatSchema.propertiesAsSpan();
        sinkNames      = flatSchema.namesAsSpan();
        sinkObject     = Span<void>(&object, sizeof(T));
        sinkTypeIndex  = 0;

        if (sinkProperties.size == 0 || sinkProperties.data[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return read();
    }

    [[nodiscard]] bool read()
    {
        sinkProperty = sinkProperties.data[sinkTypeIndex];
        switch (sinkProperty.type)
        {
        case Reflection::MetaType::TypeInvalid: //
        {
            return false;
        }
        case Reflection::MetaType::TypeUINT8:
        case Reflection::MetaType::TypeUINT16:
        case Reflection::MetaType::TypeUINT32:
        case Reflection::MetaType::TypeUINT64:
        case Reflection::MetaType::TypeINT8:
        case Reflection::MetaType::TypeINT16:
        case Reflection::MetaType::TypeINT32:
        case Reflection::MetaType::TypeINT64:
        case Reflection::MetaType::TypeFLOAT32:
        case Reflection::MetaType::TypeDOUBLE64: //
        {
            Span<void> primitiveSpan;
            SC_TRY_IF(sinkObject.viewAt(0, sinkProperty.size, primitiveSpan));
            SC_TRY_IF(source.writeTo(primitiveSpan));

            return true;
        }
        case Reflection::MetaType::TypeStruct: //
        {
            return readStruct();
        }
        case Reflection::MetaType::TypeArray:
        case Reflection::MetaType::TypeSCArray:
        case Reflection::MetaType::TypeSCVector: //
        {
            return readArray();
        }
        }
        return true;
    }

    [[nodiscard]] bool readStruct()
    {
        const auto structSinkProperty  = sinkProperty;
        const auto structSinkTypeIndex = sinkTypeIndex;
        Span<void> structSinkObject    = sinkObject;
        const bool isRecursivelyPacked =
            structSinkProperty.getCustomUint32() &
            static_cast<uint32_t>(Reflection::FlatSchemaCompiler::MetaStructFlags::IsRecursivelyPacked);

        if (isRecursivelyPacked)
        {
            // Bulk read the entire struct
            Span<void> structSpan;
            SC_TRY_IF(sinkObject.viewAt(0, structSinkProperty.size, structSpan));
            SC_TRY_IF(source.writeTo(structSpan));
        }
        else
        {
            for (int16_t idx = 0; idx < structSinkProperty.numSubAtoms; ++idx)
            {
                sinkTypeIndex = structSinkTypeIndex + idx + 1;
                SC_TRY_IF(structSinkObject.viewAt(sinkProperties.data[sinkTypeIndex].offset,
                                                  sinkProperties.data[sinkTypeIndex].size, sinkObject));
                if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
                    sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
                SC_TRY_IF(read());
            }
        }
        return true;
    }

    [[nodiscard]] bool readArray()
    {
        const auto arraySinkProperty  = sinkProperty;
        const auto arraySinkTypeIndex = sinkTypeIndex;
        sinkTypeIndex                 = arraySinkTypeIndex + 1;
        Span<void> arraySinkObject    = sinkObject;
        const auto sinkItemSize       = sinkProperties.data[sinkTypeIndex].size;
        if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
            sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
        const bool isBulkReadable = IsPrimitiveOrRecursivelyPacked(sinkProperties.data[sinkTypeIndex]);
        Span<void> arraySinkStart;
        if (arraySinkProperty.type == Reflection::MetaType::TypeArray)
        {
            SC_TRY_IF(arraySinkObject.viewAt(0, arraySinkProperty.size, arraySinkStart));
        }
        else
        {
            uint64_t sinkNumBytes = 0;
            SC_TRY_IF(source.writeTo(Span<void>(&sinkNumBytes, sizeof(sinkNumBytes))));

            SC_TRY_IF(ArrayAccess::resize(arraySinkObject, arraySinkProperty, sinkNumBytes,
                                          isBulkReadable ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                          ArrayAccess::DropEccessItems::No));
            SC_TRY_IF(ArrayAccess::getSegmentSpan(arraySinkProperty, arraySinkObject, arraySinkStart));
        }
        if (isBulkReadable)
        {
            SC_TRY_IF(source.writeTo(arraySinkStart));
        }
        else
        {
            const auto sinkNumElements   = arraySinkStart.size / sinkItemSize;
            const auto itemSinkTypeIndex = sinkTypeIndex;
            for (uint64_t idx = 0; idx < sinkNumElements; ++idx)
            {
                sinkTypeIndex = itemSinkTypeIndex;
                SC_TRY_IF(arraySinkStart.viewAt(idx * sinkItemSize, sinkItemSize, sinkObject));
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

    Span<const Reflection::MetaProperties> sinkProperties;
    Span<void>                             sinkObject;
    Reflection::MetaProperties             sinkProperty;
    int                                    sinkTypeIndex = 0;

    Span<const Reflection::MetaProperties> sourceProperties;
    BinaryWriter*                          sourceObject = nullptr;
    Reflection::MetaProperties             sourceProperty;
    int                                    sourceTypeIndex = 0;

    template <typename T>
    [[nodiscard]] bool serializeVersioned(T& object, BinaryWriter& source, VersionSchema& schema)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaCompiler::compile<T>();
        sourceProperties          = schema.sourceProperties;
        sinkProperties            = flatSchema.propertiesAsSpan();
        sinkNames                 = flatSchema.namesAsSpan();
        sinkObject                = Span<void>(&object, sizeof(T));
        sourceObject              = &source;
        sinkTypeIndex             = 0;
        sourceTypeIndex           = 0;
        if (sourceProperties.size == 0 || sourceProperties.data[0].type != Reflection::MetaType::TypeStruct ||
            sinkProperties.size == 0 || sinkProperties.data[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return read();
    }

    template <typename SourceValue, typename SinkValue>
    [[nodiscard]] bool tryWritingPrimitiveValueToSink(SourceValue sourceValue)
    {
        SinkValue sinkValue = static_cast<SinkValue>(sourceValue);
        SC_TRY_IF(Span<const void>(&sinkValue, sizeof(sinkValue)).copyTo(sinkObject));

        return true;
    }

    template <typename T>
    [[nodiscard]] bool tryWritingPrimitiveValue(T sourceValue)
    {
        Span<const void> span(&sourceValue, sizeof(T));
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
        default: break;
        }
        return false;
    }

    template <typename T>
    [[nodiscard]] bool tryReadPrimitiveValue()
    {
        T value;
        SC_TRY_IF(sourceObject->readAndAdvance(value));

        return tryWritingPrimitiveValue(value);
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
        sinkProperty   = sinkProperties.data[sinkTypeIndex];
        sourceProperty = sourceProperties.data[sourceTypeIndex];
        switch (sourceProperty.type)
        {
        case Reflection::MetaType::TypeInvalid: //
        {
            return false;
        }
        case Reflection::MetaType::TypeUINT8:
        case Reflection::MetaType::TypeUINT16:
        case Reflection::MetaType::TypeUINT32:
        case Reflection::MetaType::TypeUINT64:
        case Reflection::MetaType::TypeINT8:
        case Reflection::MetaType::TypeINT16:
        case Reflection::MetaType::TypeINT32:
        case Reflection::MetaType::TypeINT64:
        case Reflection::MetaType::TypeFLOAT32:
        case Reflection::MetaType::TypeDOUBLE64: //
        {
            if (sinkObject.isNull())
            {
                SC_TRY_IF(sourceObject->advance(sourceProperty.size))
            }
            else if (sinkProperty.type == sourceProperty.type)
            {
                SC_TRY_IF(sourceObject->writeAndAdvance(sinkObject, sourceProperty.size));
            }
            else
            {
                return tryPrimitiveConversion();
            }
            break;
        }
        case Reflection::MetaType::TypeStruct: //
        {
            return readStruct();
        }
        case Reflection::MetaType::TypeArray:
        case Reflection::MetaType::TypeSCArray:
        case Reflection::MetaType::TypeSCVector: //
        {
            return readArray();
        }
        }
        return true;
    }

    [[nodiscard]] bool readStruct()
    {
        if ((not sinkObject.isNull()) && sourceProperty.type != sinkProperty.type)
        {
            return false;
        }

        const auto structSourceProperty  = sourceProperty;
        const auto structSourceTypeIndex = sourceTypeIndex;
        const auto structSinkProperty    = sinkProperty;
        const auto structSinkTypeIndex   = sinkTypeIndex;
        Span<void> structSinkObject      = sinkObject;

        for (int16_t idx = 0; idx < structSourceProperty.numSubAtoms; ++idx)
        {
            sourceTypeIndex        = structSourceTypeIndex + idx + 1;
            const auto sourceOrder = sourceProperties.data[sourceTypeIndex].order;
            int16_t    findIdx     = structSinkProperty.numSubAtoms;
            if (not structSinkObject.isNull())
            {
                for (findIdx = 0; findIdx < structSinkProperty.numSubAtoms; ++findIdx)
                {
                    const auto typeIndex = structSinkTypeIndex + findIdx + 1;
                    if (sinkProperties.data[typeIndex].order == sourceOrder)
                    {
                        break;
                    }
                }
            }
            if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
            if (findIdx != structSinkProperty.numSubAtoms && (not structSinkObject.isNull()))
            {
                // Member with same order ordinal has been found
                sinkTypeIndex = structSinkTypeIndex + findIdx + 1;
                SC_TRY_IF(structSinkObject.viewAt(sinkProperties.data[sinkTypeIndex].offset,
                                                  sinkProperties.data[sinkTypeIndex].size, sinkObject));
                if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
                    sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
                SC_TRY_IF(read());
            }
            else if (options.allowDropEccessStructMembers)
            {
                sinkObject = Span<void>();
                SC_TRY_IF(read());
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    static constexpr bool IsArrayType(Reflection::MetaType type)
    {
        switch (type)
        {
        case Reflection::MetaType::TypeArray:
        case Reflection::MetaType::TypeSCArray:
        case Reflection::MetaType::TypeSCVector: //
        {
            return true;
        }
        default: break;
        }
        return false;
    }

    [[nodiscard]] bool readArray()
    {
        if ((not sinkObject.isNull()) and (not IsArrayType(sinkProperty.type)))
        {
            return false;
        }
        const auto arraySourceProperty  = sourceProperty;
        const auto arraySourceTypeIndex = sourceTypeIndex;
        const auto arraySinkTypeIndex   = sinkTypeIndex;
        Span<void> arraySinkObject      = sinkObject;
        const auto arraySinkProperty    = sinkProperty;

        sourceTypeIndex         = arraySourceTypeIndex + 1;
        uint64_t sourceNumBytes = arraySourceProperty.size;
        if (arraySourceProperty.type != Reflection::MetaType::TypeArray)
        {
            SC_TRY_IF(sourceObject->readAndAdvance(sourceNumBytes));
        }

        const bool isPrimitive = sourceProperties.data[sourceTypeIndex].isPrimitiveType();

        if (sinkObject.isNull())
        {
            return skipElements(isPrimitive, sourceNumBytes);
        }
        sinkTypeIndex = arraySinkTypeIndex + 1;

        const bool isSameType = sinkProperties.data[sinkTypeIndex].type == sourceProperties.data[sourceTypeIndex].type;
        const bool isMemcpyable   = isPrimitive && isSameType;
        const auto sourceItemSize = sourceProperties.data[sourceTypeIndex].size;
        const auto sinkItemSize   = sinkProperties.data[sinkTypeIndex].size;

        Span<void> arraySinkStart;
        if (arraySinkProperty.type == Reflection::MetaType::TypeArray)
        {
            SC_TRY_IF(arraySinkObject.viewAt(0, arraySinkProperty.size, arraySinkStart));
        }
        else
        {
            const auto numWantedBytes = sourceNumBytes / sourceItemSize * sinkItemSize;
            SC_TRY_IF(ArrayAccess::resize(arraySinkObject, arraySinkProperty, numWantedBytes,
                                          isMemcpyable ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                          options.allowDropEccessArrayItems ? ArrayAccess::DropEccessItems::Yes
                                                                            : ArrayAccess::DropEccessItems::No));
            SC_TRY_IF(ArrayAccess::getSegmentSpan(arraySinkProperty, arraySinkObject, arraySinkStart));
        }
        if (isMemcpyable)
        {
            const auto minBytes = min(static_cast<uint64_t>(arraySinkStart.size), sourceNumBytes);
            SC_TRY_IF(sourceObject->writeAndAdvance(arraySinkStart, minBytes));
            if (sourceNumBytes > static_cast<uint64_t>(arraySinkStart.size))
            {
                if (not options.allowDropEccessArrayItems)
                {
                    return false;
                }
                return sourceObject->advance(sourceNumBytes - minBytes);
            }
        }
        else
        {
            if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
                sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
            if (sinkProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
            const uint64_t sinkNumElements     = arraySinkStart.size / sinkItemSize;
            const uint64_t sourceNumElements   = sourceNumBytes / sourceItemSize;
            const auto     itemSinkTypeIndex   = sinkTypeIndex;
            const auto     itemSourceTypeIndex = sourceTypeIndex;
            const auto     minElements         = min(sinkNumElements, sourceNumElements);
            for (uint64_t idx = 0; idx < minElements; ++idx)
            {
                sinkTypeIndex   = itemSinkTypeIndex;
                sourceTypeIndex = itemSourceTypeIndex;
                SC_TRY_IF(arraySinkStart.viewAt(idx * sinkItemSize, sinkItemSize, sinkObject));
                SC_TRY_IF(read());
            }
            if (sourceNumElements > sinkNumElements)
            {
                sinkObject = Span<void>();
                return skipElements(isPrimitive, (sourceNumElements - sinkNumElements) * sourceItemSize);
            }
        }
        return true;
    }

    bool skipElements(bool isPrimitive, SC::uint64_t sourceNumBytes)
    {
        if (isPrimitive)
        {
            SC_TRY_IF(sourceObject->advance(sourceNumBytes));
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
                SC_TRY_IF(read());
            }
        }
        return true;
    }
};
} // namespace Serialization
} // namespace SC
