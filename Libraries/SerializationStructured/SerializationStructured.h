// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Reflection/ReflectionSC.h" // TODO: Split the SC Containers specifics in separate header

namespace SC
{
/// @brief Serializes structured formats mostly text based, like JSON (see @ref library_serialization_structured).
namespace SerializationStructured
{
namespace detail
{
template <typename SerializerStream, typename T, typename SFINAESelector = void>
struct ReadWrite;

template <typename SerializerStream, typename T, typename SFINAESelector>
struct ReadWrite
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, SerializerStream& stream)
    {
        if (not stream.startObject(index))
            return false;
        if (not Reflection::Reflect<T>::visit(FastMemberIterator{stream, object}))
            return false;
        return stream.endObject();
    }

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
            LoadVersionedMemberIterator iterator{stream, object, fieldToFind, fieldIndex};
            Reflection::Reflect<T>::visit(iterator);
            if (not iterator.consumed or not iterator.consumedWithSuccess)
                return false;
            if (not stream.getNextField(++fieldIndex, fieldToFind, hasMore))
                return false;
        }
        return stream.endObject();
    }

  private:
    struct FastMemberIterator
    {
        SerializerStream& stream;
        T&                object;

        uint32_t index = 0;

        template <typename R, int N>
        constexpr bool operator()(int, R T::*field, const char (&name)[N], size_t)
        {
            const StringView fieldName = StringView(name, N - 1, true, StringEncoding::Ascii);
            if (not stream.startObjectField(index++, fieldName))
                return false;
            return ReadWrite<SerializerStream, R>::serialize(0, object.*field, stream);
        }
    };

    struct LoadVersionedMemberIterator
    {
        SerializerStream& stream;
        T&                object;

        const StringView fieldToFind;
        const uint32_t   index = 0;

        bool consumed            = false;
        bool consumedWithSuccess = false;

        template <typename R, int N>
        constexpr bool operator()(int, R T::*field, const char (&name)[N], size_t)
        {
            const StringView fieldName = StringView(name, N - 1, true, StringEncoding::Ascii);
            if (fieldName == fieldToFind)
            {
                consumed            = true;
                consumedWithSuccess = ReadWrite<SerializerStream, R>::loadVersioned(0, object.*field, stream);
                return false; // stop iterating members
            }
            return true;
        }
    };
};

template <typename SerializerStream, typename T, int N>
struct ReadWrite<SerializerStream, T[N]>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T (&object)[N], SerializerStream& stream)
    {
        if (not stream.startArray(index))
            return false;
        uint32_t arrayIndex = 0;
        for (auto& item : object)
        {
            if (not ReadWrite<SerializerStream, T>::serialize(arrayIndex++, item, stream))
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
struct ReadWrite<SerializerStream, String>
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
        if (not stream.startArray(index, object, arraySize))
            return false;
        for (uint32_t idx = 0; idx < arraySize; ++idx)
        {
            if (not ReadWrite<SerializerStream, T>::serialize(idx, object[idx], stream))
                return false;
            if (not stream.endArrayItem(object, arraySize))
                return false;
        }
        return stream.endArray();
    }
    [[nodiscard]] static constexpr bool loadVersioned(uint32_t index, Container& object, SerializerStream& stream)
    {
        return serialize(index, object, stream); // TODO: Allow customizing allowed conversions
    }
};

template <typename SerializerStream, typename T>
struct ReadWrite<SerializerStream, T, typename SC::EnableIf<Reflection::IsPrimitive<T>::value>::type>
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
} // namespace detail

//! @defgroup group_serialization_structured Serialization Structured
//! @copybrief library_serialization_structured
//!
//! See @ref library_serialization_structured library page for more details.<br>

//! @addtogroup group_serialization_structured
//! @{

/// @brief Serializes text structured formats using reflection information
struct Serializer
{
    /// @brief Serializes object using a read or write structured serializer stream.
    /// When reading the read stream must match 1:1 what was previously written, or it will fail.
    /// @see Serializer::loadVersioned if you need support for versioned deserialization.
    /// @tparam SerializerStream A Read or Write structured serializer stream (for example JsonWriter or JsonReader)
    /// @tparam T Inferred type of object to serialize
    /// @param object Object to serialize
    /// @param stream A read or write structured serializer stream
    /// @return `true` if serialization succeeded
    template <typename SerializerStream, typename T>
    [[nodiscard]] static constexpr bool serialize(T& object, SerializerStream& stream)
    {
        if (not stream.onSerializationStart())
            return false;
        if (not detail::ReadWrite<SerializerStream, T>::serialize(0, object, stream))
            return false;
        return stream.onSerializationEnd();
    }

    /// @brief Loads an object using a read serializer stream
    /// @tparam SerializerStream A Read structured serializer stream (for example JsonReader)
    /// @tparam T Inferred type of object to load
    /// @param object Object to load
    /// @param stream A read structured serializer stream
    /// @return `true` if load succeded
    template <typename SerializerStream, typename T>
    [[nodiscard]] static constexpr bool loadVersioned(T& object, SerializerStream& stream)
    {
        if (not stream.onSerializationStart())
            return false;
        if (not detail::ReadWrite<SerializerStream, T>::loadVersioned(0, object, stream))
            return false;
        return stream.onSerializationEnd();
    }
};
//! @}

template <typename SerializerStream, typename T>
struct detail::ReadWrite<SerializerStream, SC::Vector<T>>
    : public SerializerStructuredReaderVector<SerializerStream, SC::Vector<T>, T>
{
};

template <typename SerializerStream, typename T, int N>
struct detail::ReadWrite<SerializerStream, SC::Array<T, N>>
    : public SerializerStructuredReaderVector<SerializerStream, SC::Array<T, N>, T>
{
};
} // namespace SerializationStructured
} // namespace SC
