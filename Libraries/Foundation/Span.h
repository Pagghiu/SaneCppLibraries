// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/InitializerList.h"
#include "../Foundation/LibC.h"       // memcmp
#include "../Foundation/TypeTraits.h" // SameConstnessAs

namespace SC
{
template <typename Type>
struct Span;
} // namespace SC

//! @addtogroup group_foundation_utility
//! @{

/// @brief View over a contiguous sequence of items (pointer + size in elements).
/// @tparam Type Any type
template <typename Type>
struct SC::Span
{
    using SizeType = size_t;
    using VoidType = typename TypeTraits::SameConstnessAs<Type, void>::type;

    template <size_t N>
    constexpr Span(Type (&_items)[N]) : items(_items), sizeElements(N)
    {}

    /// @brief Builds an empty Span
    constexpr Span() : items(nullptr), sizeElements(0) {}

    /// @brief Builds a Span from an array
    /// @param items  pointer to the first member of the array
    /// @param sizeInElements number of elements in in the array
    constexpr Span(Type* items, SizeType sizeInElements) : items(items), sizeElements(sizeInElements) {}

    /// @brief Builds a Span from a single object
    /// @param type A reference to a single object of type Type
    constexpr Span(Type& type) : items(&type), sizeElements(1) {}

    /// @brief Span specialized constructor (mainly used for converting const char* to StringView)
    /// @param list an initializer list of elements
    constexpr Span(std::initializer_list<Type> list) : items(nullptr), sizeElements(0)
    {
        // We need this two step initialization to avoid warnings on all compilers
        items        = list.begin();
        sizeElements = list.size();
    }

    /// @brief Converts to a span with `const` qualified Type
    operator Span<const Type>() const { return {items, sizeElements}; }

    /// @brief Constructs a Span reinterpreting memory pointed by object of type `T` as a type `Type`
    /// @tparam T Type of object to be reinterpreted
    /// @param value The source object to be reinterpreted
    /// @return The output converted Span object
    template <typename T>
    [[nodiscard]] static Span<Type> reinterpret_object(T& value)
    {
        return {reinterpret_cast<Type*>(&value), sizeof(T) / sizeof(Type)};
    }

    /// @brief Construct a span reinterpreting raw memory (`void*` or `const void*`) to `Type` or `const Type`
    /// @param rawMemory Pointer to raw buffer of memory
    /// @param sizeInBytes Size of the raw buffer in Bytes
    /// @return The reinterpreted Span object
    [[nodiscard]] static Span<Type> reinterpret_bytes(VoidType* rawMemory, SizeType sizeInBytes)
    {
        return Span(reinterpret_cast<Type*>(rawMemory), sizeInBytes / sizeof(Type));
    }

    /// @brief Reinterprets the current span as an array of the specified type
    template <typename T>
    [[nodiscard]] Span<T> reinterpret_as_array_of() const
    {
        return Span<T>(reinterpret_cast<T*>(items), sizeInBytes() / sizeof(T));
    }

    /// @brief Returns pointer to first element of the span
    /// @return pointer to first element of the span
    [[nodiscard]] constexpr const Type* begin() const { return items; }

    /// @brief Returns pointer to one after the last element of the span
    /// @return Pointer to one after the last element of the span
    [[nodiscard]] constexpr const Type* end() const { return items + sizeElements; }

    /// @brief Returns pointer to first element of the span
    /// @return pointer to first element of the span
    [[nodiscard]] constexpr const Type* data() const { return items; }

    /// @brief Returns pointer to first element of the span
    /// @return pointer to first element of the span
    [[nodiscard]] constexpr Type* begin() { return items; }

    /// @brief Returns pointer to one after the last element of the span
    /// @return Pointer to one after the last element of the span
    [[nodiscard]] constexpr Type* end() { return items + sizeElements; }

    /// @brief Returns pointer to first element of the span
    /// @return pointer to first element of the span
    [[nodiscard]] constexpr Type* data() { return items; }

    /// @brief Size of Span in elements
    /// @return The number of elements of the Span
    [[nodiscard]] constexpr SizeType sizeInElements() const { return sizeElements; }

    /// @brief Size of Span in bytes
    /// @return The number of bytes covering the entire Span
    [[nodiscard]] constexpr SizeType sizeInBytes() const { return sizeElements * sizeof(Type); }

    /// @brief Creates another Span, starting at an offset in elements from current Span, until end.
    /// @param offsetInElements Offset in current Span where destination Span will be starting
    /// @param destination Reference to a Span that will hold the resulting computed span
    /// @return
    ///         - `true` if destination has been written.
    ///         - `false` if offsetInElements is bigger to than Span::size().
    [[nodiscard]] constexpr bool sliceStart(SizeType offsetInElements, Span& destination) const
    {
        if (offsetInElements <= sizeInElements())
        {
            destination = Span(items + offsetInElements, (sizeInElements() - offsetInElements));
            return true;
        }
        return false;
    }

    /// @brief Creates another Span, starting at an offset in elements from current Span of specified length.
    /// @param offsetInElements Offset in current Span where destination Span will be starting
    /// @param lengthInElements Number of elements wanted for destination Span
    /// @param destination Reference to a Span that will hold the resulting computed span
    /// @return
    ///         - `true` if destination has been written.
    ///         - `false` if (offsetInElements + lengthInElements) is bigger to than Span::size().
    [[nodiscard]] constexpr bool sliceStartLength(SizeType offsetInElements, SizeType lengthInElements,
                                                  Span& destination) const
    {
        if (offsetInElements + lengthInElements <= sizeInElements())
        {
            destination = Span(items + offsetInElements, lengthInElements);
            return true;
        }
        return false;
    }

    /// @brief Check if Span is empty
    /// @return `true` if Span is empty
    [[nodiscard]] constexpr bool empty() const { return sizeElements == 0; }

    [[nodiscard]] constexpr bool contains(const Type& type, SizeType* index = nullptr) const
    {
        for (SizeType idx = 0; idx < sizeElements; ++idx)
        {
            if (items[idx] == type)
            {
                if (index)
                {
                    *index = idx;
                }
                return true;
            }
        }
        return false;
    }

    Type& operator[](SizeType idx) { return items[idx]; }

    const Type& operator[](SizeType idx) const { return items[idx]; }

    /// @brief Gets the item at given index or nullptr if index is negative or bigger than size
    template <typename IntType>
    Type* get(IntType idx)
    {
        if (idx >= 0 and idx < static_cast<IntType>(sizeElements))
            return items + idx;
        return nullptr;
    }

    /// @brief Gets the item at given index or nullptr if index is negative or bigger than size
    template <typename IntType>
    const Type* get(IntType idx) const
    {
        if (idx >= 0 and idx < static_cast<IntType>(sizeElements))
            return items + idx;
        return nullptr;
    }

    [[nodiscard]] bool equals(const Span other) const
    {
        if (sizeInBytes() != other.sizeInBytes())
            return false;
        if (sizeInBytes() == 0)
            return true;
        return ::memcmp(other.items, other.items, sizeInBytes()) == 0;
    }

  private:
    Type*    items;
    SizeType sizeElements;
};

//! @}
