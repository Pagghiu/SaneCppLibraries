// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
//------------------------------------------------------------------------
// Adapted from "C++ Type Loophole" with some changes to count aggregates collapsing arrays
// Up-to-date source is at GitHub repo: https://github.com/alexpolt/luple
// Author: Alexandr Poltavsky, http://alexpolt.github.io
// http://alexpolt.github.io/type-loophole.html
// Original Code License: Public Domain
//------------------------------------------------------------------------
#pragma once
#include "Language.h"

namespace luple
{
// type list
template <typename... TT>
struct type_list
{
    static const int size = sizeof...(TT);
};

template <typename T, int N, int M = 0>
struct tlist_get;

template <int N, int M, typename T, typename... TT>
struct tlist_get<type_list<T, TT...>, N, M>
{
    static_assert(N < (int)sizeof...(TT) + 1 + M, "type index out of bounds");
    using type = SC::ConditionalT<N == M, T, typename tlist_get<type_list<TT...>, N, M + 1>::type>;
};

template <int N, int M>
struct tlist_get<type_list<>, N, M>
{
    using type = void;
};

template <int N>
struct tlist_get<type_list<>, N, 0>
{
};

template <typename T, int N>
using tlist_get_t = typename tlist_get<T, N>::type;

} // namespace luple

namespace loophole_aggregates
{
template <typename T>
struct loopholeResult
{
    typedef T type;
};
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif
template <typename T, int N>
struct tag
{
    friend auto          loophole(tag<T, N>);
    constexpr friend int cloophole(tag<T, N>);
};

template <typename T, typename U, int N, bool B>
struct fn_def
{
    friend auto          loophole(tag<T, N>) { return loopholeResult<U>(); }
    constexpr friend int cloophole(tag<T, N>) { return 0; }
};
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

template <typename T, typename U, int N>
struct fn_def<T, U, N, true>
{
};

template <typename T, int N>
struct c_op
{
    template <typename U, int M>
    static auto inserter(...) -> int;
    template <typename U, int M, int = cloophole(tag<T, M>{})>
    static auto inserter(int) -> char;

    template <typename U, int = sizeof(fn_def<T, U, N, sizeof(inserter<U, N>(0)) == sizeof(char)>)>
    operator U();
};

template <typename T, typename U>
struct loophole_type_list;

template <typename T, int... NN>
struct loophole_type_list<T, SC::IntegerSequence<int, NN...>>
{
    static constexpr int size = sizeof...(NN);
    using type                = luple::type_list<typename decltype(loophole(tag<T, NN>{}))::type...>;
};
template <typename T, int... NN>
constexpr int enumerate_fields_with_aggregates(...)
{
    return sizeof...(NN) - 1;
}

template <typename T, int... NN>
constexpr auto enumerate_fields_with_aggregates(int) -> decltype(T{{c_op<T, NN>{}}...}, 0)
{
    return enumerate_fields_with_aggregates<T, NN..., sizeof...(NN)>(0);
}

template <typename T>
using as_type_list =
    typename loophole_type_list<T, SC::MakeIntegerSequence<int, enumerate_fields_with_aggregates<T>(0)>>::type;

template <typename T>
using TypeListFor = as_type_list<T>;

} // namespace loophole_aggregates
