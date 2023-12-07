// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../SerializationBinaryTypeErased.h"
#include "../../../Libraries/SerializationBinary/Tests/SerializationSuiteTest.h"
#include "../../../Libraries/Testing/Testing.h"

namespace SC
{
struct SerializationBinaryTypeErasedTest;

}
struct SC::SerializationBinaryTypeErasedTest : public SC::SerializationSuiteTest::SerializationTest
{
    SerializationBinaryTypeErasedTest(SC::TestReport& report)
        : SerializationTest(report, "SerializationBinaryTypeErasedTest")
    {
        runSameVersionTests<SerializationBinaryTypeErased, SerializationBinaryTypeErased>();
        runVersionedTests<SerializationBinaryTypeErased, SerializationBinaryTypeErased, Reflection::SchemaTypeErased>();
    }
};

namespace SC
{
void runSerializationBinaryTypeErasedTest(SC::TestReport& report) { SerializationBinaryTypeErasedTest test(report); }
} // namespace SC
