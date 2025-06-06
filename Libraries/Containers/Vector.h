// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Algorithms/AlgorithmRemove.h"           // removeIf
#include "../Foundation/Internal/Segment.inl"        // IWYU pragma: keep
#include "../Foundation/Internal/SegmentTrivial.inl" // IWYU pragma: keep
#include "../Foundation/TypeTraits.h"                // IsTriviallyCopyable

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
    static void destruct(Span<T> data) noexcept
    {
        forEach(data, [](auto, T& item) { item.~T(); });
    }

    template <typename U>
    static void copyConstructAs(Span<T> data, Span<const U> value) noexcept
    {
        const U& single = value[0];
        forEach(data, [&single](auto, T& item) { placementNew(item, single); });
    }

    template <typename U>
    static void copyConstruct(Span<T> data, const U* src) noexcept
    {
        forEach(data, [src](auto idx, T& item) { placementNew(item, src[idx]); });
    }

    template <typename U>
    static void copyAssign(Span<T> data, const U* src) noexcept
    {
        forEach(data, [src](auto idx, T& item) { item = src[idx]; });
    }

    template <typename U>
    static void moveConstruct(Span<T> data, U* src) noexcept
    {
        forEach(data, [src](auto idx, T& item) { placementNew(item, move(src[idx])); });
    }

    template <typename U>
    static void moveAssign(Span<T> data, U* src) noexcept
    {
        forEach(data, [src](auto idx, T& item) { item = move(src[idx]); });
    }

    template <typename U>
    static void copyInsert(Span<T> headerData, Span<const U> values) noexcept
    {
        // This function inserts values at the beginning of headerData
        T*       data = headerData.data();
        const U* src  = values.data();

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

    static void remove(Span<T> headerData, size_t numToRemove) noexcept
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
    static void forEach(Span<T> data, Lambda&& lambda) noexcept
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
    static void destruct(Span<T> data) noexcept { SegmentVTable<T>::destruct(data); }
    // clang-format off
    template <typename U> static void copyConstructAs(Span<T> data, Span<const U> value) noexcept { SegmentVTable<T>::template copyConstructAs<U>(data, value);}
    template <typename U> static void copyConstruct(Span<T> data, const U* src) noexcept { SegmentVTable<T>::template copyConstruct<U>(data, src);}
    template <typename U> static void copyAssign(Span<T> data, const U* src) noexcept { SegmentVTable<T>::template copyAssign<U>(data, src);}
    template <typename U> static void moveConstruct(Span<T> data, U* src) noexcept { SegmentVTable<T>::template moveConstruct<U>(data, src);}
    template <typename U> static void moveAssign(Span<T> data, U* src) noexcept { SegmentVTable<T>::template moveAssign<U>(data, src);}
    template <typename U> static void copyInsert(Span<T> data, Span<const U> values) noexcept { SegmentVTable<T>::template copyInsert<U>(data, values);}
    // clang-format on
    static void remove(Span<T> data, size_t numElements) noexcept { SegmentVTable<T>::remove(data, numElements); }
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
/// \snippet Tests/Libraries/Containers/VectorTest.cpp VectorSnippet
template <typename T>
struct Vector : public Segment<detail::VectorVTable<T>>
{
    using Parent = Segment<detail::VectorVTable<T>>;

    // Inherits all constructors from Segment
    using Parent::Parent;

    /// @brief Check if the current array contains a given value.
    /// @see Algorithms::contains
    template <typename U>
    [[nodiscard]] bool contains(const U& value, size_t* index = nullptr) const noexcept
    {
        return Algorithms::contains(*this, value, index);
    }

    /// @brief Finds the first item in array matching criteria given by the lambda
    /// @see Algorithms::findIf
    template <typename Lambda>
    [[nodiscard]] bool find(Lambda&& lambda, size_t* index = nullptr) const noexcept
    {
        return Algorithms::findIf(Parent::begin(), Parent::end(), move(lambda), index) != Parent::end();
    }

    /// @brief Removes all items matching criteria given by Lambda
    /// @see Algorithms::removeIf
    template <typename Lambda>
    [[nodiscard]] bool removeAll(Lambda&& criteria) noexcept
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
    [[nodiscard]] bool remove(const U& value) noexcept
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
/// \snippet Tests/Libraries/Containers/SmallVectorTest.cpp SmallVectorSnippet
template <typename T, int N>
struct SmallVector : public Vector<T>
{
    // clang-format off
    SmallVector(SegmentAllocator allocator = SegmentAllocator::Global) noexcept : Vector<T>( N * sizeof(T), allocator) {}
    ~SmallVector() noexcept {}
    SmallVector(const SmallVector& other) noexcept : SmallVector() { Vector<T>::operator=(other); }
    SmallVector(SmallVector&& other) noexcept : SmallVector() { Vector<T>::operator=(move(other)); }
    SmallVector& operator=(const SmallVector& other) noexcept { Vector<T>::operator=(other); return *this; }
    SmallVector& operator=(SmallVector&& other) noexcept { Vector<T>::operator=(move(other)); return *this; }

    SmallVector(const Vector<T>& other) noexcept : SmallVector() { Vector<T>::operator=(other); }
    SmallVector(Vector<T>&& other) noexcept : SmallVector() { Vector<T>::operator=(move(other)); }
    SmallVector(std::initializer_list<T> list) noexcept : SmallVector() { SC_ASSERT_RELEASE(Vector<T>::assign({list.begin(), list.size()})); }
    // clang-format on
  protected:
    SmallVector(int num, SegmentAllocator allocator) : Vector<T>(N, allocator) { (void)num; }

  private:
    uint64_t inlineCapacity = N * sizeof(T);
    union
    {
        T inlineData[N];
    };
};

template <typename T>
using VectorTL = detail::SegmentCustom<Vector<T>, Vector<T>, 0, SegmentAllocator::ThreadLocal>;
template <typename T, int N>
using SmallVectorTL = detail::SegmentCustom<SmallVector<T, N>, Vector<T>, N, SegmentAllocator::ThreadLocal>;

//! @}

} // namespace SC
