// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SerializationBinaryTemplateReadVersioned.h"
#include "SerializationBinaryTemplateReadWriteFast.h"
#include "SerializationBinaryTestSuite.h"

namespace SC
{
struct SerializationBinaryTemplateTest;
} // namespace SC

namespace SC
{
template <typename StreamType>
struct SerializerAdapter
{
    StreamType& stream;
    SerializerAdapter(StreamType& stream) : stream(stream) {}
    template <typename T>
    bool serialize(T& value)
    {
        return SC::SerializationBinaryTemplate::SerializerReadWriteFast<StreamType, T>::serialize(value, stream);
    }
};

struct SerializerReadVersionedAdapter
{
    template <typename T, typename StreamType, typename VersionSchema>
    bool readVersioned(T& value, StreamType& stream, VersionSchema& versionSchema)
    {
        return SC::SerializationBinaryTemplate::SerializerReadVersioned<StreamType, T>::readVersioned(value, stream,
                                                                                                      versionSchema);
    }
};

struct BinaryWriterStream : public SC::Serialization::BinaryBuffer
{
    [[nodiscard]] bool serialize(SpanVoid<const void> object) { return BinaryBuffer::serialize(object); }
};

struct BinaryReaderStream : public SC::Serialization::BinaryBuffer
{
    [[nodiscard]] bool serialize(SpanVoid<void> object) { return BinaryBuffer::serialize(object); }
};
} // namespace SC

namespace SC
{
struct SerializationBinaryTemplateTest;
}
struct SC::SerializationBinaryTemplateTest
    : public SC::SerializationBinaryTestSuite::SerializationTestBase<SC::BinaryWriterStream,                        //
                                                                     SC::BinaryReaderStream,                        //
                                                                     SC::SerializerAdapter<SC::BinaryWriterStream>, //
                                                                     SC::SerializerAdapter<SC::BinaryReaderStream>>
{
    SerializationBinaryTemplateTest(SC::TestReport& report)
        : SerializationTestBase(report, "SerializationBinaryTemplateTest")
    {
        runSameVersionTests();

        runVersionedTests<SC::Reflection::FlatSchemaTemplated, SerializerReadVersionedAdapter,
                          SC::SerializationBinaryTemplate::VersionSchema>();
    }
};
