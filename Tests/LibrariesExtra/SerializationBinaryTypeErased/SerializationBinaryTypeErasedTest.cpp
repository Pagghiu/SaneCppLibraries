// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "LibrariesExtra/SerializationBinaryTypeErased/SerializationBinaryTypeErased.h"
#include "Libraries/Testing/Testing.h"
#include "Tests/Libraries/SerializationBinary/SerializationSuiteTest.h"

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
