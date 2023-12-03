// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Reflection/ReflectionSC.h" // TODO: Split the SC Containers specifics in separate header

namespace SC
{
/// @brief Serializes C++ objects to a binary format (see @ref library_serialization_binary).
namespace SerializationBinary
{
namespace detail
{
/// @brief Binary serializer using Reflect
template <typename BinaryStream, typename T, typename SFINAESelector = void>
struct SerializerReadWriteFast;

/// @brief Struct serializer
template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerReadWriteFast
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
            return SerializerReadWriteFast<BinaryStream, R>::serialize(object.*field, stream);
        }
    };
};

/// @brief Array serializer
template <typename BinaryStream, typename T, int N>
struct SerializerReadWriteFast<BinaryStream, T[N]>
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
                if (not SerializerReadWriteFast<BinaryStream, T>::serialize(item, stream))
                    return false;
            }
            return true;
        }
    }
};

/// @brief Primitive serializer
template <typename BinaryStream, typename T>
struct SerializerReadWriteFast<BinaryStream, T,
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
        if (not SerializerReadWriteFast<BinaryStream, uint64_t>::serialize(sizeInBytes, stream))
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
                if (not SerializerReadWriteFast<BinaryStream, T>::serialize(item, stream))
                    return false;
            }
            return true;
        }
    }
};

// clang-format off
/// @brief Specialization for `SC::Vector<T>` types
template <typename BinaryStream, typename T>
struct SerializerReadWriteFast<BinaryStream, SC::Vector<T>> : public SerializerVector<BinaryStream, SC::Vector<T>, T> { };

/// @brief Specialization for `SC::Array<T, N>` types
template <typename BinaryStream, typename T, int N>
struct SerializerReadWriteFast<BinaryStream, SC::Array<T, N>> : public SerializerVector<BinaryStream, SC::Array<T, N>, T> { };
// clang-format on
} // namespace detail

//! @addtogroup group_serialization_binary
//! @{

/// @brief Reads or writes object `T` from and to a buffer, assuming no versioning changes
struct ReadWriteFast
{
    /// @brief Serializes or deserializes object `T` to or from stream
    /// @tparam T Type of object to be serialized/deserialized
    /// @tparam StreamType Any stream type ducking Binary SC::SerializationBinary::Buffer
    /// @param value The object to be serialized / deserialized
    /// @param stream The stream holding actual bytes for serialization / deserialization
    /// @return `true` if the operation succeeded
    template <typename T, typename StreamType>
    [[nodiscard]] bool serialize(T& value, StreamType& stream)
    {
        using Serializer = detail::SerializerReadWriteFast<StreamType, T>;
        return Serializer::serialize(value, stream);
    }
};

//! @}

} // namespace SerializationBinary
} // namespace SC
