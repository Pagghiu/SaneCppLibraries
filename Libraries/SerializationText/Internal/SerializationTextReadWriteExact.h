// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Reflection/ReflectionSC.h" // TODO: Split the SC Containers specifics in separate header

namespace SC
{
/// @brief Serializes structured formats mostly text based, like JSON (see @ref library_serialization_text).
namespace detail
{
template <typename SerializerStream, typename T, typename SFINAESelector = void>
struct SerializationTextReadWriteExact;

template <typename SerializerStream, typename T, typename SFINAESelector>
struct SerializationTextReadWriteExact
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, SerializerStream& stream)
    {
        if (not stream.startObject(index))
            return false;
        if (not Reflection::Reflect<T>::visit(MemberIterator{stream, object}))
            return false;
        return stream.endObject();
    }

  private:
    struct MemberIterator
    {
        SerializerStream& stream;
        T&                object;

        uint32_t index = 0;

        template <typename R, int N>
        constexpr bool operator()(int, R T::*field, const char (&name)[N], size_t)
        {
            const StringView fieldName = StringView({name, N - 1}, true, StringEncoding::Ascii);
            if (not stream.startObjectField(index++, fieldName))
                return false;
            return SerializationTextReadWriteExact<SerializerStream, R>::serialize(0, object.*field, stream);
        }
    };
};

template <typename SerializerStream, typename T, int N>
struct SerializationTextReadWriteExact<SerializerStream, T[N]>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T (&object)[N], SerializerStream& stream)
    {
        if (not stream.startArray(index))
            return false;
        uint32_t arrayIndex = 0;
        for (auto& item : object)
        {
            if (not SerializationTextReadWriteExact<SerializerStream, T>::serialize(arrayIndex++, item, stream))
                return false;
        }
        return stream.endArray();
    }
};

template <typename SerializerStream, typename T>
struct SerializationTextReadWriteExact<SerializerStream, T,
                                       typename SC::TypeTraits::EnableIf<Reflection::IsPrimitive<T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename SerializerStream, typename Container, typename T>
struct SerializationTextReaderVector
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, Container& object, SerializerStream& stream)
    {
        uint32_t arraySize = 0;
        if (not stream.startArray(index, object, arraySize))
            return false;
        for (uint32_t idx = 0; idx < arraySize; ++idx)
        {
            if (not SerializationTextReadWriteExact<SerializerStream, T>::serialize(idx, object[idx], stream))
                return false;
            if (not stream.endArrayItem(object, arraySize))
                return false;
        }
        return stream.endArray();
    }
};

template <typename SerializerStream>
struct SerializationTextReadWriteExact<SerializerStream, String>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, String& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename SerializerStream, typename T>
struct SerializationTextReadWriteExact<SerializerStream, SC::Vector<T>>
    : public SerializationTextReaderVector<SerializerStream, SC::Vector<T>, T>
{
};

template <typename SerializerStream, typename T, int N>
struct SerializationTextReadWriteExact<SerializerStream, SC::Array<T, N>>
    : public SerializationTextReaderVector<SerializerStream, SC::Array<T, N>, T>
{
};
} // namespace detail

} // namespace SC
