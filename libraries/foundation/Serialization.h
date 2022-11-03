#pragma once
#include "ReflectionFlatSchemaCompiler.h"
#include "ReflectionSC.h"

// TODO: Support SmallVector
// TODO: Streaming interface
// TODO: Figure out if BulkWrites optimizations could make sense into the versioned serializer, if it's not too
// complicated
// TODO: Compiling all the constexpr stuff on MSVC is very slow

namespace SC
{
struct BufferDestination
{
    SC::Vector<uint8_t> buffer;

    [[nodiscard]] bool write(Span<const void> object)
    {
        numWrites++;
        Span<const uint8_t> bytes = object.castTo<const uint8_t>();
        return buffer.appendCopy(bytes.data, bytes.size);
    }

    size_t index = 0;

    [[nodiscard]] bool read(Span<void> object)
    {
        if (object.size > buffer.size())
            return false;
        numReads++;
        Span<uint8_t> bytes = object.castTo<uint8_t>();
        memcpy(bytes.data, &buffer[index], bytes.size);
        index += bytes.size;
        return true;
    }

    // Used only for the test
    int numReads  = 0;
    int numWrites = 0;
    template <typename T>
    [[nodiscard]] constexpr const T read()
    {
        T alignedRead;
        memcpy(&alignedRead, &buffer[index], sizeof(T));
        index += sizeof(T);
        return alignedRead;
    }
};

namespace Serialization
{
struct CArrayAccess
{
    static constexpr Reflection::MetaType getType() { return Reflection::MetaType::TypeArray; }

    template <typename T>
    static constexpr bool getSegmentSpan(Reflection::MetaProperties property, Span<T>(object), Span<T>& itemBegin)
    {
        return object.viewAt(0, property.size, itemBegin);
    }

    static bool resize(Span<void>(object), Reflection::MetaProperties property, uint64_t sizeInBytes)
    {
        return sizeInBytes == property.size;
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
                                               uint64_t sizeInBytes, bool dropEccessItems)
    {
        const SizeType availableSpace = static_cast<SizeType>(property.size - sizeof(SegmentHeader));
        const SizeType convertedSize  = dropEccessItems ? min(static_cast<SizeType>(sizeInBytes), availableSpace)
                                                        : static_cast<SizeType>(sizeInBytes);
        if (availableSpace >= convertedSize)
        {
            Span<void> sizeSpan;
            SC_TRY_IF(object.viewAt(SC_OFFSET_OF(SegmentHeader, sizeBytes), sizeof(SizeType), sizeSpan));
            SC_TRY_IF(Span<const void>(&convertedSize, sizeof(SizeType)).copyTo(sizeSpan));
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
        if (property.type == CArrayAccess::getType())
            return CArrayAccess::getSegmentSpan(property, object, itemBegin);
        else if (property.type == SCArrayAccess::getType())
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
        if (property.type == CArrayAccess::getType())
            return CArrayAccess::resize(object, property, sizeInBytes);
        else if (property.type == SCArrayAccess::getType())
            return SCArrayAccess::resize(object, property, sizeInBytes, dropEccessItems == DropEccessItems::Yes);
        else if (property.type == SCVectorAccess::getType())
        {
            return SCVectorAccess::resize(object, property, sizeInBytes, initialize == Initialize::Yes);
        }
        return false;
    }
};

struct SimpleBinaryWriter
{
    Span<const Reflection::MetaProperties> sourceProperties;
    Span<const Reflection::MetaStringView> sourceNames;
    BufferDestination                      destination;
    Span<const void>                       sourceObject;
    int                                    sourceTypeIndex;
    Reflection::MetaProperties             sourceProperty;

