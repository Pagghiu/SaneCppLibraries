// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Vector.h" // SegmentObject

namespace SC
{
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
template <typename T, int N>
struct Array
{
    /// @brief Constructs an empty Array
    Array()
    {
        header.sizeBytes     = 0;
        header.capacityBytes = N * sizeof(T);

        header.isInlineBuffer           = true;
        header.isFollowedByInlineBuffer = false;
    }

    // clang-format off
    Array(std::initializer_list<T> list) : Array() { SC_ASSERT_RELEASE(assign({list.begin(), list.size()})); }
    ~Array() { ArraySegment().unsafeSetHeader(&header); }
    Array(const Array& other) : Array() { SC_ASSERT_RELEASE(append(other.toSpanConst())); }
    Array(Array&& other) : Array() { SC_ASSERT_RELEASE(appendMove(move(other))); }
    Array& operator=(const Array& other) { SC_ASSERT_RELEASE(assign(other.toSpanConst())); return *this; }
    Array& operator=(Array&& other) { SC_ASSERT_RELEASE(assign(move(other))); return *this; }
    template <int M>
    Array(const Array<T, M>& other) : Array() { SC_ASSERT_RELEASE(assign(other.toSpanConst()));}
    template <int M>
    Array(Array<T, M>&& other) : Array() { SC_ASSERT_RELEASE(assign(move(other))); }
    template <int M>
    Array& operator=(const Array<T, M>& other) { SC_ASSERT_RELEASE(assign(other.toSpanConst())); return *this; }
    template <int M>
    Array& operator=(Array<T, M>&& other) { SC_ASSERT_RELEASE(assign(move(other))); return *this; }
    Array(Span<const T> span) : Array() { SC_ASSERT_RELEASE(assign(span)); }
    template <typename U>
    Array(Span<const U> span) : Array() { SC_ASSERT_RELEASE(assign(span)); }
    // clang-format on

    /// @brief Returns content of the array in a span
    [[nodiscard]] Span<const T> toSpanConst() const { return {data(), size()}; }

    /// @brief Returns content of the array in a span
    [[nodiscard]] Span<T> toSpan() { return {data(), size()}; }

    /// @brief Copies an element in front of the Array, at position 0
    [[nodiscard]] bool push_front(const T& value)
    {
        return invoke([&value](ArraySegment& segment) { return segment.push_front(value); });
    }

    /// @brief Appends an element copying it at the end of the Array
    [[nodiscard]] bool push_back(const T& value)
    {
        return invoke([&value](ArraySegment& segment) { return segment.push_back(value); });
    }

    /// @brief Appends an element moving it at the end of the Array
    [[nodiscard]] bool push_back(T&& value)
    {
        return invoke([&value](ArraySegment& segment) { return segment.push_back(move(value)); });
    }

    /// @brief Removes the last element of the array
    /// @param removedValue The removed value will be returned if != nullptr
    [[nodiscard]] bool pop_back(T* removedValue = nullptr)
    {
        return invoke([&](ArraySegment& segment) { return segment.pop_back(removedValue); });
    }

    /// @brief Removes the first element of the array
    /// @param removedValue The removed value will be returned if != nullptr
    [[nodiscard]] bool pop_front(T* removedValue = nullptr)
    {
        return invoke([&](ArraySegment& segment) { return segment.pop_front(removedValue); });
    }

    /// @brief Reserves memory for newCapacity elements
    [[nodiscard]] bool reserve(size_t newCapacity)
    {
        return invoke([newCapacity](ArraySegment& segment) { return segment.reserve(newCapacity); });
    }

    /// @brief Resizes this array to newSize, preserving existing elements.
    /// @param newSize The wanted new size of the array
    /// @param value a default value that will be used for new elements inserted.
    /// @return `true` if resize succeeded
    [[nodiscard]] bool resize(size_t newSize, const T& value = T())
    {
        return invoke([&](ArraySegment& segment) { return segment.resize(newSize, value); });
    }

