// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Test.h"
#include "SerializationTestSuite.h"
#include "SerializationTypeErased.h"

namespace SC
{
struct SerializationTypeErasedTest;

}
struct SC::SerializationTypeErasedTest
    : public SC::SerializationTestSuite::SerializationTestBase<SC::SerializationTypeErased::BinaryBuffer,       //
                                                               SC::SerializationTypeErased::BinaryBuffer,       //
                                                               SC::SerializationTypeErased::SimpleBinaryWriter, //
                                                               SC::SerializationTypeErased::SimpleBinaryReader>
{
    SerializationTypeErasedTest(SC::TestReport& report) : SerializationTestBase(report, "SerializationTypeErasedTest")
    {
        runSameVersionTests();
        runVersionedTests<
            SC::Reflection::FlatSchemaTypeErased,
            SC::SerializationTypeErased::SimpleBinaryReaderVersioned<SC::SerializationTypeErased::BinaryBuffer>,
            SC::SerializationTypeErased::VersionSchema>();
    }
};
