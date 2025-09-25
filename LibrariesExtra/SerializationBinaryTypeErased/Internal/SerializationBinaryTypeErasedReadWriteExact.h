// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../../Libraries/Memory/Buffer.h"
#include "SerializationBinaryTypeErasedCompiler.h"

namespace SC
{
/// @brief A binary serialization bytes writer based on Buffer
struct SerializationBinaryTypeErasedWriter
{
    Buffer& buffer; ///< The underlying buffer holding serialization data

    size_t numberOfOperations = 0; ///< How many read or write operations have been issued so far
    SerializationBinaryTypeErasedWriter(Buffer& buffer) : buffer(buffer) {}

    /// @brief Write given object to buffer
    /// @param object The source object
    /// @param numBytes Size of source object
    /// @return `true` if write succeeded
    [[nodiscard]] bool serializeBytes(const void* object, size_t numBytes)
    {
        return serializeBytes(Span<const char>::reinterpret_bytes(object, numBytes));
    }
    /// @brief Write span of bytes to buffer
    /// @param object Span of bytes
    /// @return `true` if write succeeded
    [[nodiscard]] bool serializeBytes(Span<const char> object)
    {
        numberOfOperations++;
        return buffer.append(object);
    }
};

/// @brief A binary serialization bytes reader reading from a span of memory
struct SerializationBinaryTypeErasedReader
{
    Span<const char> memory;

    size_t numberOfOperations = 0; ///< How many read or write operations have been issued so far
    size_t readPosition       = 0; ///< Current read  position in the buffer

    SerializationBinaryTypeErasedReader(Span<const char> memory) : memory(memory) {}

    [[nodiscard]] bool positionIsAtEnd() const { return readPosition == memory.sizeInBytes(); }

    /// @brief Read from buffer into given object
    /// @param object Destination object
    /// @param numBytes How many bytes to read from object
    /// @return `true` if read succeeded
    [[nodiscard]] bool serializeBytes(void* object, size_t numBytes)
    {
        return serializeBytes(Span<char>::reinterpret_bytes(object, numBytes));
    }

    /// @brief Read bytes into given span of memory. Updates readPosition
    /// @param object The destination span
    /// @return `true` if read succeeded
    [[nodiscard]] bool serializeBytes(Span<char> object)
    {
        if (readPosition + object.sizeInBytes() > memory.sizeInBytes())
            return false;
        numberOfOperations++;
        if (not object.empty())
        {
            ::memcpy(object.data(), &memory.data()[readPosition], object.sizeInBytes());
        }
        readPosition += object.sizeInBytes();
        return true;
    }

    /// @brief Advance read position by numBytes
    /// @param numBytes How many bytes to advance read position of
    /// @return `true` if size of buffer has not been exceeded.
    [[nodiscard]] bool advanceBytes(size_t numBytes)
    {
        if (readPosition + numBytes > memory.sizeInBytes())
            return false;
        readPosition += numBytes;
        return true;
    }
};
/// @brief Writes object `T` to a buffer
struct SerializationBinaryTypeErasedWriteExact
{
    template <typename T>
    [[nodiscard]] constexpr bool write(const T& object, SerializationBinaryTypeErasedWriter& buffer)
    {
        constexpr auto flatSchema = Reflection::SchemaTypeErased::compile<T>();

        using VectorVTable = detail::SerializationBinaryTypeErasedArrayAccess::VectorVTable;

        arrayAccess.vectorVtable =
            Span<const VectorVTable>(flatSchema.vtables.vector.values, flatSchema.vtables.vector.size);

        sourceTypes  = Span<const Reflection::TypeInfo>(flatSchema.typeInfos.values, flatSchema.typeInfos.size);
        sourceNames  = Span<const Reflection::TypeStringView>(flatSchema.typeNames.values, flatSchema.typeNames.size);
        sourceObject = sourceObject.reinterpret_object(object);
        sourceTypeIndex = 0;

        destination = &buffer;

        destination->numberOfOperations = 0;

        if (sourceTypes.sizeInBytes() == 0 || sourceTypes.data()[0].type != Reflection::TypeCategory::TypeStruct)
        {
            return false;
        }
        return write();
    }

  private:
    [[nodiscard]] bool write();
    [[nodiscard]] bool writeStruct();
    [[nodiscard]] bool writeArrayVector();

    SerializationBinaryTypeErasedWriter* destination = nullptr;

    Span<const Reflection::TypeInfo>       sourceTypes;
    Span<const Reflection::TypeStringView> sourceNames;

    Span<const char>     sourceObject;
    uint32_t             sourceTypeIndex;
    Reflection::TypeInfo sourceType;

    detail::SerializationBinaryTypeErasedArrayAccess arrayAccess;
};

/// @brief Reads object `T` from a buffer, assuming no versioning changes with data in buffer
struct SerializationBinaryTypeErasedReadExact
{
    template <typename T>
    [[nodiscard]] bool loadExact(T& object, SerializationBinaryTypeErasedReader& buffer)
    {
        constexpr auto flatSchema = Reflection::SchemaTypeErased::compile<T>();

        using VectorVTable = detail::SerializationBinaryTypeErasedArrayAccess::VectorVTable;
        arrayAccess.vectorVtable =
            Span<const VectorVTable>(flatSchema.vtables.vector.values, flatSchema.vtables.vector.size);

        sinkTypes     = Span<const Reflection::TypeInfo>(flatSchema.typeInfos.values, flatSchema.typeInfos.size);
        sinkNames     = Span<const Reflection::TypeStringView>(flatSchema.typeNames.values, flatSchema.typeNames.size);
        sinkObject    = sinkObject.reinterpret_object(object);
        sinkTypeIndex = 0;

        source = &buffer;

        if (sinkTypes.sizeInBytes() == 0 || sinkTypes.data()[0].type != Reflection::TypeCategory::TypeStruct)
        {
            return false;
        }
        return read();
    }

  private:
    [[nodiscard]] bool read();
    [[nodiscard]] bool readStruct();
    [[nodiscard]] bool readArrayVector();

    SerializationBinaryTypeErasedReader* source = nullptr;

    Span<const Reflection::TypeInfo>       sinkTypes;
    Span<const Reflection::TypeStringView> sinkNames;

    Reflection::TypeInfo sinkType;
    uint32_t             sinkTypeIndex = 0;
    Span<char>           sinkObject;

    detail::SerializationBinaryTypeErasedArrayAccess arrayAccess;
};
} // namespace SC
