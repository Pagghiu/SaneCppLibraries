// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../SerializationBinaryBuffer.h"
#include "../SerializationBinaryReadVersioned.h"
#include "../SerializationBinaryReadWriteFast.h"
#include "SerializationParametricTestSuite.h"
namespace SC
{
struct SerializationBinaryTest;
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
        using Serializer = SC::SerializationBinary::SerializerReadWriteFast<StreamType, T>;
        return Serializer::serialize(value, stream);
    }
};

struct SerializerReadVersionedAdapter
{
    template <typename T, typename StreamType, typename VersionSchema>
    bool readVersioned(T& value, StreamType& stream, VersionSchema& versionSchema)
    {
        using VersionedSerializer = SC::SerializationBinary::SerializerReadVersioned<StreamType, T>;
        return VersionedSerializer::readVersioned(value, stream, versionSchema);
    }
};

} // namespace SC

namespace SC
{
struct SerializationBinaryTest;
}
struct SC::SerializationBinaryTest : public SC::SerializationParametricTestSuite::SerializationTestBase<
                                         SC::SerializationBinary::BinaryWriterStream,                        //
                                         SC::SerializationBinary::BinaryReaderStream,                        //
                                         SC::SerializerAdapter<SC::SerializationBinary::BinaryWriterStream>, //
                                         SC::SerializerAdapter<SC::SerializationBinary::BinaryReaderStream>>
{
    SerializationBinaryTest(SC::TestReport& report) : SerializationTestBase(report, "SerializationBinaryTest")
    {
        runSameVersionTests();

        runVersionedTests<SC::Reflection::Schema, SerializerReadVersionedAdapter,
                          SC::SerializationBinary::VersionSchema>();
    }
};

namespace SC
{
void runSerializationBinaryTest(SC::TestReport& report) { SerializationBinaryTest test(report); }
} // namespace SC
