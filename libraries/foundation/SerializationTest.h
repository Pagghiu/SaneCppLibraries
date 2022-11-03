#pragma once
#include "Serialization.h"
#include "Test.h"

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
    uint8_t arrayValue[4] = {0, 1, 2, 3};
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

namespace SC
{
struct ConversionStruct1;
struct ConversionStruct2;
} // namespace SC

struct SC::ConversionStruct1
{
    uint32_t intToFloat         = 1;
    float    floatToInt         = 1;
    uint16_t uint16To32         = 1;
    int16_t  signed16ToUnsigned = 1;
};
SC_META_STRUCT_BEGIN(SC::ConversionStruct1)
SC_META_STRUCT_MEMBER(0, intToFloat)
SC_META_STRUCT_MEMBER(1, floatToInt)
SC_META_STRUCT_MEMBER(2, uint16To32)
SC_META_STRUCT_MEMBER(3, signed16ToUnsigned)
SC_META_STRUCT_END()

struct SC::ConversionStruct2
{
    float    intToFloat         = 0;
    uint32_t floatToInt         = 0;
    uint32_t uint16To32         = 0;
    uint16_t signed16ToUnsigned = 0;
};
SC_META_STRUCT_BEGIN(SC::ConversionStruct2)
SC_META_STRUCT_MEMBER(0, intToFloat)
SC_META_STRUCT_MEMBER(1, floatToInt)
SC_META_STRUCT_MEMBER(2, uint16To32)
SC_META_STRUCT_MEMBER(3, signed16ToUnsigned)
SC_META_STRUCT_END()

struct SC::SerializationTest : public SC::TestCase
{
    // Used only for the test
    template <typename T>
    [[nodiscard]] constexpr const T readPrimitive(const Vector<uint8_t>& buffer, int& index)
    {
        T alignedRead;
        memcpy(&alignedRead, &buffer[index], sizeof(T));
        index += sizeof(T);
        return alignedRead;
    }

    SerializationTest(SC::TestReport& report) : TestCase(report, "SerializationTest")
    {
        using namespace SC;
        if (test_section("Primitive Structure Write"))
        {
            PrimitiveStruct                   primitive;
            Serialization::SimpleBinaryWriter writer;
            auto&                             destination = writer.destination;
            SC_TEST_EXPECT(writer.write(primitive));
            SC_TEST_EXPECT(writer.numberOfOperations == 1);
            int index = 0;
            for (int i = 0; i < 4; ++i)
            {
                SC_TEST_EXPECT(readPrimitive<uint8_t>(destination.buffer, index) == primitive.arrayValue[i]);
            }
            SC_TEST_EXPECT(readPrimitive<float>(destination.buffer, index) == primitive.floatValue);
            SC_TEST_EXPECT(readPrimitive<int64_t>(destination.buffer, index) == primitive.int64Value);
        }
        if (test_section("Primitive Structure Read"))
        {
            PrimitiveStruct                   primitive;
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(primitive));
            SC_TEST_EXPECT(writer.numberOfOperations == 1);
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            PrimitiveStruct primitiveRead;
            memset(&primitiveRead, 0, sizeof(primitiveRead));
            SC_TEST_EXPECT(reader.read(primitiveRead));
            SC_TEST_EXPECT(reader.numberOfOperations == 1);
            SC_TEST_EXPECT(not(primitive != primitiveRead));
        }
        if (test_section("TopLevel Structure Read"))
        {
            using namespace Reflection;
            //            auto testStruct = FlatSchemaCompiler<>::compile<TopLevelStruct>();
            //            ReflectionTest::printFlatSchema(testStruct.properties.values, testStruct.names.values);
            //            FlatSchemaCompiler<>::markRecursiveProperties(testStruct, 0);
            //            auto testStructFlags = testStruct.properties.values[0].getCustomUint32();
            //            SC_TEST_EXPECT(testStructFlags & static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked));

            TopLevelStruct                    topLevel;
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(topLevel));
            SC_TEST_EXPECT(writer.numberOfOperations == 3);
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            TopLevelStruct topLevelRead;
            memset(&topLevelRead, 0, sizeof(topLevelRead));
            SC_TEST_EXPECT(reader.read(topLevelRead));
            SC_TEST_EXPECT(reader.numberOfOperations == 3);
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
            SC_TEST_EXPECT(writer.numberOfOperations == 4);
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            VectorStructSimple topLevelRead;
            SC_TEST_EXPECT(reader.read(topLevelRead));
            SC_TEST_EXPECT(reader.numberOfOperations == 4);
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
            SC_TEST_EXPECT(writer.numberOfOperations == 7);
            Serialization::SimpleBinaryReader reader;
            reader.source.buffer = writer.destination.buffer;
            VectorStructComplex topLevelRead;
            SC_TEST_EXPECT(reader.read(topLevelRead));
            SC_TEST_EXPECT(reader.numberOfOperations == 7);
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
            SC_TEST_EXPECT(writer.numberOfOperations == 4);
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
        if (test_section("ConversionStruct1/2"))
        {
            ConversionStruct1                 struct1;
            ConversionStruct2                 struct2;
            Serialization::SimpleBinaryWriter writer;
            SC_TEST_EXPECT(writer.write(struct1));
            Serialization::SimpleBinaryReaderVersioned reader;
            auto             flatSchema = Reflection::FlatSchemaCompiler<>::compile<ConversionStruct1>();
            Span<const void> readSpan(writer.destination.buffer.data(), writer.destination.buffer.size());
            SC_TEST_EXPECT(reader.read(struct2, readSpan, flatSchema.propertiesAsSpan(), flatSchema.namesAsSpan()));
            SC_TEST_EXPECT(struct2.intToFloat == struct1.intToFloat);
            SC_TEST_EXPECT(struct2.floatToInt == struct1.floatToInt);
            SC_TEST_EXPECT(struct2.uint16To32 == struct1.uint16To32);
            SC_TEST_EXPECT(struct2.signed16ToUnsigned == struct1.signed16ToUnsigned);
        }
    }
};
