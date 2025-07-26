// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Libraries/Foundation/Compiler.h"
#include "../../Libraries/Foundation/TypeTraits.h"

namespace SC
{
namespace TypeTraits
{

//! @addtogroup group_foundation_type_traits
//! @{
// clang-format off
/// ConditionalT is an alias template that resolves to type `T` if a boolean value is `true`, otherwise to type `F`.
template <bool B, class T, class F> using ConditionalT = typename Conditional<B,T,F>::type;
// clang-format on

/// @brief Represents a variadic template type list.
///
/// @tparam TT Variadic template arguments forming the type list.
template <typename... TT>
struct TypeList
{
    /// Total number of types in the list.
    static const int size = sizeof...(TT);
};

/// @brief Retrieves the type at the specified index in the TypeList.
///
/// @tparam T The current type being inspected.
/// @tparam N Index of the type to retrieve.
/// @tparam M Initial index position in the type list.
template <typename T, int N, int M = 0>
struct TypeListGet;

/// @brief Specialization of TypeListGet that retrieves the type at the given index.
///
/// @tparam N Index of the type to retrieve.
/// @tparam M Initial index position in the type list.
/// @tparam T Current type in the list.
/// @tparam TT Variadic template argument list.
template <int N, int M, typename T, typename... TT>
struct TypeListGet<TypeList<T, TT...>, N, M>
{
    /// The retrieved type at the specified index.
    using type = SC::TypeTraits::ConditionalT<N == M, T, typename TypeListGet<TypeList<TT...>, N, M + 1>::type>;
};

/// @brief Specialization of TypeListGet for an empty TypeList.
///
/// @tparam N Index of the type to retrieve.
/// @tparam M Initial index position in the type list.
template <int N, int M>
struct TypeListGet<TypeList<>, N, M>
{
    /// If the TypeList is empty or the index is out of bounds, the retrieved type is 'void'.
    using type = void;
};

/// @brief Empty TypeListGet specialization with an index.
///
/// @tparam N Index of the type to retrieve.
template <int N>
struct TypeListGet<TypeList<>, N, 0>
{
};

/// @brief Alias template to simplify accessing the retrieved type using TypeListGet.
///
/// @tparam T TypeList for type retrieval.
/// @tparam N Index of the type to retrieve.
template <typename T, int N>
using TypeListGetT = typename TypeListGet<T, N>::type;

//! @}
} // namespace TypeTraits
} // namespace SC
