// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Algorithms/AlgorithmRemove.h" // removeIf
#include "../Foundation/Internal/Segment.inl"
#include "../Foundation/Internal/SegmentTrivial.inl"
#include "../Foundation/TypeTraits.h" // IsTriviallyCopyable

namespace SC
{
namespace detail
{

template <typename T, bool isTrivial = TypeTraits::IsTriviallyCopyable<T>::value>
struct SegmentVTable : public SegmentTrivial<T>
{
};

template <typename T>
struct SegmentVTable<T, false>
{
    static void destruct(Span<T> data)
    {
        forEach(data, [](auto, T& item) { item.~T(); });
    }

    static void copyConstructAs(Span<T> data, Span<const T> value)
    {
        const T& single = value[0];
        forEach(data, [&single](auto, T& item) { placementNew(item, single); });
    }

    static void copyConstruct(Span<T> data, const T* src)
    {
        forEach(data, [src](auto idx, T& item) { placementNew(item, src[idx]); });
    }

    static void copyAssign(Span<T> data, const T* src)
    {
        forEach(data, [src](auto idx, T& item) { item = src[idx]; });
    }

    static void moveConstruct(Span<T> data, T* src)
    {
        forEach(data, [src](auto idx, T& item) { placementNew(item, move(src[idx])); });
    }

    static void moveAssign(Span<T> data, T* src)
    {
        forEach(data, [src](auto idx, T& item) { item = move(src[idx]); });
    }

    static void copyInsert(Span<T> headerData, Span<const T> values)
    {
        // This function inserts values at the beginning of headerData
        T*       data = headerData.data();
        const T* src  = values.data();

        const size_t numElements = headerData.sizeInElements();
        const size_t numToInsert = values.sizeInElements();

        if (numElements == 0)
        {
            // All newly inserted elements will be copy-constructed in the uninitialized area
            for (size_t idx = 0; idx < numToInsert; ++idx)
            {
                placementNew(data[idx], src[idx]);
            }
        }
        else
        {
            // Move construct some elements in the not initialized area
            for (size_t idx = numElements; idx < numElements + numToInsert; ++idx)
            {
                // Guard against using slots that are before segment start.
                // In the last loop at end of this scope such slots must be
                // initialized with placement new instead of assignment operator
                if (idx >= numToInsert)
                {
                    placementNew(data[idx], move(data[idx - numToInsert]));
                }
            }

            // Move assign some elements to slots in "post-move" state left from previous loop
            for (size_t idx = numElements - 1; idx >= numToInsert; --idx)
            {
                if (idx >= numToInsert)
                {
                    data[idx] = move(data[idx - numToInsert]);
                }
            }

            // Copy assign source data to slots in "post-move" state left from previous loop
            for (size_t idx = 0; idx < numToInsert; ++idx)
            {
                // See note in the first loop in this scope to understand use of assignment vs. placement new
                if (idx < numElements)
                {
                    data[idx] = src[idx];
                }
                else
                {
                    placementNew(data[idx], src[idx]);
                }
            }
        }
    }

    static void remove(Span<T> headerData, size_t numToRemove)
    {
        T* data = headerData.data();

        const size_t numElements = headerData.sizeInElements();

        for (size_t idx = 0; idx < numElements - numToRemove; ++idx)
        {
            data[idx] = move(data[idx + numToRemove]);
        }
        for (size_t idx = numElements - numToRemove; idx < numElements; ++idx)
        {
            data[idx].~T();
        }
    }

