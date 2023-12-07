// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SerializationTextReadWriteExact.h"

namespace SC
{
/// @brief Serializes structured formats mostly text based, like JSON (see @ref library_serialization_text).
namespace detail
{
template <typename SerializerStream, typename T, typename SFINAESelector = void>
struct SerializationTextReadVersioned;

template <typename SerializerStream, typename T, typename SFINAESelector>
struct SerializationTextReadVersioned
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T& object, SerializerStream& stream)
    {
        if (not stream.startObject(index))
            return false;
        StringView fieldToFind;
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

        const StringView fieldToFind;

        bool consumed            = false;
        bool consumedWithSuccess = false;

        template <typename R, int N>
        constexpr bool operator()(int, R T::*field, const char (&name)[N], size_t)
        {
            const StringView fieldName = StringView(name, N - 1, true, StringEncoding::Ascii);
            if (fieldName == fieldToFind)
            {
                consumed = true;
                consumedWithSuccess =
                    SerializationTextReadVersioned<SerializerStream, R>::loadVersioned(0, object.*field, stream);
                return false; // stop iterating members
            }
            return true;
        }
    };
};

template <typename SerializerStream, typename T, int N>
struct SerializationTextReadVersioned<SerializerStream, T[N]>
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T (&object)[N], SerializerStream& stream)
    {
        return SerializationTextReadWriteExact<SerializerStream, T[N]>::serialize(index, object, stream);
    }
};

template <typename SerializerStream, typename T>
struct SerializationTextReadVersioned<SerializerStream, T,
                                      typename SC::TypeTraits::EnableIf<Reflection::IsPrimitive<T>::value>::type>
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename SerializerStream, typename Container, typename T>
struct SerializationTextReaderVersionedVector
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, Container& object, SerializerStream& stream)
    {
        // TODO: Allow customizing allowed conversions
        return SerializationTextReaderVector<SerializerStream, Container, T>::serialize(index, object, stream);
    }
};

template <typename SerializerStream>
struct SerializationTextReadVersioned<SerializerStream, String>
{
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, String& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename SerializerStream, typename T>
struct SerializationTextReadVersioned<SerializerStream, SC::Vector<T>>
    : public SerializationTextReaderVersionedVector<SerializerStream, SC::Vector<T>, T>
{
};

template <typename SerializerStream, typename T, int N>
struct SerializationTextReadVersioned<SerializerStream, SC::Array<T, N>>
    : public SerializationTextReaderVersionedVector<SerializerStream, SC::Array<T, N>, T>
{
};

} // namespace detail

} // namespace SC
