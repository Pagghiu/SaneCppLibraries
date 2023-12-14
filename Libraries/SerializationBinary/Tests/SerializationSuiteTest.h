// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Containers/Array.h"
#include "../../Containers/SmallVector.h"
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
struct SerializationTest;

} // namespace SerializationSuiteTest
} // namespace SC

//! [serializationBinaryExactSnippet1]

//! [serializationBinaryWriteSnippet1]
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
//! [serializationBinaryWriteSnippet1]

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
//! [serializationBinaryExactSnippet1]

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

//! [serializationBinaryVersionedSnippet1]
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

//! [serializationBinaryVersionedSnippet1]

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

struct SC::SerializationSuiteTest::SerializationTest : public SC::TestCase
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

    SerializationTest(SC::TestReport& report, StringView name) : TestCase(report, name) {}

    template <typename SerializerWriter, typename SerializerReader>
    void runSameVersionTests()
    {
        size_t numWriteOperations = 0;
        size_t numReadOperations  = 0;
        if (test_section("Primitive Structure Write"))
        {
            PrimitiveStruct objectToSerialize;

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));

            // Verification
            SC_TEST_EXPECT(numWriteOperations == 1);
            uint32_t index = 0;
            for (uint32_t i = 0; i < 4; ++i)
            {
                SC_TEST_EXPECT(readPrimitive<uint8_t>(buffer, index) == objectToSerialize.arrayValue[i]);
            }
            SC_TEST_EXPECT(readPrimitive<float>(buffer, index) == objectToSerialize.floatValue);
            SC_TEST_EXPECT(readPrimitive<int64_t>(buffer, index) == objectToSerialize.int64Value);
        }
        if (test_section("PrimitiveStruct"))
        {
            PrimitiveStruct objectToSerialize;
            PrimitiveStruct deserializedObject;
            memset(&deserializedObject, 0, sizeof(deserializedObject));

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));
            SC_TEST_EXPECT(numWriteOperations == 1);

            // Deserialization
            SC_TEST_EXPECT(SerializerReader::loadExact(deserializedObject, buffer.toSpanConst(), &numReadOperations));
            SC_TEST_EXPECT(numReadOperations == numWriteOperations);

            // Verification
            SC_TEST_EXPECT(not(objectToSerialize != deserializedObject));
        }
        if (test_section("TopLevelStruct"))
        {
            TopLevelStruct objectToSerialize;
            TopLevelStruct deserializedObject;
            memset(&deserializedObject, 0, sizeof(deserializedObject));

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));
            SC_TEST_EXPECT(numWriteOperations == 3);

            // Deserialization
            SC_TEST_EXPECT(SerializerReader::loadExact(deserializedObject, buffer.toSpanConst(), &numReadOperations));
            SC_TEST_EXPECT(numReadOperations == numWriteOperations);

            // Verification
            SC_TEST_EXPECT(not(objectToSerialize != deserializedObject));
        }
        if (test_section("VectorStructSimple"))
        {
            VectorStructSimple objectToSerialize;
            SC_TRUST_RESULT(objectToSerialize.vectorOfInts.push_back(1));
            SC_TRUST_RESULT(objectToSerialize.vectorOfInts.push_back(2));
            SC_TRUST_RESULT(objectToSerialize.vectorOfInts.push_back(3));
            SC_TRUST_RESULT(objectToSerialize.vectorOfInts.push_back(4));
            VectorStructSimple deserializedObject;

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));
            SC_TEST_EXPECT(numWriteOperations == 4);

            // Deserialization
            SC_TEST_EXPECT(SerializerReader::loadExact(deserializedObject, buffer.toSpanConst(), &numReadOperations));
            SC_TEST_EXPECT(numReadOperations == numWriteOperations);

            // Verification
            SC_TEST_EXPECT(deserializedObject.emptyVector.size() == 0);
            SC_TEST_EXPECT(deserializedObject.vectorOfInts.size() == 4);
            for (size_t idx = 0; idx < objectToSerialize.vectorOfInts.size(); ++idx)
            {
                SC_TEST_EXPECT(objectToSerialize.vectorOfInts[idx] == deserializedObject.vectorOfInts[idx]);
            }
        }
        if (test_section("VectorStructComplex"))
        {
            VectorStructComplex objectToSerialize;
            SC_TRUST_RESULT(objectToSerialize.vectorOfStrings.push_back("asdasdasd1"));
            SC_TRUST_RESULT(objectToSerialize.vectorOfStrings.push_back("asdasdasd2"));
            SC_TRUST_RESULT(objectToSerialize.vectorOfStrings.push_back("asdasdasd3"));
            VectorStructComplex deserializedObject;

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));
            SC_TEST_EXPECT(numWriteOperations == 10);

            // Deserialization
            SC_TEST_EXPECT(SerializerReader::loadExact(deserializedObject, buffer.toSpanConst(), &numReadOperations));
            SC_TEST_EXPECT(numReadOperations == numWriteOperations);

            // Verification
            SC_TEST_EXPECT(deserializedObject.vectorOfStrings.size() == 3);
            SC_TEST_EXPECT(deserializedObject.vectorOfStrings[0] == "asdasdasd1");
            SC_TEST_EXPECT(deserializedObject.vectorOfStrings[1] == "asdasdasd2");
            SC_TEST_EXPECT(deserializedObject.vectorOfStrings[2] == "asdasdasd3");
        }
    }

    template <typename SerializerWriter, typename SerializerReader, typename SerializerSchemaCompiler>
    void runVersionedTests()
    {
        size_t numReadOperations  = 0;
        size_t numWriteOperations = 0;
        if (test_section("VersionedStruct1/2"))
        {
            constexpr auto   schema = SerializerSchemaCompiler::template compile<VersionedStruct1>();
            VersionedStruct1 objectToSerialize;
            VersionedStruct2 deserializedObject;

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));

            // Deserialization
            SC_TEST_EXPECT(SerializerReader::loadVersioned(deserializedObject, buffer.toSpanConst(), schema.typeInfos,
                                                           SerializationBinaryOptions(), &numReadOperations));

            // Verification
            SC_TEST_EXPECT(not(deserializedObject != objectToSerialize));
        }
        if (test_section("VersionedArray1/2"))
        {
            constexpr auto  schema = SerializerSchemaCompiler::template compile<VersionedArray1>();
            VersionedArray1 objectToSerialize;
            VersionedArray2 deserializedObject;
            SC_TRUST_RESULT(objectToSerialize.points.push_back({1.0f, 2.0f}));
            SC_TRUST_RESULT(objectToSerialize.points.push_back({3.0f, 4.0f}));
            SC_TRUST_RESULT(objectToSerialize.points.push_back({5.0f, 6.0f}));

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));
            SC_TEST_EXPECT(numWriteOperations == 4);

            // Deserialization
            SC_TEST_EXPECT(SerializerReader::loadVersioned(deserializedObject, buffer.toSpanConst(), schema.typeInfos,
                                                           SerializationBinaryOptions(), &numReadOperations));

            // Verification
            SC_TEST_EXPECT(deserializedObject.points.size() == 2);
            SC_TEST_EXPECT(objectToSerialize.simpleInts.size() == 3);  // Dropping one element
            SC_TEST_EXPECT(deserializedObject.simpleInts.size() == 2); // Dropping one element
            SC_TEST_EXPECT(not(deserializedObject != objectToSerialize));
        }
        if (test_section("ConversionStruct1/2"))
        {
            constexpr auto    schema = SerializerSchemaCompiler::template compile<ConversionStruct1>();
            ConversionStruct1 objectToSerialize;
            ConversionStruct2 deserializedObject;

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));

            // Deserialization
            SC_TEST_EXPECT(SerializerReader::loadVersioned(deserializedObject, buffer.toSpanConst(), schema.typeInfos,
                                                           SerializationBinaryOptions(), &numReadOperations));

            // Verification
            SC_TEST_EXPECT(deserializedObject.intToFloat == objectToSerialize.intToFloat);
            SC_TEST_EXPECT(deserializedObject.floatToInt == objectToSerialize.floatToInt);
            SC_TEST_EXPECT(deserializedObject.uint16To32 == objectToSerialize.uint16To32);
            SC_TEST_EXPECT(deserializedObject.signed16ToUnsigned == objectToSerialize.signed16ToUnsigned);
        }
        if (test_section("PackedStruct"))
        {
            constexpr auto schema = SerializerSchemaCompiler::template compile<PackedStruct1>();
            PackedStruct1  objectToSerialize;
            PackedStruct2  deserializedObject;
            objectToSerialize.field0 = 0;
            objectToSerialize.field1 = 1;
            objectToSerialize.field2 = 2;

            // Serialization
            SmallVector<uint8_t, 256> buffer;
            SC_TEST_EXPECT(SerializerWriter::write(objectToSerialize, buffer, &numWriteOperations));
            SC_TEST_EXPECT(numWriteOperations == 1);

            // Deserialization
            SC_TEST_EXPECT(SerializerReader::loadVersioned(deserializedObject, buffer.toSpanConst(), schema.typeInfos,
                                                           SerializationBinaryOptions(), &numReadOperations));
            SC_TEST_EXPECT(numReadOperations == 2);

            // Verification
            SC_TEST_EXPECT(deserializedObject.field0 == 0);
            SC_TEST_EXPECT(deserializedObject.field2 == 2);
        }
    }
};
