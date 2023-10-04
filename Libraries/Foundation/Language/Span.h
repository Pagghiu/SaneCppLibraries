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

    constexpr Span() : items(nullptr), sizeElements(0) {}
    constexpr Span(Type* items, SizeType sizeInElements) : items(items), sizeElements(sizeInElements) {}
    constexpr Span(Type& type) : items(&type), sizeElements(1) {}

    // Specialization for converting const char* to StringView
    constexpr Span(std::initializer_list<Type> ilist) : items(nullptr), sizeElements(0)
    {
        // We need this two step initialization to avoid warnings on all compilers
        items        = ilist.begin();
        sizeElements = ilist.size();
    }

    template <typename T>
    [[nodiscard]] static Span<Type> reinterpret_object(T& value)
    {
        return {reinterpret_cast<Type*>(&value), sizeof(T) / sizeof(Type)};
    }

    [[nodiscard]] static Span reinterpret_bytes(VoidType* items, SizeType sizeInBytes)
    {
        return Span(reinterpret_cast<Type*>(items), sizeInBytes / sizeof(Type));
    }

    [[nodiscard]] constexpr const Type* begin() const { return items; }
    [[nodiscard]] constexpr const Type* end() const { return items + sizeElements; }
    [[nodiscard]] constexpr const Type* data() const { return items; }

    [[nodiscard]] constexpr Type* begin() { return items; }
    [[nodiscard]] constexpr Type* end() { return items + sizeElements; }
    [[nodiscard]] constexpr Type* data() { return items; }

    [[nodiscard]] constexpr SizeType sizeInElements() const { return sizeElements; }
    [[nodiscard]] constexpr SizeType sizeInBytes() const { return sizeElements * sizeof(Type); }

    [[nodiscard]] constexpr bool sliceStart(SizeType offsetInElements, Span& other) const
    {
        if (offsetInElements <= sizeInElements())
        {
            other = Span(items + offsetInElements, (sizeInElements() - offsetInElements));
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool sliceStartLength(SizeType offsetInElements, SizeType lengthInElements,
                                                  Span& other) const
    {
        if (offsetInElements + lengthInElements <= sizeInElements())
        {
            other = Span(items + offsetInElements, lengthInElements);
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr Span<const Type> asConst() const { return {items, sizeElements}; }

    [[nodiscard]] constexpr bool empty() const { return sizeElements == 0; }

  private:
    Type*    items;
    SizeType sizeElements;
    template <typename O>
    friend struct SpanVoid;
};
