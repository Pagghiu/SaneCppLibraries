// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Span.h"
#include "../../Memory/Buffer.h"

namespace SC
{
//! @addtogroup group_serialization_binary
//! @{

/// @brief A binary serialization bytes writer based on SerializationBinaryBuffer
struct SerializationBinaryBufferWriter
{
    Buffer& buffer; ///< The underlying buffer holding serialization data

    size_t numberOfOperations = 0; ///< How many read or write operations have been issued so far
    SerializationBinaryBufferWriter(Buffer& buffer) : buffer(buffer) {}

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

/// @brief A binary serialization bytes reader based on SerializationBinaryBuffer
struct SerializationBinaryBufferReader
{
    Span<const char> memory;

    size_t numberOfOperations = 0; ///< How many read or write operations have been issued so far
    size_t readPosition       = 0; ///< Current read  position in the buffer

    SerializationBinaryBufferReader(Span<const char> memory) : memory(memory) {}

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
//! @}

} // namespace SC
