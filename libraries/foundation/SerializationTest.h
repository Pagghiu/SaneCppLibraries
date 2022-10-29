#pragma once
#include "Console.h"
#include "Reflection.h"
#include "ReflectionFlatSchemaCompiler.h"
#include "ReflectionSC.h"
//#include "ReflectionTest.h"
#include "StringBuilder.h"
#include "Test.h"

// TODO: Support reordering
// TODO: Optimize Trivial Types arrays write/reads with memcpy
// TODO: Support SmallVector

namespace SC
{
struct BufferDestination
{
    SC::Vector<uint8_t> buffer;
    // clang-format off
    [[nodiscard]]  bool write(uint8_t value)
    {
        // Console::c_printf("uint8_t=%d\n", value);
        return buffer.appendCopy(&value, sizeof(value));
    }
    [[nodiscard]]  bool write(uint16_t value)
    {
        // Console::c_printf("uint16_t=%d\n", value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    [[nodiscard]]  bool write(uint32_t value)
    {
        // Console::c_printf("uint32_t=%d\n", value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    [[nodiscard]]  bool write(uint64_t value)
    {
        // Console::c_printf("uint64_t=%d\n", (int)value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    [[nodiscard]]  bool write(int8_t value)
    {
        // Console::c_printf("int8_t=%d\n", value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    [[nodiscard]]  bool write(int16_t value)
    {
        // Console::c_printf("int16_t=%d\n", value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    [[nodiscard]]  bool write(int32_t value)
    {
        // Console::c_printf("int32_t=%d\n", value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    [[nodiscard]]  bool write(int64_t value)
    {
        // Console::c_printf("int64_t=%d\n", (int)value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    [[nodiscard]]  bool write(float value)
    {
        // Console::c_printf("float=%f\n", value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    [[nodiscard]]  bool write(double value)
    {
        // Console::c_printf("double=%f\n", value);
        return buffer.appendCopy(reinterpret_cast<uint8_t*>(&value), sizeof(value));
    }
    // clang-format on

    int index = 0;
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
struct CArrayReadAccess
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
    static uint64_t getItemSize(Reflection::MetaProperties property)
    {
        return (property.size) / property.getCustomUint32();
    }
};

struct SCArrayReadAccess
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
    static uint64_t getItemSize(Reflection::MetaProperties property)
    {
        return (property.size - sizeof(SegmentHeader)) / property.getCustomUint32();
    }
};

struct SCVectorReadAccess
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
    static bool resize(uint8_t* object, Reflection::MetaProperties property, uint64_t size)
    {
        SC::Vector<uint8_t>& vector = *reinterpret_cast<SC::Vector<uint8_t>*>(object);
        return vector.resize(size);
    }
    static uint64_t getItemSize(Reflection::MetaProperties property) { return property.getCustomUint32(); }
};
struct ArrayReadAccess
{
    static bool getSizeInBytes(Reflection::MetaProperties property, const uint8_t* object, uint64_t& outSize)
    {
        if (property.type == CArrayReadAccess::getType())
            return CArrayReadAccess::getSizeInBytes(property, object, outSize);
        else if (property.type == SCArrayReadAccess::getType())
            return SCArrayReadAccess::getSizeInBytes(property, object, outSize);
        else if (property.type == SCVectorReadAccess::getType())
            return SCVectorReadAccess::getSizeInBytes(property, object, outSize);
        return false;
    }
    template <typename T>
    static constexpr T* getItemBegin(Reflection::MetaProperties property, T* object)
    {
        if (property.type == CArrayReadAccess::getType())
            return CArrayReadAccess::getItemBegin(object);
        else if (property.type == SCArrayReadAccess::getType())
            return SCArrayReadAccess::getItemBegin(object);
        else if (property.type == SCVectorReadAccess::getType())
            return SCVectorReadAccess::getItemBegin(object);
        return nullptr;
    }
    static bool resize(uint8_t* object, Reflection::MetaProperties property, uint64_t size)
    {
        if (property.type == CArrayReadAccess::getType())
            return CArrayReadAccess::resize(object, property, size);
        else if (property.type == SCArrayReadAccess::getType())
            return SCArrayReadAccess::resize(object, property, size);
        else if (property.type == SCVectorReadAccess::getType())
            return SCVectorReadAccess::resize(object, property, size);
        return false;
    }

    static bool hasVariableSize(Reflection::MetaType type)
    {
        if (type == CArrayReadAccess::getType())
            return false;
        else if (type == SCArrayReadAccess::getType())
            return true;
        else if (type == SCVectorReadAccess::getType())
            return true;
        return true;
    }

    static uint64_t getItemSize(Reflection::MetaProperties property)
    {
        if (property.type == CArrayReadAccess::getType())
            return CArrayReadAccess::getItemSize(property);
        else if (property.type == SCArrayReadAccess::getType())
            return SCArrayReadAccess::getItemSize(property);
        else if (property.type == SCVectorReadAccess::getType())
            return SCVectorReadAccess::getItemSize(property);
        return false;
    }
};
struct SimpleBinaryWriter
{
    Span<const Reflection::MetaProperties> properties;
    Span<const Reflection::MetaStringView> names;
    BufferDestination                      destination;
    const void*                            untypedObject;
    int                                    typeIndex;
    Reflection::MetaProperties             property;

    template <typename T>
    [[nodiscard]] bool write(const T& object)
    {
        auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        // ReflectionTest::printFlatSchema(flatSchema.properties.values, flatSchema.names.values);
        properties    = flatSchema.propertiesAsSpan();
        names         = flatSchema.namesAsSpan();
        untypedObject = &object;
        typeIndex     = 0;
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
        case Reflection::MetaType::TypeInvalid: return false;
        case Reflection::MetaType::TypeUINT8:   return destination.write(*static_cast<const uint8_t*>(untypedObject));
        case Reflection::MetaType::TypeUINT16:  return destination.write(*static_cast<const uint16_t*>(untypedObject));
        case Reflection::MetaType::TypeUINT32:  return destination.write(*static_cast<const uint32_t*>(untypedObject));
        case Reflection::MetaType::TypeUINT64:  return destination.write(*static_cast<const uint64_t*>(untypedObject));
        case Reflection::MetaType::TypeINT8:    return destination.write(*static_cast<const int8_t*>(untypedObject));
        case Reflection::MetaType::TypeINT16:   return destination.write(*static_cast<const int16_t*>(untypedObject));
        case Reflection::MetaType::TypeINT32:   return destination.write(*static_cast<const int32_t*>(untypedObject));
        case Reflection::MetaType::TypeINT64:   return destination.write(*static_cast<const int64_t*>(untypedObject));
        case Reflection::MetaType::TypeFLOAT32: return destination.write(*static_cast<const float*>(untypedObject));
        case Reflection::MetaType::TypeDOUBLE64: return destination.write(*static_cast<const double*>(untypedObject));

        case Reflection::MetaType::TypeStruct:  return writeStruct();
        case Reflection::MetaType::TypeArray:   return writeArray();
        case Reflection::MetaType::TypeSCArray: return writeArray();
        case Reflection::MetaType::TypeSCVector:return writeArray();
            // clang-format on
        }
        return true;
    }

    [[nodiscard]] bool writeStruct()
    {
        const auto     selfProperty  = property;
        const auto     selfTypeIndex = typeIndex;
        const uint8_t* memberObject  = static_cast<const uint8_t*>(untypedObject);
        for (int16_t idx = 0; idx < selfProperty.numSubAtoms; ++idx)
        {
            typeIndex                 = selfTypeIndex + idx + 1;
            const auto memberProperty = properties.data[typeIndex];
            if (properties.data[typeIndex].getLinkIndex() >= 0)
                typeIndex = properties.data[typeIndex].getLinkIndex();
            const auto targetProperty = properties.data[typeIndex];
            property                  = targetProperty;
            untypedObject             = memberObject + memberProperty.offset;
            SC_TRY_IF(write());
        }
        return true;
    }

    [[nodiscard]] bool writeArray()
    {
        const auto selfProperty  = property;
        const auto selfTypeIndex = typeIndex;
        typeIndex                = selfTypeIndex + 1;
        const uint8_t* arrayRoot =
            ArrayReadAccess::getItemBegin(selfProperty, static_cast<const uint8_t*>(untypedObject));
        uint64_t numActiveBytes = 0;
        SC_TRY_IF(
            ArrayReadAccess::getSizeInBytes(selfProperty, static_cast<const uint8_t*>(untypedObject), numActiveBytes));
        if (ArrayReadAccess::hasVariableSize(selfProperty.type))
        {
            SC_TRY_IF(destination.write(numActiveBytes));
        }
        const auto itemSize          = ArrayReadAccess::getItemSize(selfProperty);
        const auto numActiveElements = numActiveBytes / ArrayReadAccess::getItemSize(selfProperty);
        for (uint64_t idx = 0; idx < numActiveElements; ++idx)
        {
            typeIndex = selfTypeIndex + 1;
            if (properties.data[typeIndex].getLinkIndex() >= 0)
                typeIndex = properties.data[typeIndex].getLinkIndex();
            property      = properties.data[typeIndex];
            untypedObject = arrayRoot + idx * itemSize;
            SC_TRY_IF(write());
        }
        return true;
    }
};

struct SimpleBinaryReader
{
    Span<const Reflection::MetaProperties> properties;
    Span<const Reflection::MetaStringView> names;
    BufferDestination                      source;
    void*                                  untypedObject;
    int                                    typeIndex;
    Reflection::MetaProperties             property;

    template <typename T>
    [[nodiscard]] bool read(T& object)
    {
        auto flatSchema = Reflection::FlatSchemaCompiler<>::compile<T>();
        // ReflectionTest::printFlatSchema(flatSchema.properties.values, flatSchema.names.values);
        properties    = flatSchema.propertiesAsSpan();
        names         = flatSchema.namesAsSpan();
        untypedObject = &object;
        typeIndex     = 0;
        if (properties.size == 0 || properties.data[0].type != Reflection::MetaType::TypeStruct)
        {
            return false;
        }
        return read();
    }

    [[nodiscard]] bool read()
    {
        property = properties.data[typeIndex];
        switch (property.type)
        {
        // clang-format off
        case Reflection::MetaType::TypeInvalid: return false;
        case Reflection::MetaType::TypeUINT8:   *static_cast<uint8_t*>(untypedObject)  = source.read<uint8_t>(); break;
        case Reflection::MetaType::TypeUINT16:  *static_cast<uint16_t*>(untypedObject) = source.read<uint16_t>(); break;
        case Reflection::MetaType::TypeUINT32:  *static_cast<uint32_t*>(untypedObject) = source.read<uint32_t>(); break;
        case Reflection::MetaType::TypeUINT64:  *static_cast<uint64_t*>(untypedObject) = source.read<uint64_t>(); break;
        case Reflection::MetaType::TypeINT8:    *static_cast<int8_t*>(untypedObject)   = source.read<int8_t>(); break;
        case Reflection::MetaType::TypeINT16:   *static_cast<int16_t*>(untypedObject)  = source.read<int16_t>(); break;
        case Reflection::MetaType::TypeINT32:   *static_cast<int32_t*>(untypedObject)  = source.read<int32_t>(); break;
        case Reflection::MetaType::TypeINT64:   *static_cast<int64_t*>(untypedObject)  = source.read<int64_t>(); break;
        case Reflection::MetaType::TypeFLOAT32: *static_cast<float*>(untypedObject)    = source.read<float>(); break;
        case Reflection::MetaType::TypeDOUBLE64:*static_cast<double*>(untypedObject)   = source.read<double>(); break;

        case Reflection::MetaType::TypeStruct:  return readStruct();
        case Reflection::MetaType::TypeArray:   return readArray();
        case Reflection::MetaType::TypeSCArray: return readArray();
        case Reflection::MetaType::TypeSCVector:return readArray();
            // clang-format on
        }
        return true;
    }

    [[nodiscard]] bool readStruct()
    {
        const auto selfProperty  = property;
        const auto selfTypeIndex = typeIndex;
        uint8_t*   memberObject  = static_cast<uint8_t*>(untypedObject);
        for (int16_t idx = 0; idx < selfProperty.numSubAtoms; ++idx)
        {
            typeIndex                 = selfTypeIndex + idx + 1;
            const auto memberProperty = properties.data[typeIndex];
            if (properties.data[typeIndex].getLinkIndex() >= 0)
                typeIndex = properties.data[typeIndex].getLinkIndex();
            const auto targetProperty = properties.data[typeIndex];
            property                  = targetProperty;
            untypedObject             = memberObject + memberProperty.offset;
            SC_TRY_IF(read());
        }
        return true;
    }

    [[nodiscard]] bool readArray()
    {
        const auto selfProperty  = property;
        const auto selfTypeIndex = typeIndex;
        typeIndex                = selfTypeIndex + 1;
        uint64_t numActiveBytes  = 0;
        if (ArrayReadAccess::hasVariableSize(selfProperty.type))
        {
            numActiveBytes = source.read<uint64_t>();
            ArrayReadAccess::resize(static_cast<uint8_t*>(untypedObject), selfProperty, numActiveBytes);
        }
        else
        {
            uint8_t* arrayRoot = ArrayReadAccess::getItemBegin(selfProperty, static_cast<uint8_t*>(untypedObject));
            SC_TRY_IF(ArrayReadAccess::getSizeInBytes(selfProperty, arrayRoot, numActiveBytes));
        }
        uint8_t*   arrayRoot = ArrayReadAccess::getItemBegin(selfProperty, static_cast<uint8_t*>(untypedObject));
        const auto numActiveElements = numActiveBytes / ArrayReadAccess::getItemSize(selfProperty);
        for (uint64_t idx = 0; idx < numActiveElements; ++idx)
        {
            typeIndex                    = selfTypeIndex + 1;
            const auto arrayItemProperty = properties.data[typeIndex];
            if (properties.data[typeIndex].getLinkIndex() >= 0)
                typeIndex = properties.data[typeIndex].getLinkIndex();
            const auto targetProperty = properties.data[typeIndex];
            property                  = targetProperty;
            untypedObject             = arrayRoot + idx * arrayItemProperty.size;
            SC_TRY_IF(read());
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
    Array<int, 3>   arrayInt  = {1, 2, 3};

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
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            PrimitiveStruct primitiveRead;
            memset(&primitiveRead, 0, sizeof(primitiveRead));
            SC_TEST_EXPECT(reader.read(primitiveRead));
            SC_TEST_EXPECT(not(primitive != primitiveRead));
        }
        if (test_section("TopLevel Structure Read"))
        {
            TopLevelStruct                    topLevel;
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(topLevel));
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            TopLevelStruct topLevelRead;
            memset(&topLevelRead, 0, sizeof(topLevelRead));
            SC_TEST_EXPECT(reader.read(topLevelRead));
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
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            VectorStructSimple topLevelRead;
            SC_TEST_EXPECT(reader.read(topLevelRead));
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
            (void)topLevel.vectorOfStrings.push_back("asd1"_sv);
            (void)topLevel.vectorOfStrings.push_back("asd2"_sv);
            (void)topLevel.vectorOfStrings.push_back("asd3"_sv);
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(topLevel));
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            VectorStructComplex topLevelRead;
            SC_TEST_EXPECT(reader.read(topLevelRead));
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings.size() == 3);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[0] == "asd1"_sv);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[1] == "asd2"_sv);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[2] == "asd3"_sv);
        }
    }
};
