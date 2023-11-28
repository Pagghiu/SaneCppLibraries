// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Containers/Vector.h"
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"

namespace SC
{
namespace SerializationBinary
{
// TODO: BinaryBuffer needs to go with once we transition to streaming interface

/// @brief A simple binary reader/writer backed by a memory buffer.
struct BinaryBuffer
{
    SC::Vector<uint8_t> buffer; ///< The underlying buffer holding serialization data

    size_t readPosition       = 0; ///< Current read  position in the buffer
    size_t numberOfOperations = 0; ///< How many read or write operations have been issued so far

    /// @brief Write span of bytes to buffer
    /// @param object Span of bytes
    /// @return `true` if write succeeded
    [[nodiscard]] bool serializeBytes(Span<const uint8_t> object)
    {
        numberOfOperations++;
        return buffer.append(object);
    }

    /// @brief Read bytes into given span of memory. Updates readPosition
    /// @param object The destination span
    /// @return `true` if read succeded
    [[nodiscard]] bool serializeBytes(Span<uint8_t> object)
    {
        if (readPosition + object.sizeInBytes() > buffer.size())
            return false;
        numberOfOperations++;
        memcpy(object.data(), &buffer[readPosition], object.sizeInBytes());
        readPosition += object.sizeInBytes();
        return true;
    }

    /// @brief Advance read position by numBytes
    /// @param numBytes How many bytes to advance read position of
    /// @return `true` if size of buffer has not been exceeded.
    [[nodiscard]] bool advanceBytes(size_t numBytes)
    {
        if (readPosition + numBytes > buffer.size())
            return false;
        readPosition += numBytes;
        return true;
    }
};

struct BinaryWriterStream : public BinaryBuffer
{
    using BinaryBuffer::serializeBytes;
    /// @brief Write given object to buffer
    /// @param object The source object
    /// @param numBytes Size of source object
    /// @return `true` if write succeeded
    [[nodiscard]] bool serializeBytes(const void* object, size_t numBytes)
    {
        return BinaryBuffer::serializeBytes(Span<const uint8_t>::reinterpret_bytes(object, numBytes));
    }
};

struct BinaryReaderStream : public BinaryBuffer
{
    using BinaryBuffer::serializeBytes;
    /// @brief Read from buffer into given object
    /// @param object Destination object
    /// @param numBytes How many bytes to read from object
    /// @return `true` if read succeeded
    [[nodiscard]] bool serializeBytes(void* object, size_t numBytes)
    {
        return BinaryBuffer::serializeBytes(Span<uint8_t>::reinterpret_bytes(object, numBytes));
    }
};
} // namespace SerializationBinary
} // namespace SC
