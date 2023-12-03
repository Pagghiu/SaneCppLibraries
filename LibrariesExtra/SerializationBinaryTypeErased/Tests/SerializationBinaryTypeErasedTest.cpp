// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../../Libraries/SerializationBinary/Tests/SerializationSuiteTest.h"
#include "../../../Libraries/Testing/Testing.h"
#include "../SerializationBinaryTypeErasedReadVersioned.h"
#include "../SerializationBinaryTypeErasedReadWriteFast.h"

namespace SC
{
struct SerializationBinaryTypeErasedTest;

}
struct SC::SerializationBinaryTypeErasedTest
    : public SC::SerializationSuiteTest::TestTemplate<SC::SerializationBinary::Buffer, SC::SerializationBinary::Buffer>
{
    SerializationBinaryTypeErasedTest(SC::TestReport& report)
        : TestTemplate(report, "SerializationBinaryTypeErasedTest")
    {
        runSameVersionTests<SerializationBinaryTypeErased::WriteFast, SerializationBinaryTypeErased::ReadFast>();
        runVersionedTests<Reflection::SchemaTypeErased, SerializationBinaryTypeErased::WriteFast,
                          SerializationBinaryTypeErased::ReadVersioned, SerializationBinaryTypeErased::VersionSchema>();
    }
};

namespace SC
{
void runSerializationBinaryTypeErasedTest(SC::TestReport& report) { SerializationBinaryTypeErasedTest test(report); }
} // namespace SC
