// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#include "../Language/InitializerList.h"

namespace SC
{
template <class ForwardIt>
static constexpr ForwardIt MaxElement(ForwardIt first, ForwardIt last)
{
    if (first == last)
        return last;

    ForwardIt largest = first;
    ++first;

    for (; first != last; ++first)
        if (*largest < *first)
            largest = first;

    return largest;
}

template <class T>
static constexpr T max(std::initializer_list<T> ilist)
{
    return *MaxElement(ilist.begin(), ilist.end());
}
} // namespace SC
