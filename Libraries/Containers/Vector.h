// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Algorithms/AlgorithmRemove.h" // removeIf
#include "../Foundation/Segment.h"
#include "../Foundation/Segment.inl"
#include "../Foundation/TypeTraits.h" // IsTriviallyCopyable

namespace SC
{
namespace Internal
{

/// @brief Allows SC::Segment handle non trivial types
template <typename T>
struct SegmentNonTrivial
{
    static void destruct(SegmentHeader& header, size_t bytesOffset, size_t numBytes)
    {
        forEach(header, bytesOffset, numBytes, [](auto, T& item) { item.~T(); });
    }
    static void copyConstructSingle(SegmentHeader& header, size_t bytesOffset, const T* value, size_t numBytes, size_t)
    {
        forEach(header, bytesOffset, numBytes, [value](auto, T& item) { placementNew(item, *value); });
    }
    static void copyConstruct(SegmentHeader& dest, size_t bytesOffset, const T* src, size_t numBytes)
    {
        forEach(dest, bytesOffset, numBytes, [src](auto idx, T& item) { placementNew(item, src[idx]); });
    }
    static void copyAssign(SegmentHeader& dest, size_t bytesOffset, const T* src, size_t numBytes)
    {
        forEach(dest, bytesOffset, numBytes, [src](auto idx, T& item) { item = src[idx]; });
    }
    static void moveConstruct(SegmentHeader& dest, size_t bytesOffset, T* src, size_t numBytes)
    {
        forEach(dest, bytesOffset, numBytes, [src](auto idx, T& item) { placementNew(item, move(src[idx])); });
    }
    static void moveAssign(SegmentHeader& dest, size_t bytesOffset, T* src, size_t numBytes)
    {
        forEach(dest, bytesOffset, numBytes, [src](auto idx, T& item) { item = move(src[idx]); });
    }

    static void copyInsert(SegmentHeader& dest, size_t startOffsetBytes, const T* src, size_t numBytes)
    {
        T* data = getData(dest, 0);

        const size_t numElements    = dest.sizeBytes / sizeof(T);
        const size_t numToInsert    = numBytes / sizeof(T);
        const size_t insertStartIdx = startOffsetBytes / sizeof(T);
        const size_t insertEndIdx   = insertStartIdx + numToInsert;

        if (insertStartIdx == numElements)
        {
            // If the newly inserted elements fall in the uninitialized area, copy construct them
            for (size_t idx = numElements; idx < numElements + numToInsert; ++idx)
            {
                placementNew(data[idx], src[idx - numElements]);
            }
        }
        else
        {
            // Move construct some elements in the not initialized area
            for (size_t idx = numElements; idx < numElements + numToInsert; ++idx)
            {
                // Guard against using slots that are before segment start.
                // In the last loop at end of this scope such slots must be
                // initialized with placement new instead of assignemnt operator
                if (idx >= numToInsert)
                {
                    placementNew(data[idx], move(data[idx - numToInsert]));
                }
            }

            // Move assign some elements to slots in "post-move" state left from previous loop
            for (size_t idx = numElements - 1; idx >= insertStartIdx + numToInsert; --idx)
            {
                if (idx >= numToInsert)
                {
                    data[idx] = move(data[idx - numToInsert]);
                }
            }

            // Copy assign source data to slots in "post-move" state left from previous loop
            for (size_t idx = insertStartIdx; idx < insertEndIdx; ++idx)
            {
                // See note in the first loop in this scope to understand use of assignment vs. placement new
                if (idx < numElements)
                {
                    data[idx] = src[idx - insertStartIdx];
                }
                else
                {
                    placementNew(data[idx], src[idx - insertStartIdx]);
                }
            }
        }
    }

    static void remove(SegmentHeader& dest, size_t fromBytesOffset, size_t toBytesOffset)
    {
        T* data = getData(dest, 0);

        const size_t numElements = dest.sizeBytes / sizeof(T);
        const size_t startIdx    = fromBytesOffset / sizeof(T);
        const size_t endIdx      = toBytesOffset / sizeof(T);
        const size_t numToRemove = (endIdx - startIdx);

        for (size_t idx = startIdx; idx < numElements - numToRemove; ++idx)
        {
            data[idx] = move(data[idx + numToRemove]);
        }
        for (size_t idx = numElements - numToRemove; idx < numElements; ++idx)
        {
            data[idx].~T();
        }
    }

  private:
    static T* getData(SegmentHeader& header, size_t byteOffset) { return header.getData<T>() + byteOffset / sizeof(T); }

    static size_t getSize(SegmentHeader& header) { return header.sizeBytes / sizeof(T); }

