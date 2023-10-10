// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Assert.h"

namespace SC
{
template <typename InputIterator, typename UnaryPredicate>
constexpr InputIterator find_if(InputIterator first, InputIterator last, UnaryPredicate&& predicate)
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
} // namespace SC
