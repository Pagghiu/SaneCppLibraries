// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../../Libraries/SerializationBinary/Tests/SerializationParametricTestSuite.h"
#include "../../../Libraries/Testing/Testing.h"
#include "../SerializationBinaryTypeErasedReadVersioned.h"
#include "../SerializationBinaryTypeErasedReadWriteFast.h"

namespace SC
{
struct SerializationBinaryTypeErasedTest;

}
struct SC::SerializationBinaryTypeErasedTest : public SC::SerializationParametricTestSuite::SerializationTestBase<
                                                   SC::Serialization::BinaryBuffer,                            //
                                                   SC::Serialization::BinaryBuffer,                            //
                                                   SC::SerializationBinaryTypeErased::SerializerReadWriteFast, //
                                                   SC::SerializationBinaryTypeErased::SimpleBinaryReader>
{
    SerializationBinaryTypeErasedTest(SC::TestReport& report)
        : SerializationTestBase(report, "SerializationBinaryTypeErasedTest")
    {
        runSameVersionTests();
        runVersionedTests<SC::Reflection::FlatSchemaTypeErased,
                          SC::SerializationBinaryTypeErased::SerializerReadVersioned,
                          SC::SerializationBinaryTypeErased::VersionSchema>();
    }
};

namespace SC
{
void runSerializationBinaryTypeErasedTest(SC::TestReport& report) { SerializationBinaryTypeErasedTest test(report); }
} // namespace SC
