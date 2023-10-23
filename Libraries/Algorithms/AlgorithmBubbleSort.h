// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/TypeTraits.h" // RemovePointer

namespace SC
{
namespace Algorithms
{
template <typename T>
struct smallerThan
{
    bool operator()(const T& a, const T& b) { return a < b; }
};

template <typename Iterator, typename BinaryPredicate = smallerThan<typename RemovePointer<Iterator>::type>>
void bubbleSort(Iterator first, Iterator last, BinaryPredicate predicate = BinaryPredicate())
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
} // namespace Algorithms
} // namespace SC
