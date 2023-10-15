// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Compiler.h"
#include "../Language/MetaProgramming.h"

namespace SC
{
template <typename... TT>
struct TypeList
{
    static const int size = sizeof...(TT);
};

template <typename T, int N, int M = 0>
struct TypeListGet;

template <int N, int M, typename T, typename... TT>
struct TypeListGet<TypeList<T, TT...>, N, M>
{
    static_assert(N < (int)sizeof...(TT) + 1 + M, "TypeList: index out of bounds");
    using type = SC::ConditionalT<N == M, T, typename TypeListGet<TypeList<TT...>, N, M + 1>::type>;
};

template <int N, int M>
struct TypeListGet<TypeList<>, N, M>
{
    using type = void;
};

template <int N>
struct TypeListGet<TypeList<>, N, 0>
{
};

template <typename T, int N>
using TypeListGetT = typename TypeListGet<T, N>::type;

} // namespace SC
