// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/InitializerList.h" // IWYU pragma: keep
#include "../Foundation/LibC.h"            // IWYU pragma: keep
#include "../Foundation/TypeTraits.h"      // SameConstnessAs

namespace SC
{
template <typename Type>
struct Span;

namespace detail
{
// clang-format off
template<typename U> struct SpanSizeOfType              { static constexpr auto size = sizeof(U);  };
template<>           struct SpanSizeOfType<void>        { static constexpr auto size = 1;          };
template<>           struct SpanSizeOfType<const void>  { static constexpr auto size = 1;          };
// clang-format on
} // namespace detail

//! @addtogroup group_foundation_utility
//! @{

/// @brief View over a contiguous sequence of items (pointer + size in elements).
/// @tparam Type Any type
template <typename Type>
struct Span
{
  private:
    // clang-format off
    using SizeType = size_t;
    using VoidType = typename TypeTraits::SameConstnessAs<Type, void>::type;
    template <typename U> using TypeIfNotVoid = typename  TypeTraits::EnableIf<not TypeTraits::IsSame<U, VoidType>::value, Type>::type;
    template <typename U> using TypeInitializerList = typename  TypeTraits::EnableIf<TypeTraits::IsConst<U>::value and not TypeTraits::IsSame<U, VoidType>::value, Type>::type;
    template <typename U> using SameType = TypeTraits::IsSame<typename TypeTraits::RemoveConst<U>::type, typename TypeTraits::RemoveConst<Type>::type>;
    template <typename U> using EnableNotVoid = typename TypeTraits::EnableIf<SameType<U>::value and not TypeTraits::IsSame<U, VoidType>::value, bool>::type;
    // clang-format on
    Type*    items;
    SizeType sizeElements;

  public:
    template <size_t N, typename U = Type, EnableNotVoid<U> = true>
    constexpr Span(U (&itemsArray)[N]) : items(itemsArray), sizeElements(N)
    {}

    /// @brief Builds an empty Span
    constexpr Span() : items(nullptr), sizeElements(0) {}

    /// @brief Builds a Span from an array
    /// @param items  pointer to the first member of the array
    /// @param sizeInElements number of elements in in the array
    constexpr Span(Type* items, SizeType sizeInElements) : items(items), sizeElements(sizeInElements) {}

    /// @brief Builds a Span from a single object
    /// @param type A reference to a single object of type Type
    template <typename U = Type>
    constexpr Span(TypeIfNotVoid<U>& type) : items(&type), sizeElements(1)
    {}

    /// @brief Span specialized constructor (mainly used for converting const char* to StringView)
    /// @param list an initializer list of elements
    template <typename U = Type>
    constexpr Span(std::initializer_list<TypeInitializerList<U>> list) : items(nullptr), sizeElements(0)
    {
        // We need this two step initialization to avoid warnings on all compilers
        items        = list.begin();
        sizeElements = list.size();
    }

    // clang-format off
    template <typename U = Type> operator Span<const TypeIfNotVoid<U>>() const { return {items, sizeElements}; }
    template <typename U = Type> operator Span<      TypeIfNotVoid<U>>()       { return {items, sizeElements}; }
    operator Span<const void>() const { return Span<const void>(items, sizeElements * detail::SpanSizeOfType<Type>::size); }
    operator Span<void>() { return Span<void>(items, sizeElements * detail::SpanSizeOfType<Type>::size); }

    /// @brief Constructs a Span reinterpreting memory pointed by object of type `T` as a type `Type`
    template <typename T> [[nodiscard]] static Span<Type> reinterpret_object(T& value) { return {reinterpret_cast<Type*>(&value), sizeof(T) / detail::SpanSizeOfType<Type>::size}; }

    /// @brief Construct a span reinterpreting raw memory (`void*` or `const void*`) to `Type` or `const Type`
    [[nodiscard]] static Span<Type> reinterpret_bytes(VoidType* rawMemory, SizeType sizeInBytes) { return Span(reinterpret_cast<Type*>(rawMemory), sizeInBytes / detail::SpanSizeOfType<Type>::size); }

    /// @brief Reinterprets the current span as an array of the specified type
    template <typename T> [[nodiscard]] Span<const T> reinterpret_as_span_of() const { return Span<const T>(reinterpret_cast<const T*>(items), sizeInBytes() / sizeof(T)); }

    /// @brief Reinterprets the current span as an array of the specified type
    template <typename T> [[nodiscard]] Span<T> reinterpret_as_span_of() { return Span<T>(reinterpret_cast<T*>(items), sizeInBytes() / sizeof(T)); }

    [[nodiscard]] constexpr const Type* begin() const { return items; }
    [[nodiscard]] constexpr const Type* end() const { return items + sizeElements; }
    [[nodiscard]] constexpr const Type* data() const { return items; }
    [[nodiscard]] constexpr Type* begin() { return items; }
    [[nodiscard]] constexpr Type* end() { return items + sizeElements; }
    [[nodiscard]] constexpr Type* data() { return items; }

    [[nodiscard]] constexpr SizeType sizeInElements() const { return sizeElements; }
    [[nodiscard]] constexpr SizeType sizeInBytes() const { return sizeElements * detail::SpanSizeOfType<Type>::size; }

    [[nodiscard]] constexpr bool empty() const { return sizeElements == 0; }

    template <typename U = Type> TypeIfNotVoid<U>& operator[](SizeType idx) {  return items[idx]; }
    template <typename U = Type> const TypeIfNotVoid<U>& operator[](SizeType idx) const { return items[idx]; }
    // clang-format on

    /// @brief Creates another Span, starting at an offset in elements from current Span, until end.
    /// @param offsetInElements Offset in current Span where destination Span will be starting
    /// @param destination Reference to a Span that will hold the resulting sliced span
    /// @return `false` if offsetInElements is bigger to than Span::sizeInElements().
    [[nodiscard]] constexpr bool sliceStart(SizeType offsetInElements, Span& destination) const
    {
        bool valid  = offsetInElements <= sizeElements;
        destination = valid ? Span(items + offsetInElements, sizeElements - offsetInElements) : Span(nullptr, 0);
        return valid;
    }

    /// @brief Creates another Span, starting at an offset in elements from current Span of specified length.
    /// @param offsetInElements Offset in current Span where destination Span will be starting
    /// @param lengthInElements Number of elements to include in destination Span
    /// @param destination Reference to a Span that will hold the resulting sliced span
    /// @return false` if (offsetInElements + lengthInElements) is bigger to than Span::sizeInElements().
    [[nodiscard]] constexpr bool sliceStartLength(SizeType offsetInElements, SizeType lengthInElements,
                                                  Span& destination) const
    {
        bool valid  = offsetInElements + lengthInElements <= sizeElements;
        destination = valid ? Span(items + offsetInElements, lengthInElements) : Span(nullptr, 0);
        return valid;
    }
};
//! @}

// Allows using this type across Plugin boundaries
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Span<char>;
} // namespace SC
