// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Internal/SerializationBinaryBuffer.h"
#include "Internal/SerializationBinaryReadVersioned.h"
#include "Internal/SerializationBinaryReadWriteExact.h"

#include "SerializationBinaryOptions.h"

namespace SC
{
//! @defgroup group_serialization_binary Serialization Binary
//! @copybrief library_serialization_binary (see @ref library_serialization_binary for more details)
///
/// This is a versioned binary serializer / deserializer built on top of @ref library_reflection. @n
/// It uses struct member iterators on Reflection schema serialize all members and the recursively `Packed` property for
/// optimizations, reducing the number of read / writes (or `memcpy`) needed.

//! @addtogroup group_serialization_binary
//! @{

/// @brief Loads or writes binary data with its associated reflection schema from or into a C++ object
struct SerializationBinary
{
    /// @brief Writes object `T` to a binary buffer.
    ///
    /// SC::SerializationBinary::write is used to serialize data.
    /// The schema itself is not used at all but it could written along with the binary data
    /// so that when reading back the data in a later version of the program, the correct choice can be made between
    /// deserializing using SerializationBinary::loadVersioned (slower but allows for missing fields and conversion) or
    /// deserializing using SerializationBinary::loadExact (faster, but schema must match 1:1).
    /// @n
    /// @tparam T Type of object to be serialized (must be described by Reflection)
    /// @param value The object to be serialized
    /// @param buffer The buffer that will receive serialized bytes
    /// @param numberOfWrites If provided, will return the number of serialization operations
    /// @return `true` if serialization succeeded
    ///
    /// Assuming the following struct:
    /// \snippet Tests/Libraries/SerializationBinary/SerializationSuiteTest.h serializationBinaryWriteSnippet1

    /// `PrimitiveStruct` can be written to a binary buffer with the following code:
    /// @code{.cpp}
    /// PrimitiveStruct objectToSerialize;
    /// Vector buffer;
    /// SC_TRY(SerializerWriter::write(objectToSerialize, buffer));
    /// @endcode

    template <typename T>
    [[nodiscard]] static bool write(T& value, Buffer& buffer, size_t* numberOfWrites = nullptr)
    {
        SerializationBinaryBufferWriter writer(buffer);
        using Writer = Serialization::SerializerBinaryReadWriteExact<SerializationBinaryBufferWriter, T>;
        if (not Writer::serialize(value, writer))
            return false;
        if (numberOfWrites)
            *numberOfWrites = writer.numberOfOperations;
        return true;
    }

    /// @brief Loads object `T` from binary buffer as written by SerializationBinary::write.
    ///
    /// SC::SerializationBinary::loadExact can deserialize binary data into a struct whose schema has not changed from
    /// when SerializationBinary::write has been used to generate that same binary data.
    /// In other words if the schema of the type passed to SerializationBinary::write must match the one of current type
    /// being deserialized.
    /// If the two schemas hash match then it's possible to use this fast path, that skips all versioning checks.
    /// @tparam T Type of object to be deserialized (must be described by Reflection)
    /// @param value The object to be deserialized
    /// @param buffer The buffer holding actual bytes for deserialization
    /// @param numberOfReads If provided, will return the number deserialization operations
    /// @return `true` if deserialization succeeded
    ///
    /// Assuming the following structs:
    /// \snippet Tests/Libraries/SerializationBinary/SerializationSuiteTest.h serializationBinaryExactSnippet1
    /// `TopLevelStruct` can be serialized and de-serialized with the following code:
    /// @code{.cpp}
    ///
    /// TopLevelStruct objectToSerialize;
    /// TopLevelStruct deserializedObject;
    ///
    /// // Change a field just as a test
    /// objectToSerialize.nestedStruct.doubleVal = 44.4;
    ///
    /// // Serialization
    /// Vector buffer;
    /// SC_TRY(SerializerWriter::write(objectToSerialize, buffer));
    ///
    /// // Deserialization
    /// SC_TRY(SerializerReader::loadExact(deserializedObject, buffer.toSpanConst()));
    /// SC_ASSERT_RELEASE(objectToSerialize.nestedStruct.doubleVal == deserializedObject.nestedStruct.doubleVal);
    /// @endcode
    template <typename T>
    [[nodiscard]] static bool loadExact(T& value, Span<const char> buffer, size_t* numberOfReads = nullptr)
    {
        SerializationBinaryBufferReader bufferReader(buffer);
        using Reader = Serialization::SerializerBinaryReadWriteExact<SerializationBinaryBufferReader, T>;
        if (not Reader::serialize(value, bufferReader))
            return false;
        if (numberOfReads)
            *numberOfReads = bufferReader.numberOfOperations;
        return bufferReader.positionIsAtEnd();
    }

