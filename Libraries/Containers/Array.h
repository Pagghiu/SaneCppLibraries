// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Vector.h" // ObjectVTable<T>

namespace SC
{
namespace detail
{
template <typename T, int N>
struct ArrayVTable : public ObjectVTable<T>
{
    static constexpr bool IsArray = true;

    T*       data() { return items; }
    const T* data() const { return items; }
    void     setData(T*) {}
    T*       getInlineData() { return items; }
    uint32_t getInlineCapacity() { return header.capacityBytes; }

    static constexpr bool isInline() { return true; }

    ArrayVTable(uint32_t capacity = 0, SegmentAllocator allocator = SegmentAllocator::Global)
        : header(capacity, allocator)
    {}
    ~ArrayVTable() {}

    SegmentHeader header;
    union
    {
        T items[N];
    };
};

} // namespace detail
//! @addtogroup group_containers
//! @{

/// @brief A contiguous sequence of elements kept inside its inline storage
/// @tparam T Type of single element of the Array
/// @tparam N Number of elements contained inside this Array inline storage
///
/// SC::Array is like a SC::Vector but it will only allow up to `N` elements in the array, using inline storage, without
/// resorting to heap allocation. @n
/// Trying to push or insert more than N elements will fail. @n
/// Only up to SC::Array::size elements are valid (and `N` - `size()` are initialized).
///
/// \snippet Libraries/Containers/Tests/ArrayTest.cpp ArraySnippet
template <typename T, int N>
struct Array : public Segment<detail::ArrayVTable<T, N>>
{
    using Parent = Segment<detail::ArrayVTable<T, N>>;
    Array() : Parent(sizeof(T) * N){};
    // clang-format off
    Array(std::initializer_list<T> list) : Array() { SC_ASSERT_RELEASE(Parent::template assign<T>({list.begin(), list.size()})); }
    Array(const Array& other) : Array() { SC_ASSERT_RELEASE(Parent::append(other.toSpanConst())); }
    Array(Array&& other) : Array() { SC_ASSERT_RELEASE(Parent::appendMove(move(other))); }
    Array& operator=(const Array& other) { SC_ASSERT_RELEASE(Parent::assign(other.toSpanConst())); return *this; }
    Array& operator=(Array&& other) { SC_ASSERT_RELEASE(Parent::assignMove(move(other))); return *this; }
    template <int M>
    Array(const Array<T, M>& other) : Array() { SC_ASSERT_RELEASE(Parent::assign(other.toSpanConst()));}
    template <int M>
    Array(Array<T, M>&& other) : Array() { SC_ASSERT_RELEASE(Parent::assignMove(move(other))); }
    template <int M>
    Array& operator=(const Array<T, M>& other) { SC_ASSERT_RELEASE(Parent::assign(other.toSpanConst())); return *this; }
    template <int M>
    Array& operator=(Array<T, M>&& other) { SC_ASSERT_RELEASE(Parent::assignMove(move(other))); return *this; }
    Array(Span<const T> span) : Array() { SC_ASSERT_RELEASE(Parent::assign(span)); }
    template <typename U>
    Array(Span<const U> span) : Array() { SC_ASSERT_RELEASE(Parent::assign(span)); }
    // clang-format on

    /// @brief Check if the current array contains a given value.
    /// @tparam U Type of the object being searched
    /// @param value Value being searched
    /// @param index if passed in != `nullptr`, receives index where item was found.
    /// Only written if function returns `true`
    /// @return `true` if the array contains the given value.
    template <typename U>
    [[nodiscard]] bool contains(const U& value, size_t* index = nullptr) const
    {
        return Parent::toSpanConst().contains(value, index);
    }

    /// @brief Finds the first item in array matching criteria given by the lambda
    /// @tparam Lambda Type of the Lambda passed that declares a `bool operator()(const T&)` operator
    /// @param lambda The functor or lambda called that evaluates to `true` when item is found
    /// @param index if passed in != `nullptr`, receives index where item was found.
    /// @return `true` if the wanted value with given criteria is found.
    template <typename Lambda>
    [[nodiscard]] bool find(Lambda&& lambda, size_t* index = nullptr) const
    {
        return Parent::toSpanConst().find(move(lambda), index);
    }

    /// @brief Removes all items matching criteria given by Lambda
    /// @tparam Lambda Type of the functor/lambda with a `bool operator()(const T&)` operator
    /// @param criteria The lambda/functor passed in
    /// @return `true` if at least one item has been removed
    template <typename Lambda>
    [[nodiscard]] bool removeAll(Lambda&& criteria)
    {
        T* itBeg = Parent::begin();
        T* itEnd = Parent::end();
        T* it    = Algorithms::removeIf(itBeg, itEnd, forward<Lambda>(criteria));

        const size_t numBytes = static_cast<size_t>(itEnd - it) * sizeof(T);
        const size_t offBytes = static_cast<size_t>(it - itBeg) * sizeof(T);
        detail::VectorVTable<T>::destruct(Parent::getData(), offBytes, numBytes);
        Parent::header.sizeBytes -= static_cast<decltype(Parent::header.sizeBytes)>(numBytes);
        return it != itEnd;
    }

    /// @brief Removes all values equal to `value`
    /// @tparam U Type of the Value
    /// @param value Value to be removed
    /// @return `true` if at least one item has been removed
    template <typename U>
    [[nodiscard]] bool remove(const U& value)
    {
        return removeAll([&](auto& item) { return item == value; });
    }
};

//! @}

// Allows using this type across Plugin boundaries
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 64>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 128 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 255 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 512 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 1024 * sizeof(native_char_t)>;
} // namespace SC
