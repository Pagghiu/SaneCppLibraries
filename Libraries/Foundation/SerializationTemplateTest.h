// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SerializationBinarySkipper.h" // This can be included in cpp explicitly templatized only with the BinaryReader
#include "SerializationTemplate.h"
#include "SerializationTestSuite.h"

namespace SC
{
struct SerializationTemplateTest;
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
        return SC::SerializationTemplate::Serializer<StreamType, T>::serialize(value, stream);
    }
};

struct SerializerVersionedAdapter
{
    template <typename T, typename StreamType, typename VersionSchema>
    bool serializeVersioned(T& value, StreamType& stream, VersionSchema& versionSchema)
    {
        return SC::SerializationTemplate::Serializer<StreamType, T>::serializeVersioned(value, stream, versionSchema);
    }
};

struct BinaryWriterStream
{
    SC::Vector<uint8_t> buffer;
    int                 numberOfOperations = 0;

    [[nodiscard]] bool serialize(SpanVoid<const void> object)
    {
        numberOfOperations++;
        Span<const uint8_t> bytes = object.castTo<const uint8_t>();
        return buffer.appendCopy(bytes.data(), bytes.sizeInBytes());
    }
};

struct BinaryReaderStream
{
    size_t              index = 0;
    SC::Vector<uint8_t> buffer;
    int                 numberOfOperations = 0;

    [[nodiscard]] bool serialize(SpanVoid<void> object)
    {
        if (index + object.sizeInBytes() > buffer.size())
            return false;
        numberOfOperations++;
        Span<uint8_t> bytes = object.castTo<uint8_t>();
        memcpy(bytes.data(), &buffer[index], bytes.sizeInBytes());
        index += bytes.sizeInBytes();
        return true;
    }

    [[nodiscard]] bool advance(size_t numBytes)
    {
        if (index + numBytes > buffer.size())
            return false;
        index += numBytes;
        return true;
    }
};
} // namespace SC

namespace SC
{
struct SerializationTemplateTest;
}
struct SC::SerializationTemplateTest
    : public SC::SerializationTestSuite::SerializationTestBase<SC::BinaryWriterStream,                        //
                                                               SC::BinaryReaderStream,                        //
                                                               SC::SerializerAdapter<SC::BinaryWriterStream>, //
                                                               SC::SerializerAdapter<SC::BinaryReaderStream>>
{
    SerializationTemplateTest(SC::TestReport& report) : SerializationTestBase(report, "SerializationTemplateTest")
    {
        runSameVersionTests();

        runVersionedTests<SC::Reflection::FlatSchemaTemplated, SerializerVersionedAdapter,
                          SC::SerializationTemplate::VersionSchema>();
    }
};
