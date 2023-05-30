// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "SerializationBinaryTestSuite.h"
#include "SerializationBinaryTypeErased.h"

namespace SC
{
struct SerializationBinaryTypeErasedTest;

}
struct SC::SerializationBinaryTypeErasedTest
    : public SC::SerializationBinaryTestSuite::SerializationTestBase<SC::SerializationBinaryTypeErased::BinaryBuffer,       //
                                                               SC::SerializationBinaryTypeErased::BinaryBuffer,       //
                                                               SC::SerializationBinaryTypeErased::SimpleBinaryWriter, //
                                                               SC::SerializationBinaryTypeErased::SimpleBinaryReader>
{
    SerializationBinaryTypeErasedTest(SC::TestReport& report) : SerializationTestBase(report, "SerializationBinaryTypeErasedTest")
    {
        runSameVersionTests();
        runVersionedTests<
            SC::Reflection::FlatSchemaTypeErased,
            SC::SerializationBinaryTypeErased::SimpleBinaryReaderVersioned<SC::SerializationBinaryTypeErased::BinaryBuffer>,
            SC::SerializationBinaryTypeErased::VersionSchema>();
    }
};
