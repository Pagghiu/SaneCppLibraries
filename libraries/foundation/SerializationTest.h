#pragma once
#include "Console.h"
#include "Reflection.h"
#include "ReflectionFlatSchemaCompiler.h"
#include "ReflectionSC.h"
//#include "ReflectionTest.h"
#include "StringBuilder.h"
#include "Test.h"

// TODO: Support reordering
// TODO: Optimize for memcpy-able types (example: Vector<Point3> should be memcopyed)
// TODO: Support SmallVector
// TODO: Streaming

namespace SC
{
struct BufferDestination
{
    SC::Vector<uint8_t> buffer;

    [[nodiscard]] bool write(const void* source, size_t length)
    {
        numWrites++;
        return buffer.appendCopy(static_cast<const uint8_t*>(source), length);
    }

    size_t index = 0;

    [[nodiscard]] bool read(void* destination, size_t numBytes)
    {
        if (index + numBytes > buffer.size())
            return false;
        numReads++;
        memcpy(destination, &buffer[index], numBytes);
        index += numBytes;
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
struct BinaryReader
{
    const uint8_t* memory;
    constexpr BinaryReader(const uint8_t* memory) : memory(memory) {}

    template <typename T>
    constexpr T readAt(uint64_t offset)
    {
        auto beginOfInt = memory + offset;
        T    outSize    = 0;
        for (int i = 0; i < sizeof(T); ++i)
        {
            outSize |= static_cast<T>(beginOfInt[i]) << (i * 8);
        }
        return outSize;
    }
};
struct CArrayAccess
{
    static constexpr Reflection::MetaType getType() { return Reflection::MetaType::TypeArray; }
    static constexpr bool getSizeInBytes(Reflection::MetaProperties property, const uint8_t* object, uint64_t& outSize)
    {
        outSize = property.size;
        return true;
    }
    template <typename T>
    static constexpr T* getItemBegin(T* object)
    {
        return object;
    }
    static bool resize(uint8_t* object, Reflection::MetaProperties property, uint64_t size)
    {
        return size <= property.getCustomUint32();
    }
};

struct SCArrayAccess
{
    static constexpr Reflection::MetaType getType() { return Reflection::MetaType::TypeSCArray; }
    static constexpr bool getSizeInBytes(Reflection::MetaProperties property, const uint8_t* object, uint64_t& outSize)
    {
        outSize = BinaryReader(object).readAt<uint32_t>(0);
        return true;
    }
    template <typename T>
    static constexpr T* getItemBegin(T* object)
    {
        return object + sizeof(SegmentHeader);
    }
    static bool resize(uint8_t* object, Reflection::MetaProperties property, uint64_t size)
    {
        return size <= property.getCustomUint32();
    }
};

struct SCVectorAccess
{
    static constexpr Reflection::MetaType getType() { return Reflection::MetaType::TypeSCVector; }
    static bool getSizeInBytes(Reflection::MetaProperties property, const uint8_t* object, uint64_t& outSize)
    {
        static_assert(sizeof(uint8_t*) == 8, "");
        outSize = reinterpret_cast<const SC::Vector<uint8_t>*>(object)->size();
        return true;
    }

    static uint8_t* getItemBegin(uint8_t* object) { return reinterpret_cast<SC::Vector<uint8_t>*>(object)->data(); }

    static const uint8_t* getItemBegin(const uint8_t* object)
    {
        return reinterpret_cast<const SC::Vector<const uint8_t>*>(object)->data();
    }
    static bool resize(uint8_t* object, Reflection::MetaProperties property, uint64_t size, bool initialize)
    {
        SC::Vector<uint8_t>& vector = *reinterpret_cast<SC::Vector<uint8_t>*>(object);
        if (initialize)
        {
            return vector.resize(size);
        }
        else
        {
            return vector.resizeWithoutInitializing(size);
        }
    }
};
struct ArrayAccess
{
    [[nodiscard]] static bool getSizeInBytes(Reflection::MetaProperties property, const uint8_t* object,
                                             uint64_t& outSize)
    {
        if (property.type == CArrayAccess::getType())
            return CArrayAccess::getSizeInBytes(property, object, outSize);
        else if (property.type == SCArrayAccess::getType())
            return SCArrayAccess::getSizeInBytes(property, object, outSize);
        else if (property.type == SCVectorAccess::getType())
            return SCVectorAccess::getSizeInBytes(property, object, outSize);
        return false;
    }
    template <typename T>
    [[nodiscard]] static constexpr T* getItemBegin(Reflection::MetaProperties property, T* object)
    {
        if (property.type == CArrayAccess::getType())
            return CArrayAccess::getItemBegin(object);
        else if (property.type == SCArrayAccess::getType())
            return SCArrayAccess::getItemBegin(object);
        else if (property.type == SCVectorAccess::getType())
            return SCVectorAccess::getItemBegin(object);
        return nullptr;
    }
    [[nodiscard]] static bool resize(uint8_t* object, Reflection::MetaProperties property, uint64_t size,
                                     bool initialize)
    {
        if (property.type == CArrayAccess::getType())
            return CArrayAccess::resize(object, property, size);
        else if (property.type == SCArrayAccess::getType())
            return SCArrayAccess::resize(object, property, size);
        else if (property.type == SCVectorAccess::getType())
            return SCVectorAccess::resize(object, property, size, initialize);
        return false;
    }

    [[nodiscard]] static bool hasVariableSize(Reflection::MetaType type)
    {
        if (type == CArrayAccess::getType())
            return false;
        else if (type == SCArrayAccess::getType())
            return true;
        else if (type == SCVectorAccess::getType())
            return true;
        return true;
    }
};
struct SimpleBinaryWriter
{
    Span<const Reflection::MetaProperties> properties;
    Span<const Reflection::MetaStringView> names;
    BufferDestination                      destination;
    const void*                            sinkObject;
    int                                    typeIndex;
    Reflection::MetaProperties             property;

    template <typename T>
    [[nodiscard]] bool write(const T& object)
    {
        auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        // ReflectionTest::printFlatSchema(flatSchema.properties.values, flatSchema.names.values);
        properties = flatSchema.propertiesAsSpan();
        names      = flatSchema.namesAsSpan();
        sinkObject = &object;
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
            return destination.write(sinkObject, property.size);
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
        const auto     selfProperty  = property;
        const auto     selfTypeIndex = typeIndex;
        const uint8_t* structRoot    = static_cast<const uint8_t*>(sinkObject);
        for (int16_t idx = 0; idx < selfProperty.numSubAtoms; ++idx)
        {
            typeIndex  = selfTypeIndex + idx + 1;
            sinkObject = structRoot + properties.data[typeIndex].offset;
            if (properties.data[typeIndex].getLinkIndex() >= 0)
                typeIndex = properties.data[typeIndex].getLinkIndex();
            SC_TRY_IF(write());
        }
        return true;
    }

    [[nodiscard]] bool writeArray()
    {
        const auto     arrayProperty  = property;
        const auto     arrayTypeIndex = typeIndex;
        const uint8_t* arrayRoot      = static_cast<const uint8_t*>(sinkObject);
        uint64_t       numBytes       = 0;
        SC_TRY_IF(ArrayAccess::getSizeInBytes(arrayProperty, arrayRoot, numBytes));
        if (ArrayAccess::hasVariableSize(arrayProperty.type))
        {
            SC_TRY_IF(destination.write(&numBytes, sizeof(numBytes)));
        }
        typeIndex                  = arrayTypeIndex + 1;
        const bool     isPrimitive = Reflection::IsPrimitiveType(properties.data[typeIndex].type);
        const uint8_t* arrayStart  = ArrayAccess::getItemBegin(arrayProperty, arrayRoot);
        if (isPrimitive)
        {
            SC_TRY_IF(destination.write(arrayStart, numBytes));
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
                sinkObject = arrayStart + idx * itemSize;
                typeIndex  = itemTypeIndex;
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
    void*                                  sinkObject    = nullptr;
    BufferDestination                      source;

    template <typename T>
    [[nodiscard]] bool read(T& object)
    {
        auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        // ReflectionTest::printFlatSchema(flatSchema.properties.values, flatSchema.names.values);
        sinkProperties = flatSchema.propertiesAsSpan();
        sinkNames      = flatSchema.namesAsSpan();
        sinkObject     = &object;
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
            return source.read(sinkObject, sinkProperty.size);
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
        const auto structProperty  = sinkProperty;
        const auto structTypeIndex = sinkTypeIndex;
        uint8_t*   structRoot      = static_cast<uint8_t*>(sinkObject);
        for (int16_t idx = 0; idx < structProperty.numSubAtoms; ++idx)
        {
            sinkTypeIndex = structTypeIndex + idx + 1;
            sinkObject    = structRoot + sinkProperties.data[sinkTypeIndex].offset;
            if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
                sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
            SC_TRY_IF(read());
        }
        return true;
    }

    [[nodiscard]] bool readArray()
    {
        const auto arrayProperty  = sinkProperty;
        const auto arrayTypeIndex = sinkTypeIndex;
        sinkTypeIndex             = arrayTypeIndex + 1;
        uint64_t   numBytes       = 0;
        uint8_t*   arrayRoot      = static_cast<uint8_t*>(sinkObject);
        const bool isPrimitive    = Reflection::IsPrimitiveType(sinkProperties.data[sinkTypeIndex].type);
        if (ArrayAccess::hasVariableSize(arrayProperty.type))
        {
            SC_TRY_IF(source.read(&numBytes, sizeof(numBytes)));
            SC_TRY_IF(ArrayAccess::resize(arrayRoot, arrayProperty, numBytes, not isPrimitive));
        }
        else
        {
            SC_TRY_IF(ArrayAccess::getSizeInBytes(arrayProperty, arrayRoot, numBytes));
        }
        uint8_t* arrayStart = ArrayAccess::getItemBegin(arrayProperty, arrayRoot);
        if (isPrimitive)
        {
            SC_TRY_IF(source.read(arrayStart, numBytes));
        }
        else
        {
            const auto itemSize = sinkProperties.data[sinkTypeIndex].size;
            if (sinkProperties.data[sinkTypeIndex].getLinkIndex() >= 0)
                sinkTypeIndex = sinkProperties.data[sinkTypeIndex].getLinkIndex();
            const auto numElements   = numBytes / itemSize;
            const auto itemTypeIndex = sinkTypeIndex;
            for (uint64_t idx = 0; idx < numElements; ++idx)
            {
                sinkTypeIndex = itemTypeIndex;
                sinkObject    = arrayStart + idx * itemSize;
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
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings.size() == 3);
            SC_TEST_EXPECT(reader.source.numReads == 7);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[0] == "asdasdasd1"_sv);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[1] == "asdasdasd2"_sv);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[2] == "asdasdasd3"_sv);
        }
    }
};
