// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Compiler.h" // move
#include "AlgorithmFind.h"

namespace SC
{
template <typename ForwardIt, typename UnaryPredicate>
ForwardIt remove_if(ForwardIt first, ForwardIt last, UnaryPredicate&& predicate)
{
    auto found = find_if(first, last, forward<UnaryPredicate>(predicate));
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
} // namespace SC
