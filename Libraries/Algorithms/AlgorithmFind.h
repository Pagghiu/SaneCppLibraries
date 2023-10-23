// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Assert.h"

namespace SC
{
namespace Algorithms
{
template <typename InputIterator, typename UnaryPredicate>
constexpr InputIterator findIf(InputIterator first, InputIterator last, UnaryPredicate&& predicate)
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
} // namespace Algorithms
} // namespace SC
