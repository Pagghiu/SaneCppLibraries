// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/InitializerList.h"
#include "../Language/MetaProgramming.h" // SameConstnessAs

namespace SC
{
template <typename Type>
struct Span;
} // namespace SC

template <typename Type>
struct SC::Span
{
    using SizeType = size_t;
    using VoidType = typename SameConstnessAs<Type, void>::type;

    constexpr Span() : items(nullptr), sizeBytes(0) {}
    constexpr Span(Type* items, SizeType sizeInBytes) : items(items), sizeBytes(sizeInBytes) {}
    constexpr Span(Type& type) : items(&type), sizeBytes(sizeof(Type)) {}
    constexpr Span(Type&& type) : items(&type), sizeBytes(sizeof(Type)) {}
    Span(VoidType* items, SizeType sizeInBytes) : items(reinterpret_cast<Type*>(items)), sizeBytes(sizeInBytes) {}

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

    [[nodiscard]] constexpr SizeType sizeInElements() const { return sizeBytes / sizeof(Type); }
    [[nodiscard]] constexpr SizeType sizeInBytes() const { return sizeBytes; }

    [[nodiscard]] constexpr bool sliceStart(SizeType offsetInElements, Span& other) const
    {
        if (offsetInElements <= sizeInElements())
        {
            other = Span(items + offsetInElements, (sizeInElements() - offsetInElements) * sizeof(Type));
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool sliceStartLength(SizeType offsetInElements, SizeType lengthInElements,
                                                  Span& other) const
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

    template <typename T>
    [[nodiscard]] static Span<Type> reinterpret_span(T& value)
    {
        return {reinterpret_cast<Type*>(&value), sizeof(T) / sizeof(Type)};
    }

  private:
    Type*    items;
    SizeType sizeBytes;
    template <typename O>
    friend struct SpanVoid;
};
