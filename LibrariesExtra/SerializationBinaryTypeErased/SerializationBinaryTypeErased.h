// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Internal/SerializationBinaryTypeErasedReadVersioned.h"
#include "Internal/SerializationBinaryTypeErasedReadWriteExact.h"
namespace SC
{

//! @defgroup group_serialization_type_erased Serialization Binary Type Erased
//! @copybrief library_serialization_binary_type_erased (see @ref library_serialization_binary_type_erased for more
//! details)

//! @addtogroup group_serialization_type_erased
//! @{

/// @brief Loads or writes binary data with its associated reflection schema from or into a C++ object
struct SerializationBinaryTypeErased
{
    /// @brief Writes object `T` to a buffer
    /// @tparam T Type of object to be serialized
    /// @param object The object to be serialized
    /// @param buffer The buffer receiving serialized bytes
    /// @param numberOfWrites If provided, will return the number of serialization operations
    /// @return `true` if the operation succeeded
    template <typename T>
    [[nodiscard]] static bool write(const T& object, Vector<uint8_t>& buffer, size_t* numberOfWrites = nullptr)
    {
        SerializationBinaryBufferWriter         binaryBuffer(buffer);
        SerializationBinaryTypeErasedWriteExact writer;
        if (not writer.write(object, binaryBuffer))
            return false;
        if (numberOfWrites)
            *numberOfWrites = binaryBuffer.numberOfOperations;
        return true;
    }

    /// @brief Reads object `T` from a buffer, assuming no versioning changes
    /// @tparam T Type of object to be deserialized
    /// @param object The object to be deserialized
    /// @param buffer The buffer providing bytes for deserialization
    /// @param numberOfReads If provided, will return the number deserialization operations
    /// @return `true` if the operation succeeded
    template <typename T>
    [[nodiscard]] static bool loadExact(T& object, Span<const uint8_t> buffer, size_t* numberOfReads = nullptr)
    {
        SerializationBinaryBufferReader        bufferReader(buffer);
        SerializationBinaryTypeErasedReadExact reader;
        if (not reader.loadExact(object, bufferReader))
            return false;
        if (numberOfReads)
            *numberOfReads = bufferReader.numberOfOperations;
        return bufferReader.positionIsAtEnd();
    }
    /// @brief Deserialize object `T` from a Binary buffer with a reflection schema not matching `T` schema
    /// @tparam T Type of object to be dserialized
    /// @param object The object to deserialize
    /// @param buffer The buffer holding the bytes to be used for deserialization
    /// @param schema The schema used to serialize data in the buffer
    /// @param numberOfReads If provided, will return the number deserialization operations
    /// @return `true` if deserialization succeded
    template <typename T>
    [[nodiscard]] static bool loadVersioned(T& object, Span<const uint8_t> buffer,
                                            Span<const Reflection::TypeInfo> schema,
                                            SerializationBinaryOptions options = {}, size_t* numberOfReads = nullptr)
    {
        SerializationBinaryTypeErasedReadVersioned loader;

        SerializationSchema serializationSchema(schema);
        serializationSchema.options = options;
        SerializationBinaryBufferReader readerBuffer(buffer);
        if (not loader.loadVersioned(object, readerBuffer, serializationSchema))
            return false;
        if (numberOfReads)
            *numberOfReads = readerBuffer.numberOfOperations;
        return readerBuffer.positionIsAtEnd();
    }
};
//! @}
} // namespace SC
