// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Language.h" // RemovePointer

namespace SC
{
template <typename T>
struct SmallerThan
{
    bool operator()(const T& a, const T& b) { return a < b; }
};

template <typename T>
inline void swap(T& t1, T& t2)
{
    T temp = move(t1);
    t1     = move(t2);
    t2     = move(temp);
}

template <typename Iterator, typename Comparison = SmallerThan<typename RemovePointer<Iterator>::type>>
void bubbleSort(Iterator first, Iterator last, Comparison comparison = Comparison())
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
            if (comparison(*p1, *p0))
            {
                swap(*p1, *p0);
                doSwap = true;
            }
            ++p0;
            ++p1;
        }
    }
}
} // namespace SC
