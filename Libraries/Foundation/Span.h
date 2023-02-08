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
template <typename Type>
struct SpanVoid;

template <typename Type>
struct Span
{
    typedef typename SameConstnessAs<Type, uint8_t>::type ByteType;

    typedef SC::size_t Size;

    constexpr Span() : items(nullptr), sizeBytes(0) {}
    constexpr Span(Type* items, Size sizeInBytes) : items(items), sizeBytes(sizeInBytes) {}
    constexpr Span(Type& type) : items(&type), sizeBytes(sizeof(Type)) {}
    constexpr Span(Type&& type) : items(&type), sizeBytes(sizeof(Type)) {}

    // Specialization for converting const char* to StringView
    constexpr Span(std::initializer_list<Type> ilist)
    {
        items     = ilist.begin();
        sizeBytes = ilist.size() * sizeof(Type);
    }

    constexpr const Type* begin() const { return items; }
    constexpr const Type* end() const { return items + sizeBytes / sizeof(Type); }
    constexpr const Type* data() const { return items; }
    constexpr Type*       begin() { return items; }
    constexpr Type*       end() { return items + sizeBytes / sizeof(Type); }
    constexpr Type*       data() { return items; }
    constexpr Size        sizeInElements() const { return sizeBytes / sizeof(Type); }
    constexpr Size        sizeInBytes() const { return sizeBytes; }
    constexpr void        setSizeInBytes(Size newSize) { sizeBytes = newSize; }

  private:
    Type* items;
    Size  sizeBytes;
    template <typename O>
    friend struct SpanVoid;
};

template <typename Type>
struct SpanVoid
{
    typedef typename SameConstnessAs<Type, uint8_t>::type ByteType;

    typedef SC::size_t Size;
    template <typename OtherType>
    constexpr SpanVoid(Span<OtherType> other) : items(other.items), size(other.sizeBytes)
    {}
    constexpr SpanVoid() : items(nullptr), size(0) {}
    constexpr SpanVoid(Type* items, Size sizeInBytes) : items(items), size(sizeInBytes) {}

    constexpr Type*       data() { return items; }
    constexpr const Type* data() const { return items; }
    constexpr Size        sizeInBytes() const { return size; }

    template <typename DestinationType>
    constexpr Span<DestinationType> castTo()
    {
        return Span<DestinationType>(static_cast<DestinationType*>(items), sizeInBytes());
    }

    [[nodiscard]] constexpr bool viewAtBytes(Size offsetInBytes, Size lengthInBytes, SpanVoid& other)
    {
        if (offsetInBytes + lengthInBytes <= size)
        {
            other = SpanVoid(static_cast<ByteType*>(items) + offsetInBytes, lengthInBytes);
            return true;
        }
        return false;
    }

  protected:
    Type* items;
    Size  size;
};

} // namespace SC
