// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Algorithms/AlgorithmFind.h" // contains
#include "../Containers/Vector.h"                   // ObjectVTable<T>
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
/// \snippet Tests/Libraries/Containers/ArrayTest.cpp ArraySnippet
template <typename T, int N>
struct Array : public Segment<detail::ArrayVTable<T, N>>
{
    using Parent = Segment<detail::ArrayVTable<T, N>>;
    Array() : Parent(sizeof(T) * N) {};
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
    /// @see Algorithms::contains
    template <typename U>
    [[nodiscard]] bool contains(const U& value, size_t* index = nullptr) const
    {
        return Algorithms::contains(*this, value, index);
    }

    /// @brief Finds the first item in array matching criteria given by the lambda
    /// @see Algorithms::findIf
    template <typename Lambda>
    [[nodiscard]] bool find(Lambda&& lambda, size_t* index = nullptr) const
    {
        return Algorithms::findIf(Parent::begin(), Parent::end(), move(lambda), index) != Parent::end();
    }

    /// @brief Removes all items matching criteria given by Lambda
    /// @see Algorithms::removeIf
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
    /// @see Algorithms::removeIf
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
