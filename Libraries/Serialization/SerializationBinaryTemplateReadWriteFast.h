// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Reflection/ReflectionSC.h"

namespace SC
{
namespace SerializationBinaryTemplate
{
template <typename BinaryStream, typename T, typename SFINAESelector = void>
struct SerializerReadWriteFast;

template <typename BinaryStream, typename T>
struct SerializerMemberIterator
{
    BinaryStream& stream;

    template <typename R, int N>
    constexpr bool operator()(int /*order*/, const char (&/*name*/)[N], R& field) const
    {
        return SerializerReadWriteFast<BinaryStream, R>::serialize(field, stream);
    }
};

// Struct serializer
template <typename BinaryStream, typename T, typename SFINAESelector>
struct SerializerReadWriteFast
{
    [[nodiscard]] static constexpr bool serialize(T& object, BinaryStream& stream)
    {
        if (Reflection::MetaTypeInfo<T>::IsPacked)
        {
            return stream.serializeBytes(&object, sizeof(T));
        }
        return Reflection::MetaClass<T>::visitObject(SerializerMemberIterator<BinaryStream, T>{stream}, object);
    }
};

// Array serializer
template <typename BinaryStream, typename T, int N>
struct SerializerReadWriteFast<BinaryStream, T[N]>
{
    [[nodiscard]] static constexpr bool serialize(T (&object)[N], BinaryStream& stream)
    {
        if (Reflection::MetaTypeInfo<T>::IsPacked)
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

// Primitive serializer
template <typename BinaryStream, typename T>
struct SerializerReadWriteFast<BinaryStream, T, typename SC::EnableIf<Reflection::IsPrimitive<T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(T& object, BinaryStream& stream)
    {
        return stream.serializeBytes(&object, sizeof(T));
    }
};

// Generic serializer for anything "vector-like"
template <typename BinaryStream, typename Container, typename T>
struct SerializerVector
{
    [[nodiscard]] static constexpr bool serialize(Container& object, BinaryStream& stream)
    {
        const size_t itemSize    = sizeof(T);
        uint64_t     sizeInBytes = static_cast<uint64_t>(object.size() * itemSize);
        if (not SerializerReadWriteFast<BinaryStream, uint64_t>::serialize(sizeInBytes, stream))
            return false;
        SC_TRY(object.resize(static_cast<size_t>(sizeInBytes / itemSize)));

        if (Reflection::MetaTypeInfo<T>::IsPacked)
        {
            return stream.serializeBytes(object.data(), itemSize * object.size());
        }
        else
        {
            for (auto& item : object)
            {
                if (!SerializerReadWriteFast<BinaryStream, T>::serialize(item, stream))
                    return false;
            }
            return true;
        }
    }
};

// Custom Types using the generic "vector-like" serializer
// clang-format off
template <typename BinaryStream, typename T>
struct SerializerReadWriteFast<BinaryStream, SC::Vector<T>> : public SerializerVector<BinaryStream, SC::Vector<T>, T> { };

template <typename BinaryStream, typename T, int N>
struct SerializerReadWriteFast<BinaryStream, SC::Array<T, N>> : public SerializerVector<BinaryStream, SC::Array<T, N>, T> { };
// clang-format on

} // namespace SerializationBinaryTemplate
} // namespace SC
