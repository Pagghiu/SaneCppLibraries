// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "SerializationTextReadWriteExact.h"

namespace SC
{
/// @brief Serializes structured formats mostly text based, like JSON (see @ref library_serialization_text).
namespace Serialization
{

template <typename SerializerStream, typename T, typename SFINAESelector>
struct SerializationTextReadVersioned
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename SerializerStream, typename T, int N>
struct SerializationTextReadVersioned<SerializerStream, T[N], void>
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T (&object)[N], SerializerStream& stream)
    {
        return SerializationTextReadWriteExact<SerializerStream, T[N]>::serialize(index, object, stream);
    }
};

template <typename SerializerStream, typename T>
struct SerializationTextReadVersioned<SerializerStream, T,
                                      typename SC::TypeTraits::EnableIf<Reflection::IsStruct<T>::value>::type>
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T& object, SerializerStream& stream)
    {
        if (not stream.startObject(index))
            return false;
        StringSpan fieldToFind;
        uint32_t   fieldIndex = 0;
        bool       hasMore    = false;
        if (not stream.getNextField(fieldIndex, fieldToFind, hasMore))
            return false;
        // TODO: Figure out maybe a simple way to allow clearing fields that have not been consumed
        while (hasMore)
        {
            MemberIterator iterator{stream, object, fieldToFind};
            Reflection::Reflect<T>::visit(iterator);
            if (not iterator.consumed or not iterator.consumedWithSuccess)
                return false;
            if (not stream.getNextField(++fieldIndex, fieldToFind, hasMore))
                return false;
        }
        return stream.endObject();
    }

  private:
    struct MemberIterator
    {
        SerializerStream& stream;
        T&                object;

        const StringSpan fieldToFind;

        bool consumed            = false;
        bool consumedWithSuccess = false;

        template <typename R, int N>
        constexpr bool operator()(int, R T::* field, const char (&name)[N], size_t)
        {
            const StringSpan fieldName = StringSpan({name, N - 1}, true, StringEncoding::Ascii);
            if (fieldName == fieldToFind)
            {
                consumed = true;
                consumedWithSuccess =
                    SerializationTextReadVersioned<SerializerStream, R, void>::loadVersioned(0, object.*field, stream);
                return false; // stop iterating members
            }
            return true;
        }
    };
};

template <typename SerializerStream, typename Container, typename T>
struct SerializationTextVersionedVector
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, Container& object, SerializerStream& stream)
    {
        // TODO: Allow customizing allowed conversions
        return SerializationTextExactVector<SerializerStream, Container, T>::serialize(index, object, stream);
    }
};
} // namespace Serialization

} // namespace SC
