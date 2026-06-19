// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/StringSpan.h"
#include "../../Reflection/Reflection.h"

namespace SC
{
namespace Reflection
{
template <>
struct Reflect<StringSpan>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeInvalid; }
};
} // namespace Reflection

/// @brief Serializes structured formats mostly text based, like JSON (see @ref library_serialization_text).
namespace Serialization
{
template <typename TextStream, typename T, typename SFINAESelector = void>
struct SerializationTextReadWriteExact;

/// Wraps the compiler intrinsic because older Apple Clang cannot mangle it directly in a function signature.
template <typename Base, typename Derived>
struct SerializationTextIsBaseOf
{
    static constexpr bool value = __is_base_of(Base, Derived);
};

template <typename T, bool IsStringSpanType = SerializationTextIsBaseOf<StringSpan, T>::value>
struct SerializationTextIsStructSerializable
{
    static constexpr bool value = Reflection::IsStruct<T>::value;
};

template <typename T>
struct SerializationTextIsStructSerializable<T, true>
{
    static constexpr bool value = false;
};

template <typename TextStream, typename T, typename SFINAESelector>
struct SerializationTextReadWriteExact
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, TextStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename TextStream>
struct SerializationTextReadWriteExact<TextStream, StringSpan, void>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, StringSpan& object, TextStream& stream)
    {
        return stream.serialize(index, object);
    }
};

template <typename TextStream, typename T>
struct SerializationTextReadWriteExact<
    TextStream, T,
    typename SC::TypeTraits::EnableIf<SerializationTextIsBaseOf<StringSpan, T>::value and
                                      not SC::TypeTraits::IsSame<StringSpan, T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, TextStream& stream)
    {
        return stream.serialize(index, static_cast<StringSpan&>(object));
    }
};

template <typename TextStream, typename T, int N>
struct SerializationTextReadWriteExact<TextStream, T[N]>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T (&object)[N], TextStream& stream)
    {
        if (not stream.startArray(index))
            return false;
        uint32_t arrayIndex = 0;
        for (auto& item : object)
        {
            if (not SerializationTextReadWriteExact<TextStream, T>::serialize(arrayIndex++, item, stream))
                return false;
        }
        return stream.endArray();
    }
};

template <typename TextStream, typename T>
struct SerializationTextReadWriteExact<
    TextStream, T, typename SC::TypeTraits::EnableIf<SerializationTextIsStructSerializable<T>::value>::type>
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, T& object, TextStream& stream)
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
        TextStream& stream;
        T&          object;

        uint32_t index = 0;

        template <typename R, int N>
        constexpr bool operator()(int, R T::* field, const char (&name)[N], size_t)
        {
            const StringSpan fieldName = StringSpan({name, N - 1}, true, StringEncoding::Ascii);
            if (not stream.startObjectField(index++, fieldName))
                return false;
            return SerializationTextReadWriteExact<TextStream, R>::serialize(0, object.*field, stream);
        }
    };
};

template <typename TextStream, typename Container, typename T>
struct SerializationTextExactVector
{
    [[nodiscard]] static constexpr bool serialize(uint32_t index, Container& object, TextStream& stream)
    {
        uint32_t arraySize = 0;
        if (not stream.startArray(index, object, arraySize))
            return false;
        for (decltype(object.size()) idx = 0; idx < static_cast<decltype(idx)>(arraySize); ++idx)
        {
            auto data            = Reflection::ExtendedTypeInfo<Container>::data(object);
            using SerializerForT = SerializationTextReadWriteExact<TextStream, T>;
            if (not SerializerForT::serialize(static_cast<uint32_t>(idx), data[idx], stream))
                return false;
            if (not stream.endArrayItem(object, arraySize))
                return false;
        }
        return stream.endArray();
    }
};

} // namespace Serialization

} // namespace SC
