// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/TypeTraits.h" // RemovePointer

namespace SC
{
//! @defgroup group_algorithms Algorithms
//! @copybrief library_algorithms (see @ref library_algorithms for more details)

namespace Algorithms
{
//! @addtogroup group_algorithms
//! @{

/// @brief Functor that evaluates to `a < b`
template <typename T>
struct smallerThan
{
    /// @brief Returns `true` if `a < b`
    /// @param a First element to be tested for `a < b`
    /// @param b Second element to be tested for `a < b`
    /// @return  Returns `true` if `a < b`
    constexpr bool operator()(const T& a, const T& b) { return a < b; }
};

/// @brief Sorts iterator range according to BinaryPredicate (bubble sort).
/// @tparam Iterator A type that behaves as an iterator (can just be a pointer to element in array / vector)
/// @tparam BinaryPredicate A predicate that takes `(a, b)` and returns `bool` (example SC::Algorithms::smallerThan)
/// @param first Iterator pointing at first element of the range
/// @param last Iterator pointing at last element of the array
/// @param predicate The given BinaryPredicate
template <typename Iterator, typename BinaryPredicate = smallerThan<typename TypeTraits::RemovePointer<Iterator>::type>>
constexpr void bubbleSort(Iterator first, Iterator last, BinaryPredicate predicate = BinaryPredicate())
{
    if (first >= last)
    {
        return;
    }
    bool doSwap = true;
    while (doSwap)
    {
        doSwap      = false;
        Iterator p0 = first;
        Iterator p1 = first + 1;
        while (p1 != last)
        {
            if (predicate(*p1, *p0))
            {
                swap(*p1, *p0);
                doSwap = true;
            }
            ++p0;
            ++p1;
        }
    }
}
//! @}
} // namespace Algorithms
} // namespace SC
