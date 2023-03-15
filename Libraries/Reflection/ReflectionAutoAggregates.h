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
#include "../Foundation/Language.h"
#include "../Foundation/TypeList.h"

namespace SC
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
struct loophole_TypeList;

template <typename T, int... NN>
struct loophole_TypeList<T, IntegerSequence<int, NN...>>
{
    static constexpr int size = sizeof...(NN);
    using type                = TypeList<typename decltype(loophole(tag<T, NN>{}))::type...>;
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
using as_TypeList =
    typename loophole_TypeList<T, MakeIntegerSequence<int, enumerate_fields_with_aggregates<T>(0)>>::type;

template <typename T>
using TypeListFor = as_TypeList<T>;

} // namespace SC
