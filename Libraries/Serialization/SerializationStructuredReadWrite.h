// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Containers/Array.h"
#include "../Foundation/Containers/Vector.h"
#include "../Foundation/Objects/Result.h"
#include "../Reflection/ReflectionSC.h"

namespace SC
{
namespace SerializationStructuredTemplate
{
template <typename SerializerStream, typename T, typename SFINAESelector = void>
struct SerializationReadWrite;

template <typename SerializerStream, typename T>
struct SerializerReadWriteFastMemberIterator
{
    SerializerStream& stream;
    uint32_t          index = 0;

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R& field)
    {
        SC_UNUSED(order);
        const StringView fieldName = StringView(name, N - 1, true, StringEncoding::Ascii);
        SC_TRY(stream.startObjectField(index++, fieldName));
        return SerializationReadWrite<SerializerStream, R>::serialize(0, field, stream);
    }
};

template <typename SerializerStream, typename T>
struct SerializerReadVersionedMemberIterator
{
    SerializerStream& stream;
    const StringView  fieldToFind;
    const uint32_t    index               = 0;
    bool              consumed            = false;
    bool              consumedWithSuccess = false;

    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R& field)
    {
        SC_UNUSED(order);
        const StringView fieldName = StringView(name, N - 1, true, StringEncoding::Ascii);
        if (fieldName == fieldToFind)
        {
            consumed            = true;
            consumedWithSuccess = SerializationReadWrite<SerializerStream, R>::loadVersioned(0, field, stream);
            return false; // stop iterating members
        }
        return true;
    }
};

template <typename SerializerStream, typename T, typename SFINAESelector>
struct SerializationReadWrite
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, SerializerStream& stream)
    {
        SC_TRY(stream.startObject(index));
        using FastIterator = SerializerReadWriteFastMemberIterator<SerializerStream, T>;
        SC_TRY(Reflection::MetaClass<T>::visitObject(FastIterator{stream}, object));
        return stream.endObject();
    }

    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T& object, SerializerStream& stream)
    {
        SC_TRY(stream.startObject(index));
        StringView fieldToFind;
        uint32_t   fieldIndex = 0;
        bool       hasMore    = false;
        SC_TRY(stream.getNextField(fieldIndex, fieldToFind, hasMore));
        // TODO: Figure out maybe a simple way to allow clearing fields that have not been consumed
        while (hasMore)
        {
            using VersionedIterator = SerializerReadVersionedMemberIterator<SerializerStream, T>;
            VersionedIterator iterator{stream, fieldToFind, fieldIndex};
            Reflection::MetaClass<T>::visitObject(iterator, object);
            SC_TRY(not iterator.consumed or iterator.consumedWithSuccess);
            SC_TRY(stream.getNextField(++fieldIndex, fieldToFind, hasMore));
        }
        return stream.endObject();
    }
};

template <typename SerializerStream, typename T, int N>
struct SerializationReadWrite<SerializerStream, T[N]>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T (&object)[N], SerializerStream& stream)
    {
        SC_TRY(stream.startArray(index));
        uint32_t arrayIndex = 0;
        for (auto& item : object)
        {
            if (not SerializationReadWrite<SerializerStream, T>::serialize(arrayIndex++, item, stream))
                return false;
        }
        return stream.endArray();
    }
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T (&object)[N], SerializerStream& stream)
    {
        return serialize(index, object, stream);
    }
};

template <typename SerializerStream>
struct SerializationReadWrite<SerializerStream, String>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, String& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, String& object, SerializerStream& stream)
    {
        return serialize(index, object, stream);
    }
};

template <typename SerializerStream, typename Container, typename T>
struct SerializerStructuredReaderVector
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, Container& object, SerializerStream& stream)
    {
        uint32_t arraySize = 0;
        SC_TRY(stream.startArray(index, object, arraySize));
        for (uint32_t idx = 0; idx < arraySize; ++idx)
        {
            if (not SerializationReadWrite<SerializerStream, T>::serialize(idx, object[idx], stream))
                return false;
            SC_TRY(stream.endArrayItem(object, arraySize));
        }
        return stream.endArray();
    }
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, Container& object, SerializerStream& stream)
    {
        return serialize(index, object, stream); // TODO: Allow customzing allowed conversions
    }
};

template <typename SerializerStream, typename T>
struct SerializationReadWrite<SerializerStream, SC::Vector<T>>
    : public SerializerStructuredReaderVector<SerializerStream, SC::Vector<T>, T>
{
};

template <typename SerializerStream, typename T, int N>
struct SerializationReadWrite<SerializerStream, SC::Array<T, N>>
    : public SerializerStructuredReaderVector<SerializerStream, SC::Array<T, N>, T>
{
};

template <typename SerializerStream, typename T>
struct SerializationReadWrite<SerializerStream, T, typename SC::EnableIf<Reflection::IsPrimitive<T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, SerializerStream& stream)
    {
        return stream.serialize(index, object);
    }
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, T& object, SerializerStream& stream)
    {
        return serialize(index, object, stream);
    }
};

template <typename SerializerStream, typename T>
[[nodiscard]] constexpr bool serialize(T& object, SerializerStream& stream)
{
    SC_TRY(stream.onSerializationStart());
    SC_TRY((SerializationReadWrite<SerializerStream, T>::serialize(0, object, stream)));
    return stream.onSerializationEnd();
}

template <typename SerializerStream, typename T>
[[nodiscard]] constexpr bool loadVersioned(T& object, SerializerStream& stream)
{
    SC_TRY(stream.onSerializationStart());
    SC_TRY((SerializationReadWrite<SerializerStream, T>::loadVersioned(0, object, stream)));
    return stream.onSerializationEnd();
}
} // namespace SerializationStructuredTemplate
} // namespace SC
