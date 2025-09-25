// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../ContainersSerialization/MemoryReflection.h"

namespace SC
{
namespace Serialization
{

// Forward Declarations (to avoid depending on SerializationBinary)
template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerBinaryReadWriteExact;
template <typename BinaryStream, typename Container, typename T>
struct SerializerBinaryExactVector;

// Forward Declarations (to avoid depending on SerializationText)
template <typename TextStream, typename T, typename SFINAESelector>
struct SerializationTextReadWriteExact;
template <typename TextStream, typename Container, typename T>
struct SerializationTextExactVector;

// clang-format off
/// @brief Specialization for `SC::Buffer` type
template <typename BinaryStream>
struct SerializerBinaryReadWriteExact<BinaryStream, SC::Buffer, void> : public SerializerBinaryExactVector<BinaryStream, SC::Buffer, char> { };

template <typename BinaryStream>
struct SerializerBinaryReadVersioned<BinaryStream, SC::Buffer, void> : public SerializationBinaryVersionedVector<BinaryStream, SC::Buffer, char, 0xffffffff> { };

template <typename TextStream>
struct SerializationTextReadWriteExact<TextStream, String, void>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, String& object, TextStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename SerializerStream>
struct SerializationTextReadVersioned<SerializerStream, String, void>
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, String& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
};

// clang-format on
} // namespace Serialization
} // namespace SC
