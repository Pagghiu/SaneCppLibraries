// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "SerializationBinaryTestSuite.h"
#include "SerializationBinaryTypeErasedReadVersioned.h"
#include "SerializationBinaryTypeErasedReadWriteFast.h"

namespace SC
{
struct SerializationBinaryTypeErasedTest;

}
struct SC::SerializationBinaryTypeErasedTest : public SC::SerializationBinaryTestSuite::SerializationTestBase<
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
