// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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
/// @return Iterator pointing at the found item
template <typename ForwardIterator, typename UnaryPredicate>
constexpr ForwardIterator findIf(ForwardIterator first, ForwardIterator last, UnaryPredicate&& predicate)
{
    SC_ASSERT_DEBUG(first <= last);
    for (auto it = first; it != last; ++it)
    {
        if (predicate(*it))
        {
            return it;
        }
    }
    return last;
}

//! @}

} // namespace Algorithms
} // namespace SC
