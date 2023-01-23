// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "InitializerList.h"
#include "Language.h"
#include "LibC.h"
#include "Types.h"

namespace SC
{
enum class Comparison
{
    Smaller = -1,
    Equals  = 0,
    Bigger  = 1
};

template <typename Type>
struct SpanBase
{
    typedef typename SameConstnessAs<Type, uint8_t>::type ByteType;
    typedef SC::size_t                                    Size;

    Type* data;
    Size  size;

    constexpr SpanBase() : data(nullptr), size(0) {}
    constexpr SpanBase(Type* data, Size size) : data(data), size(size) {}
    constexpr SpanBase(std::initializer_list<Type> ilist)
    {
        data = ilist.begin();
        size = ilist.size();
    }
    constexpr bool isNull() const { return data == nullptr; }

    [[nodiscard]] constexpr bool viewAt(Size offset, Size length, SpanBase& other)
    {
        if (offset + length <= size)
        {
            other = SpanBase(static_cast<ByteType*>(data) + offset, length);
            return true;
        }
        else
        {
            return false;
        }
    }
};

template <typename Type>
struct Span : public SpanBase<Type>
{
    using SpanBase<Type>::data;
    using SpanBase<Type>::size;
    using typename SpanBase<Type>::Size;
    using typename SpanBase<Type>::ByteType;

    constexpr Span() : SpanBase<Type>() {}
    constexpr Span(Type& type) : SpanBase<Type>(&type, 1) {}
    constexpr Span(Type&& type) : SpanBase<Type>(&type, 1) {}

    // Specialization for converting const char* to StringView
    template <int N>
    constexpr Span(const char (&type)[N]) : Span(Type(type))
    {}
    constexpr Span(Type* data, Size size) : SpanBase<Type>(data, size) {}
    constexpr Span(std::initializer_list<Type> ilist)
    {
        data = ilist.begin();
        size = ilist.size();
    }

    [[nodiscard]] bool insertCopy(size_t idx, const Type* source, Size sourceSize)
    {
        if (sourceSize + idx <= size)
        {
            memcpy(data + idx, source, sourceSize * sizeof(Type));
            return true;
        }
        return false;
    }
};

template <>
struct Span<void> : public SpanBase<void>
{
    constexpr Span() : SpanBase<void>() {}
    constexpr Span(void* data, Size size) : SpanBase<void>(data, size) {}

    template <typename DestinationType>
    constexpr Span<DestinationType> castTo()
    {
        return Span<DestinationType>(static_cast<DestinationType*>(data), size);
    }
};

template <>
struct Span<const void> : public SpanBase<const void>
{
    constexpr Span() : SpanBase<const void>() {}
    constexpr Span(const void* data, Size size) : SpanBase<const void>(data, size) {}

    [[nodiscard]] bool copyTo(Span<void> other)
    {
        if (other.size >= size)
        {
            memcpy(other.data, data, size);
            return true;
        }
        return false;
    }

    template <typename DestinationType>
    constexpr Span<DestinationType> castTo()
    {
        return Span<DestinationType>(static_cast<DestinationType*>(data), size);
    }
};

} // namespace SC
