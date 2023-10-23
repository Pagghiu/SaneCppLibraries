// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Compiler.h" // move
#include "AlgorithmFind.h"

namespace SC
{
namespace Algorithms
{
template <typename ForwardIt, typename UnaryPredicate>
ForwardIt removeIf(ForwardIt first, ForwardIt last, UnaryPredicate&& predicate)
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
} // namespace Algorithms
} // namespace SC
