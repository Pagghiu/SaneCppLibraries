#pragma once
#include "Serialization.h"
#include "SerializationTestSuite.h"
#include "Test.h"

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::PrimitiveStruct)
SC_META_STRUCT_MEMBER(0, arrayValue)
SC_META_STRUCT_MEMBER(1, floatValue)
SC_META_STRUCT_MEMBER(2, int64Value)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::NestedStruct)
SC_META_STRUCT_MEMBER(0, int16Value)
SC_META_STRUCT_MEMBER(1, structsArray)
SC_META_STRUCT_MEMBER(2, doubleVal)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::TopLevelStruct)
SC_META_STRUCT_MEMBER(0, nestedStruct)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::VectorStructSimple)
SC_META_STRUCT_MEMBER(0, emptyVector)
SC_META_STRUCT_MEMBER(1, vectorOfInts)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::VectorStructComplex)
SC_META_STRUCT_MEMBER(0, vectorOfStrings)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedStruct1)
SC_META_STRUCT_MEMBER(0, floatValue)
SC_META_STRUCT_MEMBER(1, fieldToRemove)
SC_META_STRUCT_MEMBER(2, field2ToRemove)
SC_META_STRUCT_MEMBER(3, int64Value)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedStruct2)
SC_META_STRUCT_MEMBER(3, int64Value)
SC_META_STRUCT_MEMBER(0, floatValue)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedPoint3D)
SC_META_STRUCT_MEMBER(0, x)
SC_META_STRUCT_MEMBER(1, y)
SC_META_STRUCT_MEMBER(2, z)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedPoint2D)
SC_META_STRUCT_MEMBER(0, x)
SC_META_STRUCT_MEMBER(1, y)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedArray1)
SC_META_STRUCT_MEMBER(0, points)
SC_META_STRUCT_MEMBER(1, simpleInts)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::VersionedArray2)
SC_META_STRUCT_MEMBER(0, points)
SC_META_STRUCT_MEMBER(1, simpleInts)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::ConversionStruct1)
SC_META_STRUCT_MEMBER(0, intToFloat)
SC_META_STRUCT_MEMBER(1, floatToInt)
SC_META_STRUCT_MEMBER(2, uint16To32)
SC_META_STRUCT_MEMBER(3, signed16ToUnsigned)
SC_META_STRUCT_END()

SC_META_STRUCT_BEGIN(SC::SerializationTestSuite::ConversionStruct2)
SC_META_STRUCT_MEMBER(0, intToFloat)
SC_META_STRUCT_MEMBER(1, floatToInt)
SC_META_STRUCT_MEMBER(2, uint16To32)
SC_META_STRUCT_MEMBER(3, signed16ToUnsigned)
SC_META_STRUCT_END()

namespace SC
{
struct SerializationTest;
}
struct SC::SerializationTest
    : public SC::SerializationTestSuite::SerializationTestBase<SC::Serialization::BinaryBuffer,                //
                                                               SC::Serialization::BinaryBuffer,                //
                                                               SC::Serialization::SimpleBinaryWriter,          //
                                                               SC::Serialization::SimpleBinaryReader,          //
                                                               SC::Serialization::SimpleBinaryReaderVersioned, //
                                                               SC::Reflection::FlatSchemaCompiler>
{
    SerializationTest(SC::TestReport& report) : SerializationTestBase(report, "SerializationTest")
    {
        runSameVersionTests();
        runVersionedTests();
    }
};
