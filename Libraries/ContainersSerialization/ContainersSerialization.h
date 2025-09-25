// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../ContainersSerialization/ContainersReflection.h"

namespace SC
{
namespace Serialization
{

// Forward Declarations (to avoid depending on SerializationBinary)
template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerBinaryReadWriteExact;
template <typename BinaryStream, typename Container, typename T>
struct SerializerBinaryExactVector;
template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerBinaryReadVersioned;
template <typename BinaryStream, typename Container, typename T, size_t NumMaxItems>
struct SerializationBinaryVersionedVector;

// Forward Declarations (to avoid depending on SerializationText)
template <typename SerializerStream, typename Container, typename T>
struct SerializationTextVersionedVector;
template <typename TextStream, typename T, typename SFINAESelector>
struct SerializationTextReadWriteExact;
template <typename TextStream, typename Container, typename T>
struct SerializationTextExactVector;
template <typename SerializerStream, typename T, typename SFINAESelector>
struct SerializationTextReadVersioned;

// clang-format off
/// @brief Specialization for `SC::Vector<T>` types
template <typename BinaryStream, typename T>
struct SerializerBinaryReadWriteExact<BinaryStream, SC::Vector<T>, void> : public SerializerBinaryExactVector<BinaryStream, SC::Vector<T>, T> { };

/// @brief Specialization for `SC::Array<T, N>` types
template <typename BinaryStream, typename T, int N>
struct SerializerBinaryReadWriteExact<BinaryStream, SC::Array<T, N>, void> : public SerializerBinaryExactVector<BinaryStream, SC::Array<T, N>, T> { };

template <typename BinaryStream, typename T>
struct SerializerBinaryReadVersioned<BinaryStream, SC::Vector<T>, void> : public SerializationBinaryVersionedVector<BinaryStream, SC::Vector<T>, T, 0xffffffff> { };

template <typename BinaryStream, typename T, int N>
struct SerializerBinaryReadVersioned<BinaryStream, SC::Array<T, N>, void> : public SerializationBinaryVersionedVector<BinaryStream, SC::Array<T, N>, T, N> { };



template <typename TextStream, typename T>
struct SerializationTextReadWriteExact<TextStream, SC::Vector<T>, void>
    : public SerializationTextExactVector<TextStream, SC::Vector<T>, T>
{
};

template <typename TextStream, typename T, int N>
struct SerializationTextReadWriteExact<TextStream, SC::Array<T, N>, void>
    : public SerializationTextExactVector<TextStream, SC::Array<T, N>, T>
{
};



template <typename SerializerStream, typename T>
struct SerializationTextReadVersioned<SerializerStream, SC::Vector<T>, void>
    : public SerializationTextVersionedVector<SerializerStream, SC::Vector<T>, T>
{
};

template <typename SerializerStream, typename T, int N>
struct SerializationTextReadVersioned<SerializerStream, SC::Array<T, N>, void>
    : public SerializationTextVersionedVector<SerializerStream, SC::Array<T, N>, T>
{
};

// clang-format on
} // namespace Serialization
} // namespace SC
