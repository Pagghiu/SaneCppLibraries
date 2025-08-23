// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Compiler.h" // move
#include "AlgorithmFind.h"

namespace SC
{
namespace Algorithms
{
//! @addtogroup group_algorithms
//! @{

/// @brief Removes all items in the given range, satisfying the given predicate
/// @tparam ForwardIterator A forward iterator (supports ++ and dereference operators)
/// @tparam UnaryPredicate A lambda or functor predicate with a `bool (const T&)` operator
/// @param first Iterator pointing at the first item in the range to check
/// @param last Iterator pointing after last item in the range to check
/// @param predicate The functor or lambda predicate that decides if current item must be removed
/// @return `last`
template <typename ForwardIterator, typename UnaryPredicate>
ForwardIterator removeIf(ForwardIterator first, ForwardIterator last, UnaryPredicate&& predicate)
{
    auto found = findIf(first, last, forward<UnaryPredicate>(predicate));
    if (found != last)
    {
        auto it = found;
        while (++it != last)
        {
            if (not predicate(*it))
            {
                *found++ = move(*it);
            }
        }
    }
    return found;
}

//! @}

} // namespace Algorithms
} // namespace SC
