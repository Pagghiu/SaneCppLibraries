// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Array.h"
#include "../Foundation/Result.h"
#include "../Foundation/Vector.h"
#include "../Reflection/ReflectionSC.h"

namespace SC
{
namespace SerializationStructuredTemplate
{
template <typename SerializerStream, typename T, typename SFINAESelector = void>
struct SerializationReadWriteFast;

template <typename SerializerStream, typename T>
struct SerializerStructuredReaderMemberIterator
{
    SerializerStream& stream;
    uint32_t          index = 0;

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R& field)
    {
        SC_UNUSED(order);
        SC_TRY_IF(stream.objectFieldName(index++, StringView(name, N - 1, true, StringEncoding::Ascii)));
        return SerializationReadWriteFast<SerializerStream, R>::serialize(0, field, stream);
    }
};

template <typename SerializerStream, typename T, typename SFINAESelector>
struct SerializationReadWriteFast
{
    [[nodiscard]] static constexpr bool startSerialization(T& object, SerializerStream& stream)
    {
        SC_TRY_IF(stream.onSerializationStart());
        SC_TRY_IF(serialize(0, object, stream));
        return stream.onSerializationEnd();
    }

    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, SerializerStream& stream)
    {
        SC_TRY_IF(stream.startObject(index));
        SC_TRY_IF(Reflection::MetaClass<T>::visitObject(
            SerializerStructuredReaderMemberIterator<SerializerStream, T>{stream}, object));
        return stream.endObject();
    }
};

template <typename SerializerStream, typename T, int N>
struct SerializationReadWriteFast<SerializerStream, T[N]>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T (&object)[N], SerializerStream& stream)
    {
        SC_TRY_IF(stream.startArray(index));
        uint32_t arrayIndex = 0;
        for (auto& item : object)
        {
            if (not SerializationReadWriteFast<SerializerStream, T>::serialize(arrayIndex++, item, stream))
                return false;
        }
        return stream.endArray();
    }
};

template <typename SerializerStream>
struct SerializationReadWriteFast<SerializerStream, String>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, String& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename SerializerStream, typename Container, typename T>
struct SerializerStructuredReaderVector
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, Container& object, SerializerStream& stream)
    {
        uint32_t arraySize = 0;
        SC_TRY_IF(stream.startArray(index, object, arraySize));
        for (uint32_t idx = 0; idx < arraySize; ++idx)
        {
            if (!SerializationReadWriteFast<SerializerStream, T>::serialize(idx, object[idx], stream))
                return false;
            SC_TRY_IF(stream.arrayItem(object, arraySize));
        }
        return stream.endArray();
    }
};

template <typename SerializerStream, typename T>
struct SerializationReadWriteFast<SerializerStream, SC::Vector<T>>
    : public SerializerStructuredReaderVector<SerializerStream, SC::Vector<T>, T>
{
};

template <typename SerializerStream, typename T, int N>
struct SerializationReadWriteFast<SerializerStream, SC::Array<T, N>>
    : public SerializerStructuredReaderVector<SerializerStream, SC::Array<T, N>, T>
{
};

template <typename SerializerStream, typename T>
struct SerializationReadWriteFast<SerializerStream, T, typename SC::EnableIf<Reflection::IsPrimitive<T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
};

} // namespace SerializationStructuredTemplate
} // namespace SC
