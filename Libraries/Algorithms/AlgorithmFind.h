// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"

namespace SC
{
/// @brief Simple algorithms (see @ref library_algorithms)
namespace Algorithms
{

//! @addtogroup group_algorithms
//! @{

/// @brief Find item satisfying the given predicate
/// @tparam ForwardIterator A forward iterator (supports ++ and dereference operators)
/// @tparam UnaryPredicate A lambda or functor predicate with a `bool (const T&)` operator
/// @param first Iterator pointing at the first item in the range to check
/// @param last Iterator pointing after last item in the range to check
/// @param predicate The functor or lambda predicate that decides if wanted item is found
/// @param index if passed in != `nullptr`, receives index where item was found. Only written if function returns `true`
/// @return Iterator pointing at the found item or `last` if not found
template <typename ForwardIterator, typename UnaryPredicate>
constexpr ForwardIterator findIf(ForwardIterator first, ForwardIterator last, UnaryPredicate&& predicate,
                                 size_t* index = nullptr)
{
    SC_ASSERT_DEBUG(first <= last);
    for (auto it = first; it != last; ++it)
    {
        if (predicate(*it))
        {
            if (index)
            {
                *index = static_cast<size_t>(it - first);
            }
            return it;
        }
    }
    return last;
}

/// @brief Check if the container contains a given value.
/// @tparam Container A container with begin and end iterators
/// @tparam T The type of the value to search for
/// @param container The container to search in
/// @param value The value to search for
/// @param index if passed in != `nullptr`, receives index where item was found. Only written if function returns `true`
/// @return `true` if the container contains the given value, `false` otherwise
template <typename Container, typename T>
constexpr bool contains(Container container, const T& value, size_t* index = nullptr)
{
    return findIf(
               container.begin(), container.end(), [&](auto& item) { return item == value; }, index) != container.end();
}

//! @}

} // namespace Algorithms
} // namespace SC