    /// @brief Resizes to newSize, preserving existing elements without initializing newly added ones.
    [[nodiscard]] bool resizeWithoutInitializing(size_t newSize)
    {
        return invoke([&](ArraySegment& segment) { return segment.resizeWithoutInitializing(newSize); });
    }

    /// @brief Destroys all elements in the container, making the array empty
    void clear()
    {
        call([](ArraySegment& segment) { return segment.clear(); });
    }

    /// @brief This operation is a no-op on Array.
    [[nodiscard]] bool shrink_to_fit() { return true; }

    /// @brief Inserts a range of items copying them at given index
    /// @param idx Index where to start inserting the range of items
    /// @param data Data that will be inserted  at index `idx` (by copy)
    [[nodiscard]] bool insert(size_t idx, Span<const T> data)
    {
        return invoke([&](ArraySegment& segment) { return segment.insert(idx, data); });
    }

    /// @brief Appends a range of items copying them at the end of array
    [[nodiscard]] bool append(Span<const T> data)
    {
        return invoke([data](ArraySegment& segment) { return segment.append(data); });
    }

    /// @brief Appends another array moving its contents at the end of array
    [[nodiscard]] bool appendMove(Array&& other)
    {
        return invoke(
            [&other](ArraySegment& segment)
            {
                ArraySegment otherSegment;
                otherSegment.unsafeSetHeader(&other.header);
                if (not segment.appendMove(move(otherSegment)))
                {
                    otherSegment.unsafeSetHeader(nullptr);
                    return false;
                }
                return true;
            });
    }

    /// @brief Replaces contents of the array copying elements from the span
    [[nodiscard]] bool assign(Span<const T> data)
    {
        return invoke([&data](ArraySegment& segment) { return segment.assign(data); });
    }

    /// @brief Replaces contents of the array moving all elements from the other array
    template <int M>
    [[nodiscard]] bool assign(Array<T, M>&& other)
    {
        return invoke(
            [&other](ArraySegment& segment)
            {
                ArraySegment otherSegment;
                otherSegment.unsafeSetHeader(&other.unsafeGetHeader());
                if (not segment.assignMove(move(otherSegment)))
                {
                    otherSegment.unsafeSetHeader(nullptr);
                    return false;
                }
                return true;
            });
    }

    /// @brief Returns `true` if the array is empty
    [[nodiscard]] bool isEmpty() const { return header.sizeBytes == 0; }

    /// @brief Returns the size of the array
    [[nodiscard]] size_t size() const { return header.sizeBytes / sizeof(T); }

    /// @brief Returns the capacity of the array
    [[nodiscard]] size_t capacity() const { return header.capacityBytes / sizeof(T); }

    /// @brief Gets pointer to first element of the array (or `nullptr` if empty)
    [[nodiscard]] const T* data() const { return header.sizeBytes > 0 ? items : nullptr; }

    /// @brief Gets pointer to first element of the array (or `nullptr` if empty)
    [[nodiscard]] T* data() { return header.sizeBytes > 0 ? items : nullptr; }

    // clang-format off
    [[nodiscard]] T*       begin() SC_LANGUAGE_LIFETIME_BOUND { return data(); }
    [[nodiscard]] const T* begin() const SC_LANGUAGE_LIFETIME_BOUND { return data(); }
    [[nodiscard]] T*       end() SC_LANGUAGE_LIFETIME_BOUND { return data() + size(); }
    [[nodiscard]] const T* end() const SC_LANGUAGE_LIFETIME_BOUND { return data() + size(); }