    /// @brief Deserialize object `T` from a Binary buffer with a reflection schema not matching `T` schema
    //
    /// The versioned read serializer SC::SerializationBinary::loadVersioned must be used when source and destination
    /// schemas do not match.
    /// _Compatibility_ flags can be customized through SC::SerializationBinaryOptions object, allowing to remap data
    /// coming from an older (or just different) version of the schema to the current one.
    /// SerializationBinary::loadVersioned will try to match the `memberTag` field specified in [Reflection](@ref
    /// library_reflection) to match fields between source and destination schemas.
    /// When the types of the fields are different, a few options allow controlling the behaviour.
    /// @tparam T Type of object to be deserialized (must be described by Reflection)
    /// @param value The object to deserialize
    /// @param buffer The buffer holding the bytes to be used for deserialization
    /// @param schema The schema used to serialize data in the buffer
    /// @param numberOfReads If provided, will return the number deserialization operations
    /// @param options Options for data conversion (allow dropping fields, array items etc)
    /// @return `true` if deserialization succeeded
    ///
    /// @n
    /// Assuming the following structs:
    /// \snippet Tests/Libraries/SerializationBinary/SerializationSuiteTest.h serializationBinaryVersionedSnippet1
    /// `VersionedStruct2` can be deserialized from `VersionedStruct1` in the following way
    /// @code{.cpp}
    ///  constexpr auto   schema = SerializerSchemaCompiler::template compile<VersionedStruct1>();
    ///  VersionedStruct1 objectToSerialize;
    ///  VersionedStruct2 deserializedObject;
    ///
    ///  // Serialization
    ///  Vector buffer;
    ///  SC_TRY(SerializerWriter::write(objectToSerialize, buffer));
    ///
    ///  // Deserialization
    ///  SC_TRY(SerializerReader::loadVersioned(deserializedObject, buffer.toSpanConst(), schema.typeInfos));
    /// @endcode
    template <typename T>
    [[nodiscard]] static bool loadVersioned(T& value, Span<const char> buffer, Span<const Reflection::TypeInfo> schema,
                                            SerializationBinaryOptions options = {}, size_t* numberOfReads = nullptr)
    {
        SerializationBinaryBufferReader readerBuffer(buffer);
        using Reader = Serialization::SerializerBinaryReadVersioned<SerializationBinaryBufferReader, T>;
        SerializationSchema versionSchema(schema);
        versionSchema.options = options;
        if (not Reader::readVersioned(value, readerBuffer, versionSchema))
            return false;
        if (numberOfReads)
            *numberOfReads = readerBuffer.numberOfOperations;
        return readerBuffer.positionIsAtEnd();
    }

    /// @brief Writes the reflection schema of object `T` followed by contents of object `T` to a binary buffer.
    /// The serialized buffer can be used with SerializationBinary::loadVersionedWithSchema to allow a "best effort"
    /// de-serialization trying to match fields with corresponding `memberTag`.
    /// @see SerializationBinary::write
    /// @see SerializationBinary::loadVersionedWithSchema
    template <typename T>
    [[nodiscard]] static bool writeWithSchema(T& value, Buffer& buffer, size_t* numberOfWrites = nullptr)
    {
        constexpr auto     typeInfos = Reflection::Schema::template compile<T>().typeInfos;
        constexpr uint32_t numInfos  = typeInfos.size;
        static_assert(alignof(Reflection::TypeInfo) == sizeof(uint32_t), "Alignof TypeInfo");
        // Implying same endianness when reading here
        SC_TRY(buffer.append(Span<const char>::reinterpret_bytes(&numInfos, sizeof(numInfos))));
        SC_TRY(buffer.append(
            Span<const char>::reinterpret_bytes(typeInfos.values, typeInfos.size * sizeof(Reflection::TypeInfo))));
        return write(value, buffer, numberOfWrites);
    }

    /// @brief Loads object `T` using the schema information that has been prepended by
    /// SerializationBinary::writeWithSchema. The schema allows a "best effort" de-serialization trying to match fields
    /// with corresponding `memberTag`.
    /// @see SerializationBinary::loadVersioned
    /// @see SerializationBinary::writeWithSchema
    template <typename T>
    [[nodiscard]] static bool loadVersionedWithSchema(T& value, Span<const char> buffer,
                                                      SerializationBinaryOptions options       = {},
                                                      size_t*                    numberOfReads = nullptr)
    {
        uint32_t numInfos = 0;
        // Read number of type info
        Span<const char> numInfosSlice;
        SC_TRY(buffer.sliceStartLength(0, sizeof(numInfos), numInfosSlice));
        memcpy(&numInfos, numInfosSlice.data(), sizeof(numInfos));

        // Cast first part of the buffer to a Span of TypeInfo.
        // It's possible as we've been serializing it with correct alignment (currently 32 bits).
        static_assert(alignof(Reflection::TypeInfo) == sizeof(uint32_t), "Alignof TypeInfo");
        Span<const char> typeInfos;
        SC_TRY(buffer.sliceStartLength(sizeof(numInfos), numInfos * sizeof(Reflection::TypeInfo), typeInfos));
        Span<const Reflection::TypeInfo> serializedSchema =
            typeInfos.reinterpret_as_span_of<const Reflection::TypeInfo>();

        constexpr auto sourceSchema = Reflection::Schema::template compile<T>().typeInfos;

        // Get the slice of bytes where actual serialization data has been written by writeWithSchema
        Span<const char> serializedDataSlice;
        SC_TRY(buffer.sliceStart(sizeof(numInfos) + numInfos * sizeof(Reflection::TypeInfo), serializedDataSlice));
        if (sourceSchema.equals(serializedSchema))
        {
            // If the serialized schema matches current object schema, we can run the fast "loadExact" path
            return loadExact(value, serializedDataSlice);
        }
        else
        {
            // The two schemas differs, so resort to the "slower" versioned loader that tries to match field order
            return loadVersioned(value, serializedDataSlice, serializedSchema, options, numberOfReads);
        }
    }
};

//! @}

} // namespace SC
