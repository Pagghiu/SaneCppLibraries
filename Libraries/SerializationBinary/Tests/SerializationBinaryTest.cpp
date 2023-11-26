// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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
        return SC::SerializationBinary::SerializerReadWriteFast<StreamType, T>::serialize(value, stream);
    }
};

struct SerializerReadVersionedAdapter
{
    template <typename T, typename StreamType, typename VersionSchema>
    bool readVersioned(T& value, StreamType& stream, VersionSchema& versionSchema)
    {
        return SC::SerializationBinary::SerializerReadVersioned<StreamType, T>::readVersioned(value, stream,
                                                                                              versionSchema);
    }
};

struct BinaryWriterStream : public SC::Serialization::BinaryBuffer
{
    [[nodiscard]] bool serializeBytes(const void* object, size_t numBytes)
    {
        return serializeBytes(Span<const uint8_t>::reinterpret_bytes(object, numBytes));
    }
    [[nodiscard]] bool serializeBytes(Span<const uint8_t> object) { return BinaryBuffer::serializeBytes(object); }
};

struct BinaryReaderStream : public SC::Serialization::BinaryBuffer
{
    [[nodiscard]] bool serializeBytes(void* object, size_t numBytes)
    {
        return serializeBytes(Span<uint8_t>::reinterpret_bytes(object, numBytes));
    }
    [[nodiscard]] bool serializeBytes(Span<uint8_t> object) { return BinaryBuffer::serializeBytes(object); }
};
} // namespace SC

namespace SC
{
struct SerializationBinaryTest;
}
struct SC::SerializationBinaryTest : public SC::SerializationParametricTestSuite::SerializationTestBase<
                                         SC::BinaryWriterStream,                        //
                                         SC::BinaryReaderStream,                        //
                                         SC::SerializerAdapter<SC::BinaryWriterStream>, //
                                         SC::SerializerAdapter<SC::BinaryReaderStream>>
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