    [[nodiscard]] T& back() SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(not isEmpty()); return *(data() + size() - 1);}
    [[nodiscard]] T& front() SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(not isEmpty()); return *data();}
    [[nodiscard]] T& operator[](size_t idx) SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(idx < size()); return *(data() + idx);}
    [[nodiscard]] const T& back() const SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(not isEmpty()); return *(data() + size() - 1);}
    [[nodiscard]] const T& front() const SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(not isEmpty()); return *data();}
    [[nodiscard]] const T& operator[](size_t idx) const SC_LANGUAGE_LIFETIME_BOUND { SC_ASSERT_DEBUG(idx < size()); return *(data() + idx);}
    // clang-format on

    /// @brief Return `true` if array contains value, returning index of found item (if != `nullptr`)
    /// @return `true` if the array contains the given value, if `false` index will not be written
    template <typename U>
    [[nodiscard]] bool contains(const U& value, size_t* index = nullptr) const
    {
        return toSpanConst().contains(value, index);
    }

    /// @brief Finds the first item in array matching criteria given by the lambda
    /// @tparam Lambda Type of the Lambda passed that declares a `bool operator()(const T&)` operator
    /// @param lambda The functor or lambda called that evaluates to `true` when item is found
    /// @param index if passed in != `nullptr`, receives index where item was found.
    /// @return `true` if the wanted value with given criteria is found.
    template <typename Lambda>
    [[nodiscard]] bool find(Lambda&& lambda, size_t* index = nullptr) const
    {
        return toSpanConst().find(move(lambda), index);
    }

    /// @brief Removes an item at a given index
    [[nodiscard]] bool removeAt(size_t index)
    {
        return invoke([=](ArraySegment& segment) { return segment.removeAt(index); });
    }

    /// @brief Removes all items matching criteria by Lambda / Functor with a `bool operator()(const T&)`
    template <typename Lambda>
    [[nodiscard]] bool removeAll(Lambda&& criteria)
    {
        T* itBeg = begin();
        T* itEnd = end();
        T* it    = Algorithms::removeIf(itBeg, itEnd, forward<Lambda>(criteria));

        const size_t numBytes = static_cast<size_t>(itEnd - it) * sizeof(T);
        const size_t offBytes = static_cast<size_t>(it - itBeg) * sizeof(T);
        ArrayVTable::destruct(header, offBytes, numBytes);
        header.sizeBytes -= static_cast<decltype(header.sizeBytes)>(numBytes);
        return it != itEnd;
    }

    /// @brief Removes all values equal to `value`
    template <typename U>
    [[nodiscard]] bool remove(const U& value)
    {
        return removeAll([&](auto& v) { return value == v; });
    }

    [[nodiscard]] SegmentHeader& unsafeGetHeader() { return header; }

  private:
    static_assert(N > 0, "Array must have N > 0");

    template <typename Lambda>
    auto invoke(Lambda&& lambda)
    {
        ArraySegment segment;
        segment.unsafeSetHeader(&header);
        auto res = lambda(segment);
        segment.unsafeSetHeader(nullptr);
        return res;
    }

    template <typename Lambda>
    auto call(Lambda&& lambda)
    {
        ArraySegment segment;
        segment.unsafeSetHeader(&header);
        lambda(segment);
        segment.unsafeSetHeader(nullptr);
    }

    struct ArrayVTable : public Internal::ObjectVTable<T>
    {
        static SegmentHeader* allocateNewHeader(size_t) { return nullptr; }
        static SegmentHeader* reallocateExistingHeader(SegmentHeader& src, size_t newCapacityInBytes)
        {
            return newCapacityInBytes < sizeof(items) ? &src : nullptr;
        }
        static void destroyHeader(SegmentHeader&) {}
    };

    using ArraySegment = Segment<ArrayVTable>;

    SegmentHeader header;
    union
    {
        T items[N];
    };
};

//! @}

// Allows using this type across Plugin boundaries
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 64>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 128 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 255 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 512 * sizeof(native_char_t)>;
SC_COMPILER_EXTERN template struct SC_COMPILER_EXPORT Array<char, 1024 * sizeof(native_char_t)>;
} // namespace SC
