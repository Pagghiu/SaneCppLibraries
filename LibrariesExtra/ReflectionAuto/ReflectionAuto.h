// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//------------------------------------------------------------------------
// Adapted from "C++ Type Loophole" with some changes to count aggregates collapsing arrays
// Up-to-date source is at GitHub repo: https://github.com/alexpolt/luple
// Author: Alexandr Poltavsky, http://alexpolt.github.io
// http://alexpolt.github.io/type-loophole.html
// Original Code License: Public Domain
//------------------------------------------------------------------------
#pragma once
#include "../../Libraries/Foundation/Compiler.h"
#include "../../Libraries/Foundation/TypeList.h"

namespace SC
{
namespace Reflection
{
namespace Auto
{
template <typename TypeList, int N>
struct TypeListVisit
{
    template <typename Visitor>
    constexpr static bool visit(Visitor&& visitor)
    {
        using Type = TypeTraits::TypeListGetT<TypeList, N - 1>;
        return TypeListVisit<TypeList, N - 1>::visit(forward<Visitor>(visitor)) and
               visitor.template visit<N - 1, Type>();
    }
};

template <typename TypeList>
struct TypeListVisit<TypeList, 0>
{
    template <typename Visitor>
    constexpr static bool visit(Visitor&& visitor)
    {
        SC_COMPILER_UNUSED(visitor);
        return true;
    }
};

template <class IntegerType, IntegerType... Values>
struct IntegerSequence
{
    using value_type = IntegerType;
    static constexpr size_t size() noexcept { return sizeof...(Values); }
};

#if SC_COMPILER_GCC
template <class IntegerType, IntegerType N>
using MakeIntegerSequence = IntegerSequence<IntegerType, __integer_pack(N)...>;
#else
template <class IntegerType, IntegerType N>
using MakeIntegerSequence = __make_integer_seq<IntegerSequence, IntegerType, N>;
#endif
template <size_t N>
using MakeIndexSequence = MakeIntegerSequence<size_t, N>;
template <class... T>
using IndexSequenceFor = MakeIndexSequence<sizeof...(T)>;
template <size_t... N>
using IndexSequence = IntegerSequence<size_t, N...>;

template <typename T>
struct result
{
    using type = T;
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
    friend auto          loophole(tag<T, N>) { return result<U>(); }
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

#if SC_LANGUAGE_CPP_LESS_THAN_20
    template <typename U, int = sizeof(fn_def<T, U, N, sizeof(inserter<U, N>(0)) == sizeof(char)>)>
    operator U();
#else
    template <typename U, int = sizeof(fn_def<T, U, N, sizeof(inserter<U, N>(0)) == sizeof(char)>)>
    using type = U;
#endif
};

template <typename T, typename U>
struct type_list;

template <typename T, int... NN>
struct type_list<T, IntegerSequence<int, NN...>>
{
    static constexpr int size = sizeof...(NN);
    using type                = SC::TypeTraits::TypeList<typename decltype(loophole(tag<T, NN>{}))::type...>;
};
} // namespace Auto
} // namespace Reflection
} // namespace SC
