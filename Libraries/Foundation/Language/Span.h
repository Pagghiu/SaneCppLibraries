// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/InitializerList.h"
#include "../Language/MetaProgramming.h" // SameConstnessAs

namespace SC
{
template <typename Type>
struct SpanVoid;

template <typename Type>
struct Span;
} // namespace SC

template <typename Type>
struct SC::Span
{
    using Size = size_t;

    constexpr Span() : items(nullptr), sizeBytes(0) {}
    constexpr Span(Type* items, Size sizeInBytes) : items(items), sizeBytes(sizeInBytes) {}
    constexpr Span(Type& type) : items(&type), sizeBytes(sizeof(Type)) {}
    constexpr Span(Type&& type) : items(&type), sizeBytes(sizeof(Type)) {}

    // Specialization for converting const char* to StringView
    constexpr Span(std::initializer_list<Type> ilist) : items(nullptr), sizeBytes(0)
    {
        // We need this two step initialization to avoid warnings on all compilers
        items     = ilist.begin();
        sizeBytes = ilist.size() * sizeof(Type);
    }

    [[nodiscard]] constexpr const Type* begin() const { return items; }
    [[nodiscard]] constexpr const Type* end() const { return items + sizeBytes / sizeof(Type); }
    [[nodiscard]] constexpr const Type* data() const { return items; }

    [[nodiscard]] constexpr Type* begin() { return items; }
    [[nodiscard]] constexpr Type* end() { return items + sizeBytes / sizeof(Type); }
    [[nodiscard]] constexpr Type* data() { return items; }

    [[nodiscard]] constexpr Size sizeInElements() const { return sizeBytes / sizeof(Type); }
    [[nodiscard]] constexpr Size sizeInBytes() const { return sizeBytes; }

    [[nodiscard]] constexpr bool sliceStart(Size offsetInElements, Span& other) const
    {
        if (offsetInElements <= sizeInElements())
        {
            other = Span(items + offsetInElements, (sizeInElements() - offsetInElements) * sizeof(Type));
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool sliceStartLength(Size offsetInElements, Size lengthInElements, Span& other) const
    {
        if (offsetInElements + lengthInElements <= sizeInElements())
        {
            other = Span(items + offsetInElements, lengthInElements * sizeof(Type));
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr Span<const Type> asConst() const { return {items, sizeBytes}; }

    [[nodiscard]] constexpr bool empty() const { return sizeBytes == 0; }

  private:
    Type* items;
    Size  sizeBytes;
    template <typename O>
    friend struct SpanVoid;
};

template <typename Type>
struct SC::SpanVoid
{
    using ByteType = typename SameConstnessAs<Type, uint8_t>::type;

    using Size = SC::size_t;
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