  private:
    template <typename Lambda>
    static void forEach(Span<T> data, Lambda&& lambda)
    {
        const size_t numElements = data.sizeInElements();
        T*           elements    = data.data();
        for (size_t idx = 0; idx < numElements; ++idx)
        {
            lambda(idx, elements[idx]);
        }
    }
};

// Executes operations using trivial or non trivial variations of the move/copy/construct functions.
// Trivial check cannot be a template param of this class because it would cause Vector to need
// the entire definition of T to declare Vector<T>, making it impossible to create "recursive" structures
// like SC::Build::WriteInternal::RenderGroup (that has a "Vector<RenderGroup> children" as member )
template <typename T>
struct ObjectVTable
{
    using Type = T;
    static void destruct(Span<T> data) { SegmentVTable<T>::destruct(data); }
    static void copyConstructAs(Span<T> data, Span<const T> value) { SegmentVTable<T>::copyConstructAs(data, value); }
    static void copyConstruct(Span<T> data, const T* src) { SegmentVTable<T>::copyConstruct(data, src); }
    static void copyAssign(Span<T> data, const T* src) { SegmentVTable<T>::copyAssign(data, src); }
    static void moveConstruct(Span<T> data, T* src) { SegmentVTable<T>::moveConstruct(data, src); }
    static void moveAssign(Span<T> data, T* src) { SegmentVTable<T>::moveAssign(data, src); }
    static void copyInsert(Span<T> data, Span<const T> values) { SegmentVTable<T>::copyInsert(data, values); }
    static void remove(Span<T> data, size_t numElements) { SegmentVTable<T>::remove(data, numElements); }
};

template <typename T>
struct VectorVTable : public ObjectVTable<T>, public SegmentSelfRelativePointer<T>
{
    static constexpr bool IsArray = false;
};
} // namespace detail

//! @defgroup group_containers Containers
//! @copybrief library_containers (see @ref library_containers for more details)

//! @addtogroup group_containers
//! @{

/// @brief A contiguous sequence of heap allocated elements
/// @tparam T Type of single vector element
///
/// All methods of SC::Vector that can fail, return a `[[nodiscard]]` `bool` (example SC::Vector::push_back). @n
/// Assignment and Copy / move construct operators will just assert as they can't return a failure code. @n
/// `memcpy` is used to optimize copies when `T` is a memcpy-able object.
/// @note Use SC::SmallVector everywhere a SC::Vector reference is needed if the upper bound size of required elements
/// is known to get rid of unnecessary heap allocations.
///
/// \snippet Libraries/Containers/Tests/VectorTest.cpp VectorSnippet
template <typename T>
struct Vector : public Segment<detail::VectorVTable<T>>
{
    using Parent = Segment<detail::VectorVTable<T>>;

    // Inherits all constructors from Segment
    using Parent::Parent;

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

        const size_t numElements = static_cast<size_t>(itEnd - it);
        const size_t offset      = static_cast<size_t>(it - itBeg);
        detail::VectorVTable<T>::destruct({Parent::data() + offset, numElements});
        Parent::header.sizeBytes -= static_cast<decltype(Parent::header.sizeBytes)>(numElements * sizeof(T));
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

/// @brief A Vector that can hold up to `N` elements inline and `> N` on heap
/// @tparam T Type of single vector element
/// @tparam N Number of elements kept inline to avoid heap allocation
///
/// SC::SmallVector is like SC::Vector but it will do heap allocation once more than `N` elements are needed. @n
/// When the `size()` becomes less than `N` the container will switch back using memory coming from inline storage.
/// @note SC::SmallVector derives from SC::Vector and it can be passed everywhere a reference to SC::Vector is needed.
/// It can be used to get rid of unnecessary allocations where the upper bound of required elements is known or it can
/// be predicted.
///
/// \snippet Libraries/Containers/Tests/SmallVectorTest.cpp SmallVectorSnippet
template <typename T, int N>
struct SmallVector : public Vector<T>
{
    // clang-format off
    SmallVector() : Vector<T>( N * sizeof(T)) {}
    ~SmallVector() {}
    SmallVector(const SmallVector& other) : SmallVector() { Vector<T>::operator=(other); }
    SmallVector(SmallVector&& other) : SmallVector() { Vector<T>::operator=(move(other)); }
    SmallVector& operator=(const SmallVector& other) { Vector<T>::operator=(other); return *this; }
    SmallVector& operator=(SmallVector&& other) { Vector<T>::operator=(move(other)); return *this; }

    SmallVector(const Vector<T>& other) : SmallVector() { Vector<T>::operator=(other); }
    SmallVector(Vector<T>&& other) : SmallVector() { Vector<T>::operator=(move(other)); }
    SmallVector(std::initializer_list<T> list) : SmallVector() { SC_ASSERT_RELEASE(Vector<T>::assign({list.begin(), list.size()})); }
    // clang-format on

  private:
    uint64_t inlineCapacity = N * sizeof(T);
    union
    {
        T inlineData[N];
    };
};
//! @}

} // namespace SC