    template <typename T>
    [[nodiscard]] bool write(const T& object)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        sourceProperties          = flatSchema.propertiesAsSpan();
        sourceNames               = flatSchema.namesAsSpan();
        sourceObject              = Span<const void>(&object, sizeof(T));
        sourceTypeIndex           = 0;
        if (sourceProperties.size == 0 || sourceProperties.data[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return write();
    }

    [[nodiscard]] bool write()
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
            SC_TRY_IF(destination.write(primitiveSpan));
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

    [[nodiscard]] bool writeStruct()
    {
        const auto       structSourceProperty  = sourceProperty;
        const auto       structSourceTypeIndex = sourceTypeIndex;
        Span<const void> structSourceRoot      = sourceObject;

        const bool isBulkWriteable = structSourceProperty.getCustomUint32() &
                                     static_cast<uint32_t>(Reflection::MetaStructFlags::IsRecursivelyPacked);
        if (isBulkWriteable)
        {
            // Bulk Write the entire struct
            Span<const void> structSpan;
            SC_TRY_IF(sourceObject.viewAt(0, structSourceProperty.size, structSpan));
            SC_TRY_IF(destination.write(structSpan));
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

    [[nodiscard]] bool writeArray()
    {
        const auto       arrayProperty  = sourceProperty;
        const auto       arrayTypeIndex = sourceTypeIndex;
        Span<const void> arraySpan;
        uint64_t         numBytes = 0;
        SC_TRY_IF(ArrayAccess::getSegmentSpan(arrayProperty, sourceObject, arraySpan));
        numBytes = arraySpan.size;
        if (arrayProperty.type != Reflection::MetaType::TypeArray)
        {
            SC_TRY_IF(destination.write(Span<const void>(&numBytes, sizeof(numBytes))));
        }
        sourceTypeIndex     = arrayTypeIndex + 1;
        const auto itemSize = sourceProperties.data[sourceTypeIndex].size;
        if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
            sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();

        const bool isBulkWriteable = Reflection::IsPrimitiveOrRecursivelyPacked(sourceProperties.data[sourceTypeIndex]);
        if (isBulkWriteable)
        {
            SC_TRY_IF(destination.write(arraySpan));
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
    Span<const Reflection::MetaStringView> sinkNames;
    Reflection::MetaProperties             sinkProperty;
    int                                    sinkTypeIndex = 0;
    Span<void>                             sinkObject;
    BufferDestination                      source;

    template <typename T>
    [[nodiscard]] bool read(T& object)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        sinkProperties            = flatSchema.propertiesAsSpan();
        sinkNames                 = flatSchema.namesAsSpan();
        sinkObject                = Span<void>(&object, sizeof(T));
        sinkTypeIndex             = 0;
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
            SC_TRY_IF(source.read(primitiveSpan));
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
        const bool isRecursivelyPacked = structSinkProperty.getCustomUint32() &
                                         static_cast<uint32_t>(Reflection::MetaStructFlags::IsRecursivelyPacked);

        if (isRecursivelyPacked)
        {
            // Bulk read the entire struct
            Span<void> structSpan;
            SC_TRY_IF(sinkObject.viewAt(0, structSinkProperty.size, structSpan));
            SC_TRY_IF(source.read(structSpan));
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
        const bool isBulkReadable = Reflection::IsPrimitiveOrRecursivelyPacked(sinkProperties.data[sinkTypeIndex]);
        if (arraySinkProperty.type != Reflection::MetaType::TypeArray)
        {
            uint64_t sinkNumBytes = 0;
            SC_TRY_IF(source.read(Span<void>(&sinkNumBytes, sizeof(sinkNumBytes))));
            SC_TRY_IF(ArrayAccess::resize(arraySinkObject, arraySinkProperty, sinkNumBytes,
                                          isBulkReadable ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                          ArrayAccess::DropEccessItems::No));
        }
        Span<void> arraySinkStart;
        SC_TRY_IF(ArrayAccess::getSegmentSpan(arraySinkProperty, arraySinkObject, arraySinkStart));
        if (isBulkReadable)
        {
            SC_TRY_IF(source.read(arraySinkStart));
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

struct SimpleBinaryReaderVersioned
{
    struct Options
    {
        bool allowFloatToIntTruncation    = true;
        bool allowDropEccessArrayItems    = true;
        bool allowDropEccessStructMembers = true;
    };
    Options options;

    Span<const Reflection::MetaStringView> sinkNames;
    Span<const Reflection::MetaStringView> sourceNames;

    Span<const Reflection::MetaProperties> sinkProperties;
    Span<void>                             sinkObject;
    Reflection::MetaProperties             sinkProperty;
    int                                    sinkTypeIndex = 0;

    Span<const Reflection::MetaProperties> sourceProperties;
    Span<const void>                       sourceObject;
    Reflection::MetaProperties             sourceProperty;
    int                                    sourceTypeIndex = 0;

    template <typename T>
    [[nodiscard]] bool read(T& object, Span<const void> source, Span<const Reflection::MetaProperties> properties,
                            Span<const Reflection::MetaStringView> names)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        sourceProperties          = properties;
        sinkProperties            = flatSchema.propertiesAsSpan();
        sinkNames                 = flatSchema.namesAsSpan();
        sourceNames               = names;
        sinkObject                = Span<void>(&object, sizeof(T));
        sourceObject              = source;
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
        SC_TRY_IF(sourceObject.readAndAdvance(value));
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
                SC_TRY_IF(sourceObject.advance(sourceProperty.size))
            }
            else if (sinkProperty.type == sourceProperty.type)
            {
                SC_TRY_IF(sourceObject.writeAndAdvance(sinkObject, sourceProperty.size));
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
            if (findIdx != structSinkProperty.numSubAtoms && (not structSinkObject.isNull()))
            {
                // Member with same order ordinal has been found
                sinkTypeIndex = structSinkTypeIndex + findIdx + 1;
                SC_TRY_IF(structSinkObject.viewAt(sinkProperties.data[sinkTypeIndex].offset,
                                                  sinkProperties.data[sinkTypeIndex].size, sinkObject));
                if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
                if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
                    sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
                SC_TRY_IF(read());
            }
            else if (options.allowDropEccessStructMembers)
            {
                sinkObject = Span<void>();
                if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
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
            SC_TRY_IF(sourceObject.readAndAdvance(sourceNumBytes));
        }

        const bool isPrimitive = Reflection::IsPrimitiveType(sourceProperties.data[sourceTypeIndex].type);

        if (sinkObject.isNull())
        {
            if (isPrimitive)
            {
                SC_TRY_IF(sourceObject.advance(sourceNumBytes));
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
        sinkTypeIndex = arraySinkTypeIndex + 1;

        const bool isSameType = sinkProperties.data[sinkTypeIndex].type == sourceProperties.data[sourceTypeIndex].type;
        const bool isMemcpyable   = isPrimitive && isSameType;
        const auto sourceItemSize = sourceProperties.data[sourceTypeIndex].size;
        const auto sinkItemSize   = sinkProperties.data[sinkTypeIndex].size;
        if (arraySinkProperty.type != Reflection::MetaType::TypeArray)
        {
            const auto numWantedBytes = sourceNumBytes / sourceItemSize * sinkItemSize;
            SC_TRY_IF(ArrayAccess::resize(arraySinkObject, arraySinkProperty, numWantedBytes,
                                          isMemcpyable ? ArrayAccess::Initialize::No : ArrayAccess::Initialize::Yes,
                                          options.allowDropEccessArrayItems ? ArrayAccess::DropEccessItems::Yes
                                                                            : ArrayAccess::DropEccessItems::No));
        }
        Span<void> arraySinkStart;
        SC_TRY_IF(ArrayAccess::getSegmentSpan(arraySinkProperty, arraySinkObject, arraySinkStart));
        if (isMemcpyable)
        {
            const auto minBytes = min(static_cast<uint64_t>(arraySinkStart.size), sourceNumBytes);
            SC_TRY_IF(sourceObject.writeAndAdvance(arraySinkStart, minBytes));
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
        }
        return true;
    }
};
} // namespace Serialization
} // namespace SC
