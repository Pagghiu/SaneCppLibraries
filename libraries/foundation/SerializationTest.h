#pragma once
#include "Console.h"
#include "Reflection.h"
#include "ReflectionFlatSchemaCompiler.h"
#include "ReflectionSC.h"
#include "StringBuilder.h"
#include "Test.h"

// TODO: Optional flags to allow dropping array elements (with testing of edge cases)
// TODO: Optimize for memcpy-able types (example: Vector<Point3> should be memcopyed)
// TODO: Primitive conversions
// TODO: Support SmallVector
// TODO: Streaming

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
                                               uint64_t sizeInBytes)
    {
        const SizeType minValue =
            SC::min(static_cast<SizeType>(sizeInBytes), static_cast<SizeType>(property.size - sizeof(SegmentHeader)));
        Span<void> sizeSpan;
        SC_TRY_IF(object.viewAt(SC_OFFSET_OF(SegmentHeader, sizeBytes), sizeof(SizeType), sizeSpan));
        SC_TRY_IF(Span<const void>(&minValue, sizeof(SizeType)).copyTo(sizeSpan));
        return true;
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
            auto& vectorByte = *reinterpret_cast<VectorType*>(object.data);
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
            auto& vectorByte = *reinterpret_cast<SC::Vector<uint8_t>*>(object.data);
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
    [[nodiscard]] static bool resize(Span<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
                                     bool initialize)
    {
        if (property.type == CArrayAccess::getType())
            return CArrayAccess::resize(object, property, sizeInBytes);
        else if (property.type == SCArrayAccess::getType())
            return SCArrayAccess::resize(object, property, sizeInBytes);
        else if (property.type == SCVectorAccess::getType())
            return SCVectorAccess::resize(object, property, sizeInBytes, initialize);
        return false;
    }
};

struct SimpleBinaryWriter
{
    Span<const Reflection::MetaProperties> properties;
    Span<const Reflection::MetaStringView> names;
    BufferDestination                      destination;
    Span<const void>                       sinkObject;
    int                                    typeIndex;
    Reflection::MetaProperties             property;

