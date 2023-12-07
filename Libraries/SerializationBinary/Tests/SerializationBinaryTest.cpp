// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../SerializationBinary.h"
#include "SerializationSuiteTest.h"
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
