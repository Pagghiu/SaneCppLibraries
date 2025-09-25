// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/SerializationBinary/SerializationBinary.h"
#include "Libraries/ContainersSerialization/ContainersSerialization.h"
#include "Libraries/ContainersSerialization/MemorySerialization.h"
#include "Tests/Libraries/SerializationBinary/SerializationSuiteTest.h"

namespace SC
{
struct SerializationBinaryTest;
} // namespace SC

struct SC::SerializationBinaryTest : public SC::SerializationSuiteTest::SerializationTest
{
    SerializationBinaryTest(SC::TestReport& report) : SerializationTest(report, "SerializationBinaryTest")
    {
        runSameVersionTests<SerializationBinary, SerializationBinary>();
        runVersionedTests<SerializationBinary, SerializationBinary, Reflection::Schema>();
    }
};

namespace SC
{
void runSerializationBinaryTest(SC::TestReport& report) { SerializationBinaryTest test(report); }
} // namespace SC