    template <typename T>
    [[nodiscard]] bool write(const T& object)
    {
        auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        // ReflectionTest::printFlatSchema(flatSchema.properties.values, flatSchema.names.values);
        properties = flatSchema.propertiesAsSpan();
        names      = flatSchema.namesAsSpan();
        sinkObject = Span<const void>(&object, sizeof(T));
        typeIndex  = 0;
        if (properties.size == 0 || properties.data[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return write();
    }

    [[nodiscard]] bool write()
    {
        property = properties.data[typeIndex];
        switch (property.type)
        {
        // clang-format off
        case Reflection::MetaType::TypeInvalid:
            return false;
        case Reflection::MetaType::TypeUINT8:
        case Reflection::MetaType::TypeUINT16:
        case Reflection::MetaType::TypeUINT32:
        case Reflection::MetaType::TypeUINT64:
        case Reflection::MetaType::TypeINT8:
        case Reflection::MetaType::TypeINT16:
        case Reflection::MetaType::TypeINT32:
        case Reflection::MetaType::TypeINT64:
        case Reflection::MetaType::TypeFLOAT32:
        case Reflection::MetaType::TypeDOUBLE64:
            {
                Span<const void> primitiveSpan;
                SC_TRY_IF(sinkObject.viewAt(0, property.size, primitiveSpan));
                SC_TRY_IF(destination.write(primitiveSpan));
                return true;
            }
        case Reflection::MetaType::TypeStruct:
            return writeStruct();
        case Reflection::MetaType::TypeArray:
        case Reflection::MetaType::TypeSCArray:
        case Reflection::MetaType::TypeSCVector:
            return writeArray();
            // clang-format on
        }
        return true;
    }

    [[nodiscard]] bool writeStruct()
    {
        const auto       selfProperty  = property;
        const auto       selfTypeIndex = typeIndex;
        Span<const void> structRoot    = sinkObject;
        for (int16_t idx = 0; idx < selfProperty.numSubAtoms; ++idx)
        {
            typeIndex = selfTypeIndex + idx + 1;
            SC_TRY_IF(
                structRoot.viewAt(properties.data[typeIndex].offset, properties.data[typeIndex].size, sinkObject));
            if (properties.data[typeIndex].getLinkIndex() >= 0)
                typeIndex = properties.data[typeIndex].getLinkIndex();
            SC_TRY_IF(write());
        }
        return true;
    }

    [[nodiscard]] bool writeArray()
    {
        const auto       arrayProperty  = property;
        const auto       arrayTypeIndex = typeIndex;
        Span<const void> arraySpan;
        uint64_t         numBytes = 0;
        SC_TRY_IF(ArrayAccess::getSegmentSpan(arrayProperty, sinkObject, arraySpan));
        numBytes = arraySpan.size;
        if (arrayProperty.type != Reflection::MetaType::TypeArray)
        {
            SC_TRY_IF(destination.write(Span<const void>(&numBytes, sizeof(numBytes))));
        }
        typeIndex              = arrayTypeIndex + 1;
        const bool isPrimitive = Reflection::IsPrimitiveType(properties.data[typeIndex].type);
        if (isPrimitive)
        {
            SC_TRY_IF(destination.write(arraySpan));
        }
        else
        {
            const auto itemSize = properties.data[typeIndex].size;
            if (properties.data[typeIndex].getLinkIndex() >= 0)
                typeIndex = properties.data[typeIndex].getLinkIndex();
            const auto numElements   = numBytes / itemSize;
            const auto itemTypeIndex = typeIndex;
            for (uint64_t idx = 0; idx < numElements; ++idx)
            {
                typeIndex = itemTypeIndex;
                SC_TRY_IF(arraySpan.viewAt(idx * itemSize, itemSize, sinkObject));
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
        auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        // ReflectionTest::printFlatSchema(flatSchema.properties.values, flatSchema.names.values);
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
        // clang-format off
        case Reflection::MetaType::TypeInvalid:
            return false;
        case Reflection::MetaType::TypeUINT8:
        case Reflection::MetaType::TypeUINT16:
        case Reflection::MetaType::TypeUINT32:
        case Reflection::MetaType::TypeUINT64:
        case Reflection::MetaType::TypeINT8:
        case Reflection::MetaType::TypeINT16:
        case Reflection::MetaType::TypeINT32:
        case Reflection::MetaType::TypeINT64:
        case Reflection::MetaType::TypeFLOAT32:
        case Reflection::MetaType::TypeDOUBLE64:
            {
                Span<void> primitiveSpan;
                SC_TRY_IF(sinkObject.viewAt(0, sinkProperty.size, primitiveSpan));
                SC_TRY_IF(source.read(primitiveSpan));
                return true;
            }
        case Reflection::MetaType::TypeStruct:
            return readStruct();
        case Reflection::MetaType::TypeArray:
        case Reflection::MetaType::TypeSCArray:
        case Reflection::MetaType::TypeSCVector:
            return readArray();
            // clang-format on
        }
        return true;
    }

    [[nodiscard]] bool readStruct()
    {
        const auto structSinkProperty  = sinkProperty;
        const auto structSinkTypeIndex = sinkTypeIndex;
        Span<void> structSinkObject    = sinkObject;
        for (int16_t idx = 0; idx < structSinkProperty.numSubAtoms; ++idx)
        {
            sinkTypeIndex = structSinkTypeIndex + idx + 1;
            SC_TRY_IF(structSinkObject.viewAt(sinkProperties.data[sinkTypeIndex].offset,
                                              sinkProperties.data[sinkTypeIndex].size, sinkObject));
            if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
                sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
            SC_TRY_IF(read());
        }
        return true;
    }

    [[nodiscard]] bool readArray()
    {
        const auto arraySinkProperty  = sinkProperty;
        const auto arraySinkTypeIndex = sinkTypeIndex;
        sinkTypeIndex                 = arraySinkTypeIndex + 1;
        Span<void> arraySinkObject    = sinkObject;
        const bool isSinkPrimitive    = Reflection::IsPrimitiveType(sinkProperties.data[sinkTypeIndex].type);
        if (arraySinkProperty.type != Reflection::MetaType::TypeArray)
        {
            uint64_t sinkNumBytes = 0;
            SC_TRY_IF(source.read(Span<void>(&sinkNumBytes, sizeof(sinkNumBytes))));
            SC_TRY_IF(ArrayAccess::resize(arraySinkObject, arraySinkProperty, sinkNumBytes, not isSinkPrimitive));
        }
        Span<void> arraySinkStart;
        SC_TRY_IF(ArrayAccess::getSegmentSpan(arraySinkProperty, arraySinkObject, arraySinkStart));
        if (isSinkPrimitive)
        {
            SC_TRY_IF(source.read(arraySinkStart));
        }
        else
        {
            const auto sinkItemSize = sinkProperties.data[sinkTypeIndex].size;
            if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
                sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
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
        auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        // ReflectionTest::printFlatSchema(flatSchema.properties.values, flatSchema.names.values);
        sourceProperties = properties;
        sinkProperties   = flatSchema.propertiesAsSpan();
        sinkNames        = flatSchema.namesAsSpan();
        sourceNames      = names;
        sinkObject       = Span<void>(&object, sizeof(T));
        sourceObject     = source;
        sinkTypeIndex    = 0;
        sourceTypeIndex  = 0;
        if (sourceProperties.size == 0 || sourceProperties.data[0].type != Reflection::MetaType::TypeStruct ||
            sinkProperties.size == 0 || sinkProperties.data[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return read();
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
                return false; // Incompatible types, we could do some conversions
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
            else
            {
                sinkObject = Span<void>();
                if (sourceProperties.data[sourceTypeIndex].getLinkIndex() >= 0)
                    sourceTypeIndex = sourceProperties.data[sourceTypeIndex].getLinkIndex();
                SC_TRY_IF(read());
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
        case Reflection::MetaType::TypeSCVector: return true;
        default: return false;
        }
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
            SC_TRY_IF(ArrayAccess::resize(arraySinkObject, arraySinkProperty,
                                          sourceNumBytes / sourceItemSize * sinkItemSize, not isMemcpyable));
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

namespace SC
{
struct SerializationTest;
struct PrimitiveStruct;
struct NestedStruct;
struct TopLevelStruct;
struct VectorStructSimple;
struct VectorStructComplex;
struct VersionedStruct1;
struct VersionedStruct2;
} // namespace SC

struct SC::PrimitiveStruct
{
    uint8_t arrayValue[3] = {0, 1, 2};
    float   floatValue    = 1.5f;
    int64_t int64Value    = -13;

    bool operator!=(const PrimitiveStruct& other) const
    {
        for (int i = 0; i < ConstantArraySize(arrayValue); ++i)
            if (arrayValue[i] != other.arrayValue[i])
                return true;
        if (floatValue != other.floatValue)
            return true;
        if (int64Value != other.int64Value)
            return true;
        return false;
    }
};
SC_META_STRUCT_BEGIN(SC::PrimitiveStruct)
SC_META_STRUCT_MEMBER(0, arrayValue)
SC_META_STRUCT_MEMBER(1, floatValue)
SC_META_STRUCT_MEMBER(2, int64Value)
SC_META_STRUCT_END()
struct SC::NestedStruct
{
    int16_t         int16Value = 244;
    PrimitiveStruct structsArray[2];
    double          doubleVal = -1.24;
    Array<int, 7>   arrayInt  = {1, 2, 3, 4, 5, 6};

    bool operator!=(const NestedStruct& other) const
    {
        if (int16Value != other.int16Value)
            return true;
        for (int i = 0; i < ConstantArraySize(structsArray); ++i)
            if (structsArray[i] != other.structsArray[i])
                return true;
        if (doubleVal != other.doubleVal)
            return true;
        return false;
    }
};

SC_META_STRUCT_BEGIN(SC::NestedStruct)
SC_META_STRUCT_MEMBER(0, int16Value)
SC_META_STRUCT_MEMBER(1, structsArray)
SC_META_STRUCT_MEMBER(2, doubleVal)
SC_META_STRUCT_END()
struct SC::TopLevelStruct
{
    NestedStruct nestedStruct;

    bool operator!=(const TopLevelStruct& other) const { return nestedStruct != other.nestedStruct; }
};
SC_META_STRUCT_BEGIN(SC::TopLevelStruct)
SC_META_STRUCT_MEMBER(0, nestedStruct)
SC_META_STRUCT_END()

struct SC::VectorStructSimple
{
    SC::Vector<int> emptyVector;
    SC::Vector<int> vectorOfInts;
};

SC_META_STRUCT_BEGIN(SC::VectorStructSimple)
SC_META_STRUCT_MEMBER(0, emptyVector)
SC_META_STRUCT_MEMBER(1, vectorOfInts)
SC_META_STRUCT_END()

struct SC::VectorStructComplex
{
    SC::Vector<SC::String> vectorOfStrings;
};

SC_META_STRUCT_BEGIN(SC::VectorStructComplex)
SC_META_STRUCT_MEMBER(0, vectorOfStrings)
SC_META_STRUCT_END()

struct SC::VersionedStruct1
{
    float          floatValue     = 1.5f;
    int64_t        fieldToRemove  = 12;
    Vector<String> field2ToRemove = {"ASD1"_sv, "ASD2"_sv, "ASD3"_sv};
    int64_t        int64Value     = -13;
};

SC_META_STRUCT_BEGIN(SC::VersionedStruct1)
SC_META_STRUCT_MEMBER(0, floatValue)
SC_META_STRUCT_MEMBER(1, fieldToRemove)
SC_META_STRUCT_MEMBER(2, field2ToRemove)
SC_META_STRUCT_MEMBER(3, int64Value)
SC_META_STRUCT_END()

struct SC::VersionedStruct2
{
    int64_t int64Value = 55;
    float   floatValue = -2.9f;

    bool operator!=(const VersionedStruct1& other) const
    {
        if (floatValue != other.floatValue)
            return true;
        if (int64Value != other.int64Value)
            return true;
        return false;
    }
};
SC_META_STRUCT_BEGIN(SC::VersionedStruct2)
SC_META_STRUCT_MEMBER(3, int64Value)
SC_META_STRUCT_MEMBER(0, floatValue)
SC_META_STRUCT_END()

namespace SC
{
struct VersionedPoint3D;
struct VersionedPoint2D;
struct VersionedArray1;
struct VersionedArray2;
} // namespace SC
struct SC::VersionedPoint3D
{
    float x;
    float y;
    float z;
};
SC_META_STRUCT_BEGIN(SC::VersionedPoint3D)
SC_META_STRUCT_MEMBER(0, x)
SC_META_STRUCT_MEMBER(1, y)
SC_META_STRUCT_MEMBER(2, z)
SC_META_STRUCT_END()
struct SC::VersionedPoint2D
{
    float x;
    float y;
};
SC_META_STRUCT_BEGIN(SC::VersionedPoint2D)
SC_META_STRUCT_MEMBER(0, x)
SC_META_STRUCT_MEMBER(1, y)
SC_META_STRUCT_END()

struct SC::VersionedArray1
{
    Vector<VersionedPoint2D> points;
    Vector<int>              simpleInts = {1, 2, 3};
};
SC_META_STRUCT_BEGIN(SC::VersionedArray1)
SC_META_STRUCT_MEMBER(0, points)
SC_META_STRUCT_MEMBER(1, simpleInts)
SC_META_STRUCT_END()

struct SC::VersionedArray2
{
    Array<VersionedPoint3D, 5> points;
    Array<int, 2>              simpleInts;

    bool operator!=(const VersionedArray1& other) const
    {
        if (other.points.size() < points.size())
            return true;
        for (size_t i = 0; i < points.size(); ++i)
        {
            if (points[i].x != other.points[i].x)
                return true;
            if (points[i].y != other.points[i].y)
                return true;
        }
        if (simpleInts.size() > other.points.size())
            return false;
        for (size_t i = 0; i < simpleInts.size(); ++i)
        {
            if (simpleInts[i] != other.simpleInts[i])
                return false;
        }
        return false;
    }
};
SC_META_STRUCT_BEGIN(SC::VersionedArray2)
SC_META_STRUCT_MEMBER(0, points)
SC_META_STRUCT_MEMBER(1, simpleInts)
SC_META_STRUCT_END()

struct SC::SerializationTest : public SC::TestCase
{
    SerializationTest(SC::TestReport& report) : TestCase(report, "SerializationTest")
    {
        using namespace SC;
        if (test_section("Primitive Structure Write"))
        {
            PrimitiveStruct                   primitive;
            Serialization::SimpleBinaryWriter writer;
            auto&                             destination = writer.destination;
            SC_TEST_EXPECT(writer.write(primitive));
            SC_TEST_EXPECT(destination.numWrites == 3);
            for (int i = 0; i < 3; ++i)
            {
                SC_TEST_EXPECT(destination.read<uint8_t>() == primitive.arrayValue[i]);
            }
            SC_TEST_EXPECT(destination.read<float>() == primitive.floatValue);
            SC_TEST_EXPECT(destination.read<int64_t>() == primitive.int64Value);
        }
        if (test_section("Primitive Structure Read"))
        {
            PrimitiveStruct                   primitive;
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(primitive));
            SC_TEST_EXPECT(writer.destination.numWrites == 3);
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            PrimitiveStruct primitiveRead;
            memset(&primitiveRead, 0, sizeof(primitiveRead));
            SC_TEST_EXPECT(reader.read(primitiveRead));
            SC_TEST_EXPECT(reader.source.numReads == 3);
            SC_TEST_EXPECT(not(primitive != primitiveRead));
        }
        if (test_section("TopLevel Structure Read"))
        {
            TopLevelStruct                    topLevel;
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(topLevel));
            SC_TEST_EXPECT(writer.destination.numWrites == 8);
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            TopLevelStruct topLevelRead;
            memset(&topLevelRead, 0, sizeof(topLevelRead));
            SC_TEST_EXPECT(reader.read(topLevelRead));
            SC_TEST_EXPECT(reader.source.numReads == 8);
            SC_TEST_EXPECT(not(topLevel != topLevelRead));
        }
        if (test_section("VectorStructSimple"))
        {
            VectorStructSimple topLevel;
            (void)topLevel.vectorOfInts.push_back(1);
            (void)topLevel.vectorOfInts.push_back(2);
            (void)topLevel.vectorOfInts.push_back(3);
            (void)topLevel.vectorOfInts.push_back(4);
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(topLevel));
            SC_TEST_EXPECT(writer.destination.numWrites == 4);
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            VectorStructSimple topLevelRead;
            SC_TEST_EXPECT(reader.read(topLevelRead));
            SC_TEST_EXPECT(reader.source.numReads == 4);
            SC_TEST_EXPECT(topLevelRead.emptyVector.size() == 0);
            SC_TEST_EXPECT(topLevelRead.vectorOfInts.size() == 4);
            for (size_t idx = 0; idx < topLevel.vectorOfInts.size(); ++idx)
            {
                SC_TEST_EXPECT(topLevel.vectorOfInts[idx] = topLevelRead.vectorOfInts[idx]);
            }
        }
        if (test_section("VectorStructComplex"))
        {
            VectorStructComplex topLevel;
            (void)topLevel.vectorOfStrings.push_back("asdasdasd1"_sv);
            (void)topLevel.vectorOfStrings.push_back("asdasdasd2"_sv);
            (void)topLevel.vectorOfStrings.push_back("asdasdasd3"_sv);
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(topLevel));
            SC_TEST_EXPECT(writer.destination.numWrites == 7);
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            VectorStructComplex topLevelRead;
            SC_TEST_EXPECT(reader.read(topLevelRead));
            SC_TEST_EXPECT(reader.source.numReads == 7);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings.size() == 3);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[0] == "asdasdasd1"_sv);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[1] == "asdasdasd2"_sv);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[2] == "asdasdasd3"_sv);
        }
        if (test_section("VersionedStruct1/2"))
        {
            VersionedStruct1                  struct1;
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(struct1));
            Serialization::SimpleBinaryReaderVersioned reader;
            VersionedStruct2                           struct2;
            auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<VersionedStruct1>();
            // ReflectionTest::printFlatSchema(flatSchema.properties.values, flatSchema.names.values);
            Span<const void> readSpan(writer.destination.buffer.data(), writer.destination.buffer.size());
            SC_TEST_EXPECT(reader.read(struct2, readSpan, flatSchema.propertiesAsSpan(), flatSchema.namesAsSpan()));
            SC_TEST_EXPECT(not(struct2 != struct1));
        }
        if (test_section("VersionedArray1/2"))
        {
            VersionedArray1 array1;
            (void)array1.points.push_back({1.0f, 2.0f});
            (void)array1.points.push_back({3.0f, 4.0f});
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(array1));
            SC_TEST_EXPECT(writer.destination.numWrites == 7);
            Serialization::SimpleBinaryReaderVersioned reader;
            VersionedArray2                            array2;
            auto             flatSchema = Reflection::FlatSchemaCompiler<>::compile<VersionedArray1>();
            Span<const void> readSpan(writer.destination.buffer.data(), writer.destination.buffer.size());
            SC_TEST_EXPECT(reader.read(array2, readSpan, flatSchema.propertiesAsSpan(), flatSchema.namesAsSpan()));
            SC_TEST_EXPECT(array2.points.size() == 2);
            SC_TEST_EXPECT(array1.simpleInts.size() == 3); // It's dopping one element
            SC_TEST_EXPECT(array2.simpleInts.size() == 2); // It's dopping one element
            SC_TEST_EXPECT(not(array2 != array1));
        }
    }
};
