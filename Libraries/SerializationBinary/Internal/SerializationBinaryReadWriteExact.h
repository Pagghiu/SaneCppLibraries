// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Reflection/ReflectionSC.h" // TODO: Split the SC Containers specifics in separate header

namespace SC
{
namespace detail
{
/// @brief Binary serializer using Reflect
template <typename BinaryStream, typename T, typename SFINAESelector = void>
struct SerializerBinaryReadWriteExact;

/// @brief Struct serializer
template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerBinaryReadWriteExact
{
    [[nodiscard]] static constexpr bool serialize(T& object, BinaryStream& stream)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (Reflection::ExtendedTypeInfo<T>::IsPacked)
        {
            return stream.serializeBytes(&object, sizeof(T));
        }
        else
        {
            return Reflection::Reflect<T>::visit(MemberIterator{stream, object});
        }
    }

  private:
    struct MemberIterator
    {
        BinaryStream& stream;
        T&            object;

        template <typename R, int N>
        constexpr bool operator()(int /*memberTag*/, R T::*field, const char (&/*name*/)[N], size_t offset) const
        {
            SC_COMPILER_UNUSED(offset);
            return SerializerBinaryReadWriteExact<BinaryStream, R>::serialize(object.*field, stream);
        }
    };
};

/// @brief Array serializer
template <typename BinaryStream, typename T, int N>
struct SerializerBinaryReadWriteExact<BinaryStream, T[N]>
{
    [[nodiscard]] static constexpr bool serialize(T (&object)[N], BinaryStream& stream)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (Reflection::ExtendedTypeInfo<T>::IsPacked)
        {
            return stream.serializeBytes(object, sizeof(object));
        }
        else
        {
            for (auto& item : object)
            {
                if (not SerializerBinaryReadWriteExact<BinaryStream, T>::serialize(item, stream))
                    return false;
            }
            return true;
        }
    }
};

/// @brief Primitive serializer
template <typename BinaryStream, typename T>
struct SerializerBinaryReadWriteExact<BinaryStream, T,
                                      typename SC::TypeTraits::EnableIf<Reflection::IsPrimitive<T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(T& object, BinaryStream& stream)
    {
        return stream.serializeBytes(&object, sizeof(T));
    }
};

/// @brief Generic serializer for `Vector<T>`-like objects
template <typename BinaryStream, typename Container, typename T>
struct SerializerVector
{
    [[nodiscard]] static constexpr bool serialize(Container& object, BinaryStream& stream)
    {
        constexpr size_t itemSize    = sizeof(T);
        uint64_t         sizeInBytes = static_cast<uint64_t>(object.size() * itemSize);
        if (not SerializerBinaryReadWriteExact<BinaryStream, uint64_t>::serialize(sizeInBytes, stream))
            return false;

        const auto numElements = static_cast<size_t>(sizeInBytes / itemSize);

        if SC_LANGUAGE_IF_CONSTEXPR (Reflection::ExtendedTypeInfo<T>::IsPacked)
        {
#if SC_LANGUAGE_CPP_AT_LEAST_17
            if (not object.resizeWithoutInitializing(numElements))
                return false;
#else
            if (not object.resize(numElements)) // TODO: C++ 14 mode would need SFINAE as it lacks "if constexpr"
                return false;
#endif
            return stream.serializeBytes(object.data(), itemSize * numElements);
        }
        else
        {
            if (not object.resize(numElements))
                return false;
            for (auto& item : object)
            {
                if (not SerializerBinaryReadWriteExact<BinaryStream, T>::serialize(item, stream))
                    return false;
            }
            return true;
        }
    }
};

// clang-format off
/// @brief Specialization for `SC::Vector<T>` types
template <typename BinaryStream, typename T>
struct SerializerBinaryReadWriteExact<BinaryStream, SC::Vector<T>> : public SerializerVector<BinaryStream, SC::Vector<T>, T> { };

/// @brief Specialization for `SC::Array<T, N>` types
template <typename BinaryStream, typename T, int N>
struct SerializerBinaryReadWriteExact<BinaryStream, SC::Array<T, N>> : public SerializerVector<BinaryStream, SC::Array<T, N>, T> { };
// clang-format on
} // namespace detail

} // namespace SC