    template <typename Lambda>
    static void forEach(SegmentHeader& header, size_t byteOffset, size_t numBytes, Lambda&& lambda)
    {
        T*           data        = getData(header, byteOffset);
        const size_t numElements = numBytes / sizeof(T);
        for (size_t idx = 0; idx < numElements; ++idx)
        {
            lambda(idx, data[idx]);
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
    static void destruct(SegmentHeader& header, size_t bytesOffset, size_t numBytes)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            SegmentTrivial::destruct(header, bytesOffset, numBytes);
        else
            SegmentNonTrivial<T>::destruct(header, bytesOffset, numBytes);
    }

    static void copyConstructSingle(SegmentHeader& header, size_t bytesOffset, const T* value, size_t numBytes,
                                    size_t sizeOfValue)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            SegmentTrivial::copyConstructSingle(header, bytesOffset, value, numBytes, sizeOfValue);
        else
            SegmentNonTrivial<T>::copyConstructSingle(header, bytesOffset, value, numBytes, sizeOfValue);
    }

    static void copyConstruct(SegmentHeader& dest, size_t bytesOffset, const T* src, size_t numBytes)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            SegmentTrivial::copyConstruct(dest, bytesOffset, src, numBytes);
        else
            SegmentNonTrivial<T>::copyConstruct(dest, bytesOffset, src, numBytes);
    }

    static void copyAssign(SegmentHeader& dest, size_t bytesOffset, const T* src, size_t numBytes)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            SegmentTrivial::copyAssign(dest, bytesOffset, src, numBytes);
        else
            SegmentNonTrivial<T>::copyAssign(dest, bytesOffset, src, numBytes);
    }

    static void moveConstruct(SegmentHeader& dest, size_t bytesOffset, T* src, size_t numBytes)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            SegmentTrivial::moveConstruct(dest, bytesOffset, src, numBytes);
        else
            SegmentNonTrivial<T>::moveConstruct(dest, bytesOffset, src, numBytes);
    }

    static void moveAssign(SegmentHeader& dest, size_t bytesOffset, T* src, size_t numBytes)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            SegmentTrivial::moveAssign(dest, bytesOffset, src, numBytes);
        else
            SegmentNonTrivial<T>::moveAssign(dest, bytesOffset, src, numBytes);
    }

    static void copyInsert(SegmentHeader& dest, size_t bytesOffset, const T* src, size_t numBytes)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            SegmentTrivial::copyInsert(dest, bytesOffset, src, numBytes);
        else
            SegmentNonTrivial<T>::copyInsert(dest, bytesOffset, src, numBytes);
    }

    static void remove(SegmentHeader& dest, size_t fromBytesOffset, size_t toBytesOffset)
    {
        if SC_LANGUAGE_IF_CONSTEXPR (TypeTraits::IsTriviallyCopyable<T>::value)
            SegmentTrivial::remove(dest, fromBytesOffset, toBytesOffset);
        else
            SegmentNonTrivial<T>::remove(dest, fromBytesOffset, toBytesOffset);
    }
};
} // namespace Internal

template <typename T>
struct SegmentVector : public Internal::ObjectVTable<T>
{
    static SegmentHeader* allocateNewHeader(size_t newCapacityInBytes)
    {
        return SegmentAllocator::allocateNewHeader(newCapacityInBytes);
    }

    static SegmentHeader* reallocateExistingHeader(SegmentHeader& src, size_t newCapacityInBytes)
    {
        if (TypeTraits::IsTriviallyCopyable<T>::value)
        {
            return SegmentAllocator::reallocateExistingHeader(src, newCapacityInBytes);
        }
        else
        {
            // TODO: Room for optimization for memcpy-able objects (a >= subset than trivially copyable)
            SegmentHeader* newHeader = allocateNewHeader(newCapacityInBytes);
            if (newHeader != nullptr)
            {
                *newHeader = src; // copy capacity, size and other fields
                T* tsrc    = src.getData<T>();
                Internal::ObjectVTable<T>::moveConstruct(*newHeader, 0, tsrc, src.sizeBytes);
                Internal::ObjectVTable<T>::destruct(src, 0, src.sizeBytes);
                destroyHeader(src);
            }
            return newHeader;
        }
    }
    static void destroyHeader(SegmentHeader& header) { SegmentAllocator::destroyHeader(header); }
};
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
struct Vector : public Segment<SegmentVector<T>>
{
    using Parent = Segment<SegmentVector<T>>;

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

        const size_t numBytes = static_cast<size_t>(itEnd - it) * sizeof(T);
        const size_t offBytes = static_cast<size_t>(it - itBeg) * sizeof(T);
        SegmentVector<T>::destruct(*Parent::header, offBytes, numBytes);
        Parent::header->sizeBytes -= static_cast<decltype(Parent::header->sizeBytes)>(numBytes);
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

} // namespace SC
