// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Containers/Array.h"
#include "../../Containers/Vector.h"
#include "../../Reflection/Reflection.h"
#include "../../Strings/String.h"
#include "../../Strings/StringBuilder.h"
#include "../../Strings/StringView.h"
#include "../../System/Console.h"
#include "../../Testing/Testing.h"

namespace SC
{
namespace SerializationSuiteTest
{
struct PrimitiveStruct;
struct NestedStruct;
struct TopLevelStruct;
struct VectorStructSimple;
struct VectorStructComplex;
struct VersionedStruct1;
struct VersionedStruct2;
struct VersionedPoint3D;
struct VersionedPoint2D;
struct VersionedArray1;
struct VersionedArray2;
struct ConversionStruct1;
struct ConversionStruct2;
struct PackedStruct1;
struct PackedStruct2;
template <typename SerializerWriterBuffer, typename SerializerReaderBuffer>
struct TestTemplate;

} // namespace SerializationSuiteTest
} // namespace SC

struct SC::SerializationSuiteTest::PrimitiveStruct
{
    uint8_t arrayValue[4] = {0, 1, 2, 3};
    float   floatValue    = 1.5f;
    int64_t int64Value    = -13;

    bool operator!=(const PrimitiveStruct& other) const
    {
        for (size_t i = 0; i < TypeTraits::SizeOfArray(arrayValue); ++i)
        {
            if (arrayValue[i] != other.arrayValue[i])
                return true;
        }
        if (floatValue != other.floatValue)
            return true;
        if (int64Value != other.int64Value)
            return true;
        return false;
    }
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::PrimitiveStruct)
SC_REFLECT_STRUCT_FIELD(0, arrayValue)
SC_REFLECT_STRUCT_FIELD(1, floatValue)
SC_REFLECT_STRUCT_FIELD(2, int64Value)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::NestedStruct
{
    int16_t         int16Value = 244;
    PrimitiveStruct structsArray[2];
    double          doubleVal = -1.24;
    Array<int, 7>   arrayInt  = {1, 2, 3, 4, 5, 6};

    bool operator!=(const NestedStruct& other) const
    {
        if (int16Value != other.int16Value)
            return true;
        for (size_t i = 0; i < TypeTraits::SizeOfArray(structsArray); ++i)
            if (structsArray[i] != other.structsArray[i])
                return true;
        if (doubleVal != other.doubleVal)
            return true;
        return false;
    }
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::NestedStruct)
SC_REFLECT_STRUCT_FIELD(0, int16Value)
SC_REFLECT_STRUCT_FIELD(1, structsArray)
SC_REFLECT_STRUCT_FIELD(2, doubleVal)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::TopLevelStruct
{
    NestedStruct nestedStruct;

    bool operator!=(const TopLevelStruct& other) const { return nestedStruct != other.nestedStruct; }
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::TopLevelStruct)
SC_REFLECT_STRUCT_FIELD(0, nestedStruct)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::VectorStructSimple
{
    SC::Vector<int> emptyVector;
    SC::Vector<int> vectorOfInts;
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::VectorStructSimple)
SC_REFLECT_STRUCT_FIELD(0, emptyVector)
SC_REFLECT_STRUCT_FIELD(1, vectorOfInts)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::VectorStructComplex
{
    SC::Vector<SC::String> vectorOfStrings;
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::VectorStructComplex)
SC_REFLECT_STRUCT_FIELD(0, vectorOfStrings)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::VersionedStruct1
{
    float          floatValue     = 1.5f;
    int64_t        fieldToRemove  = 12;
    Vector<String> field2ToRemove = {"ASD1", "ASD2", "ASD3"};
    int64_t        int64Value     = -13;
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::VersionedStruct1)
SC_REFLECT_STRUCT_FIELD(2, field2ToRemove)
SC_REFLECT_STRUCT_FIELD(0, floatValue)
SC_REFLECT_STRUCT_FIELD(1, fieldToRemove)
SC_REFLECT_STRUCT_FIELD(3, int64Value)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::VersionedStruct2
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

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::VersionedStruct2)
SC_REFLECT_STRUCT_FIELD(3, int64Value)
SC_REFLECT_STRUCT_FIELD(0, floatValue)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::VersionedPoint3D
{
    float x;
    float y;
    float z;
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::VersionedPoint3D)
SC_REFLECT_STRUCT_FIELD(0, x)
SC_REFLECT_STRUCT_FIELD(1, y)
SC_REFLECT_STRUCT_FIELD(2, z)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::VersionedPoint2D
{
    float x;
    float y;
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::VersionedPoint2D)
SC_REFLECT_STRUCT_FIELD(0, x)
SC_REFLECT_STRUCT_FIELD(1, y)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::VersionedArray1
{
    Vector<VersionedPoint2D> points;
    Vector<int>              simpleInts = {1, 2, 3};
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::VersionedArray1)
SC_REFLECT_STRUCT_FIELD(0, points)
SC_REFLECT_STRUCT_FIELD(1, simpleInts)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::VersionedArray2
{
    Array<VersionedPoint3D, 2> points;
    Array<int, 2>              simpleInts;

    bool operator!=(const VersionedArray1& other) const
    {
        if (other.points.size() < points.size())
            return true;
        for (size_t i = 0; i < points.size(); ++i)
        {
            auto p1x = points[i].x;
            auto p1y = points[i].y;
            auto p2x = other.points[i].x;
            auto p2y = other.points[i].y;
            if (p1x != p2x)
                return true;
            if (p1y != p2y)
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

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::VersionedArray2)
SC_REFLECT_STRUCT_FIELD(0, points)
SC_REFLECT_STRUCT_FIELD(1, simpleInts)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::ConversionStruct1
{
    uint32_t intToFloat         = 1;
    float    floatToInt         = 1;
    uint16_t uint16To32         = 1;
    int16_t  signed16ToUnsigned = 1;
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::ConversionStruct1)
SC_REFLECT_STRUCT_FIELD(0, intToFloat)
SC_REFLECT_STRUCT_FIELD(1, floatToInt)
SC_REFLECT_STRUCT_FIELD(2, uint16To32)
SC_REFLECT_STRUCT_FIELD(3, signed16ToUnsigned)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::ConversionStruct2
{
    float    intToFloat         = 0;
    uint32_t floatToInt         = 0;
    uint32_t uint16To32         = 0;
    uint16_t signed16ToUnsigned = 0;
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::ConversionStruct2)
SC_REFLECT_STRUCT_FIELD(0, intToFloat)
SC_REFLECT_STRUCT_FIELD(1, floatToInt)
SC_REFLECT_STRUCT_FIELD(2, uint16To32)
SC_REFLECT_STRUCT_FIELD(3, signed16ToUnsigned)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::PackedStruct1
{
    uint8_t field0 = 255;
    uint8_t field1 = 255;
    uint8_t field2 = 255;
};
SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::PackedStruct1)
SC_REFLECT_STRUCT_FIELD(0, field0)
SC_REFLECT_STRUCT_FIELD(2, field2)
SC_REFLECT_STRUCT_FIELD(1, field1)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationSuiteTest::PackedStruct2
{
    uint8_t field2 = 255;
    uint8_t field0 = 255;
};
SC_REFLECT_STRUCT_VISIT(SC::SerializationSuiteTest::PackedStruct2)
SC_REFLECT_STRUCT_FIELD(0, field0)
SC_REFLECT_STRUCT_FIELD(2, field2)
SC_REFLECT_STRUCT_LEAVE()

template <typename SerializerWriterBuffer, typename SerializerReaderBuffer>
struct SC::SerializationSuiteTest::TestTemplate : public SC::TestCase
{
    // Used only for the test
    template <typename T>
    [[nodiscard]] constexpr const T readPrimitive(const Vector<uint8_t>& buffer, uint32_t& index)
    {
        T alignedRead;
        memcpy(&alignedRead, &buffer[index], sizeof(T));
        index += sizeof(T);
        return alignedRead;
    }

    TestTemplate(SC::TestReport& report, StringView name) : TestCase(report, name) {}

    template <typename SerializerWriter, typename SerializerReader>
    void runSameVersionTests()
    {
        if (test_section("Primitive Structure Write"))
        {
            PrimitiveStruct        primitive;
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(primitive, writerBuffer));
            SC_TEST_EXPECT(writerBuffer.numberOfOperations == 1);
            uint32_t index = 0;
            for (uint32_t i = 0; i < 4; ++i)
            {
                SC_TEST_EXPECT(readPrimitive<uint8_t>(writerBuffer.buffer, index) == primitive.arrayValue[i]);
            }
            SC_TEST_EXPECT(readPrimitive<float>(writerBuffer.buffer, index) == primitive.floatValue);
            SC_TEST_EXPECT(readPrimitive<int64_t>(writerBuffer.buffer, index) == primitive.int64Value);
        }
        if (test_section("Primitive Structure Read"))
        {
            PrimitiveStruct        primitive;
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(primitive, writerBuffer));
            SC_TEST_EXPECT(writerBuffer.numberOfOperations == 1);
            SerializerReaderBuffer reader;
            reader.buffer = move(writerBuffer.buffer);
            PrimitiveStruct primitiveRead;
            memset(&primitiveRead, 0, sizeof(primitiveRead));
            SerializerReader serializerReader;
            SC_TEST_EXPECT(serializerReader.serialize(primitiveRead, reader));
            SC_TEST_EXPECT(reader.numberOfOperations == writerBuffer.numberOfOperations);
            SC_TEST_EXPECT(not(primitive != primitiveRead));
        }
        if (test_section("TopLevel Structure Read"))
        {
            TopLevelStruct         topLevel;
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(topLevel, writerBuffer));
            SC_TEST_EXPECT(writerBuffer.numberOfOperations == 3);
            SerializerReaderBuffer reader;
            reader.buffer = move(writerBuffer.buffer);
            TopLevelStruct topLevelRead;
            memset(&topLevelRead, 0, sizeof(topLevelRead));
            SerializerReader serializerReader;
            SC_TEST_EXPECT(serializerReader.serialize(topLevelRead, reader));
            SC_TEST_EXPECT(reader.numberOfOperations == writerBuffer.numberOfOperations);
            SC_TEST_EXPECT(not(topLevel != topLevelRead));
        }
        if (test_section("VectorStructSimple"))
        {
            VectorStructSimple topLevel;
            SC_TRUST_RESULT(topLevel.vectorOfInts.push_back(1));
            SC_TRUST_RESULT(topLevel.vectorOfInts.push_back(2));
            SC_TRUST_RESULT(topLevel.vectorOfInts.push_back(3));
            SC_TRUST_RESULT(topLevel.vectorOfInts.push_back(4));
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(topLevel, writerBuffer));
            SC_TEST_EXPECT(writerBuffer.numberOfOperations == 4);
            SerializerReaderBuffer reader;
            reader.buffer = move(writerBuffer.buffer);
            VectorStructSimple topLevelRead;
            SerializerReader   serializerReader;
            SC_TEST_EXPECT(serializerReader.serialize(topLevelRead, reader));
            SC_TEST_EXPECT(reader.numberOfOperations == writerBuffer.numberOfOperations);
            SC_TEST_EXPECT(topLevelRead.emptyVector.size() == 0);
            SC_TEST_EXPECT(topLevelRead.vectorOfInts.size() == 4);
            for (size_t idx = 0; idx < topLevel.vectorOfInts.size(); ++idx)
            {
                SC_TEST_EXPECT(topLevel.vectorOfInts[idx] == topLevelRead.vectorOfInts[idx]);
            }
        }
        if (test_section("VectorStructComplex"))
        {
            VectorStructComplex topLevel;
            SC_TRUST_RESULT(topLevel.vectorOfStrings.push_back("asdasdasd1"));
            SC_TRUST_RESULT(topLevel.vectorOfStrings.push_back("asdasdasd2"));
            SC_TRUST_RESULT(topLevel.vectorOfStrings.push_back("asdasdasd3"));
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(topLevel, writerBuffer));
            SC_TEST_EXPECT(writerBuffer.numberOfOperations == 10);
            SerializerReaderBuffer reader;
            reader.buffer = move(writerBuffer.buffer);
            VectorStructComplex topLevelRead;
            SerializerReader    serializerReader;
            SC_TEST_EXPECT(serializerReader.serialize(topLevelRead, reader));
            SC_TEST_EXPECT(reader.numberOfOperations == writerBuffer.numberOfOperations);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings.size() == 3);
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[0] == "asdasdasd1");
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[1] == "asdasdasd2");
            SC_TEST_EXPECT(topLevelRead.vectorOfStrings[2] == "asdasdasd3");
        }
    }

    template <typename SchemaCompiler, typename SerializerWriter, typename SerializerVersioned, typename VersionSchema>
    void runVersionedTests()
    {
        if (test_section("VersionedStruct1/2"))
        {
            VersionedStruct1       struct1;
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(struct1, writerBuffer));
            SerializerVersioned    reader;
            VersionedStruct2       struct2;
            constexpr auto         schema = SchemaCompiler::template compile<VersionedStruct1>();
            SerializerReaderBuffer readerBuffer;
            readerBuffer.buffer = move(writerBuffer.buffer);
            VersionSchema versionSchema;
            versionSchema.sourceTypes = {schema.typeInfos.values, schema.typeInfos.size};
            SC_TEST_EXPECT(reader.readVersioned(struct2, readerBuffer, versionSchema));
            SC_TEST_EXPECT(readerBuffer.readPosition == readerBuffer.buffer.size());
            SC_TEST_EXPECT(not(struct2 != struct1));
        }
        if (test_section("VersionedArray1/2"))
        {
            VersionedArray1 array1;
            SC_TRUST_RESULT(array1.points.push_back({1.0f, 2.0f}));
            SC_TRUST_RESULT(array1.points.push_back({3.0f, 4.0f}));
            SC_TRUST_RESULT(array1.points.push_back({5.0f, 6.0f}));
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(array1, writerBuffer));
            SC_TEST_EXPECT(writerBuffer.numberOfOperations == 4);
            SerializerVersioned    reader;
            VersionedArray2        array2;
            constexpr auto         schema = SchemaCompiler::template compile<VersionedArray1>();
            SerializerReaderBuffer readerBuffer;
            readerBuffer.buffer = move(writerBuffer.buffer);
            VersionSchema versionSchema;
            versionSchema.sourceTypes = {schema.typeInfos.values, schema.typeInfos.size};
            SC_TEST_EXPECT(reader.readVersioned(array2, readerBuffer, versionSchema));
            SC_TEST_EXPECT(readerBuffer.readPosition == readerBuffer.buffer.size());
            SC_TEST_EXPECT(array2.points.size() == 2);
            SC_TEST_EXPECT(array1.simpleInts.size() == 3); // It's dropping one element
            SC_TEST_EXPECT(array2.simpleInts.size() == 2); // It's dropping one element
            SC_TEST_EXPECT(not(array2 != array1));
        }
        if (test_section("ConversionStruct1/2"))
        {
            ConversionStruct1      struct1;
            ConversionStruct2      struct2;
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(struct1, writerBuffer));
            SerializerVersioned    reader;
            constexpr auto         schema = SchemaCompiler::template compile<ConversionStruct1>();
            SerializerReaderBuffer readerBuffer;
            readerBuffer.buffer = move(writerBuffer.buffer);
            VersionSchema versionSchema;
            versionSchema.sourceTypes = {schema.typeInfos.values, schema.typeInfos.size};
            SC_TEST_EXPECT(reader.readVersioned(struct2, readerBuffer, versionSchema));
            SC_TEST_EXPECT(readerBuffer.readPosition == readerBuffer.buffer.size());
            SC_TEST_EXPECT(struct2.intToFloat == struct1.intToFloat);
            SC_TEST_EXPECT(struct2.floatToInt == struct1.floatToInt);
            SC_TEST_EXPECT(struct2.uint16To32 == struct1.uint16To32);
            SC_TEST_EXPECT(struct2.signed16ToUnsigned == struct1.signed16ToUnsigned);
        }
        if (test_section("Packed Struct"))
        {
            PackedStruct1 struct1;
            struct1.field0 = 0;
            struct1.field1 = 1;
            struct1.field2 = 2;
            PackedStruct2          struct2;
            SerializerWriterBuffer writerBuffer;
            SerializerWriter       serializerWriter;
            SC_TEST_EXPECT(serializerWriter.serialize(struct1, writerBuffer));
            SC_TEST_EXPECT(writerBuffer.numberOfOperations == 1);
            SerializerVersioned    reader;
            constexpr auto         schema = SchemaCompiler::template compile<PackedStruct1>();
            SerializerReaderBuffer readerBuffer;
            readerBuffer.buffer = move(writerBuffer.buffer);
            VersionSchema versionSchema;
            versionSchema.sourceTypes = {schema.typeInfos.values, schema.typeInfos.size};
            SC_TEST_EXPECT(reader.readVersioned(struct2, readerBuffer, versionSchema));
            SC_TEST_EXPECT(readerBuffer.numberOfOperations == 2);
            SC_TEST_EXPECT(readerBuffer.readPosition == readerBuffer.buffer.size());
            SC_TEST_EXPECT(struct2.field0 == 0);
            SC_TEST_EXPECT(struct2.field2 == 2);
        }
    }
};
