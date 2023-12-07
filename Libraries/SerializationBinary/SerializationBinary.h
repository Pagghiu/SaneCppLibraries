// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Internal/SerializationBinaryBuffer.h"
#include "Internal/SerializationBinaryReadVersioned.h"
#include "Internal/SerializationBinaryReadWriteExact.h"

namespace SC
{
//! @defgroup group_serialization_binary Serialization Binary
//! @copybrief library_serialization_binary (see @ref library_serialization_binary for more details)

//! @addtogroup group_serialization_binary
//! @{

/// @brief Loads or writes binary data with its associated reflection schema from or into a C++ object
struct SerializationBinary
{
    /// @brief Writes object `T` to buffer
    /// @tparam T Type of object to be serialized (must be described by Reflection)
    /// @param value The object to be serialized
    /// @param buffer The buffer that will receive serialized bytes
    /// @param numberOfWrites If provided, will return the number of serialization operations
    /// @return `true` if serialization succeeded
    template <typename T>
    [[nodiscard]] static bool write(T& value, Vector<uint8_t>& buffer, size_t* numberOfWrites = nullptr)
    {
        SerializationBinaryBufferWriter writer(buffer);
        using Writer = detail::SerializerBinaryReadWriteExact<SerializationBinaryBufferWriter, T>;
        if (not Writer::serialize(value, writer))
            return false;
        if (numberOfWrites)
            *numberOfWrites = writer.numberOfOperations;
        return true;
    }

    /// @brief Loads object `T` from buffer without trying to compsensate for any schema change.
    /// @tparam T Type of object to be deserialized (must be described by Reflection)
    /// @param value The object to be deserialized
    /// @param buffer The buffer holding actual bytes for deserialization
    /// @param numberOfReads If provided, will return the number deserialization operations
    /// @return `true` if deserialization succeeded
    template <typename T>
    [[nodiscard]] static bool loadExact(T& value, Span<const uint8_t> buffer, size_t* numberOfReads = nullptr)
    {
        SerializationBinaryBufferReader bufferReader(buffer);
        using Reader = detail::SerializerBinaryReadWriteExact<SerializationBinaryBufferReader, T>;
        if (not Reader::serialize(value, bufferReader))
            return false;
        if (numberOfReads)
            *numberOfReads = bufferReader.numberOfOperations;
        return bufferReader.positionIsAtEnd();
    }

    /// @brief Deserialize object `T` from a Binary buffer with a reflection schema not matching `T` schema
    /// @tparam T Type of object to be deserialized (must be described by Reflection)
    /// @param value The object to deserialize
    /// @param buffer The buffer holding the bytes to be used for deserialization
    /// @param schema The schema used to serialize data in the buffer
    /// @param numberOfReads If provided, will return the number deserialization operations
    /// @return `true` if deserialization succeded
    template <typename T>
    [[nodiscard]] static bool loadVersioned(T& value, Span<const uint8_t> buffer,
                                            Span<const Reflection::TypeInfo> schema, size_t* numberOfReads = nullptr)
    {
        SerializationBinaryBufferReader readerBuffer(buffer);
        using Reader = detail::SerializerBinaryReadVersioned<SerializationBinaryBufferReader, T>;
        SerializationSchema versionSchema(schema);
        if (not Reader::readVersioned(value, readerBuffer, versionSchema))
            return false;
        if (numberOfReads)
            *numberOfReads = readerBuffer.numberOfOperations;
        return readerBuffer.positionIsAtEnd();
    }
};

//! @}

} // namespace SC
