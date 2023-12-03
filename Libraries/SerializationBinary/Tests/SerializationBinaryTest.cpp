// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../SerializationBinaryBuffer.h"
#include "../SerializationBinaryReadVersioned.h"
#include "../SerializationBinaryReadWriteFast.h"
#include "SerializationSuiteTest.h"
namespace SC
{
struct SerializationBinaryTest;
} // namespace SC

namespace SC
{
struct SerializationBinaryTest;
}
struct SC::SerializationBinaryTest
    : public SC::SerializationSuiteTest::TestTemplate<SC::SerializationBinary::BufferWriter,
                                                      SC::SerializationBinary::BufferReader>
{
    SerializationBinaryTest(SC::TestReport& report) : TestTemplate(report, "SerializationBinaryTest")
    {
        runSameVersionTests<SerializationBinary::ReadWriteFast, SerializationBinary::ReadWriteFast>();

        runVersionedTests<SC::Reflection::Schema, SerializationBinary::ReadWriteFast,
                          SerializationBinary::ReadVersioned, SerializationBinary::VersionSchema>();
    }
};

namespace SC
{
void runSerializationBinaryTest(SC::TestReport& report) { SerializationBinaryTest test(report); }
} // namespace SC
