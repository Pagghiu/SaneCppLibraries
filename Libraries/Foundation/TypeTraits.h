// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/PrimitiveTypes.h"

namespace SC
{
// clang-format off
template <bool B, class T = void> struct EnableIf {};
template <class T> struct EnableIf<true, T> { using type = T; };
template <bool B, class T = void> using EnableIfT = typename EnableIf<B, T>::type;

template <typename T, typename U>   struct IsSame       { static constexpr bool value = false; };
template <typename T>               struct IsSame<T, T> { static constexpr bool value = true;  };

template <class T> struct RemovePointer       { using type = T; };
template <class T> struct RemovePointer<T*>   { using type = T; };
template <class T> struct RemovePointer<T**>  { using type = T; };

template <class T> struct AddReference      { using type = T&; };
template <class T> struct AddReference<T&>  { using type = T ; };
template <class T> struct AddReference<T&&> { using type = T ; };

template <class T> struct AddPointer      { using type = typename RemoveReference<T>::type*; };

template <class T> struct RemoveConst           { using type = T;};
template <class T> struct RemoveConst<const T>  { using type = T; };

template <typename T>                               struct ReturnType;
template <typename R, typename... Args>             struct ReturnType<R(Args...)>       { using type = R; };
template <typename R, typename... Args>             struct ReturnType<R(*)(Args...)>    { using type = R; };
template <typename R, typename C, typename... Args> struct ReturnType<R(C::*)(Args...)> { using type = R; };

template <class T, T v>
struct IntegralConstant
{
    static constexpr T value = v;

    using value_type = T;
    using type       = IntegralConstant;

    constexpr            operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; }
};

using true_type  = IntegralConstant<bool, true>;
using false_type = IntegralConstant<bool, false>;

template <typename T> struct IsConst            : public false_type { using type = T; };
template <typename T> struct IsConst<const T>   : public true_type  { using type = T; };

template <typename T> struct IsTriviallyCopyable  : public IntegralConstant<bool, __is_trivially_copyable(T)> { };

template <class T> struct IsReference : IntegralConstant<bool, IsLValueReference<T>::value || IsRValueReference<T>::value> { };

template <bool B, class T, class F> struct Conditional { using type = T; };
template <class T, class F>         struct Conditional<false, T, F> { using type = F; };
template <bool B, class T, class F> using ConditionalT = typename Conditional<B,T,F>::type;

template <typename SourceType, typename T> struct SameConstnessAs { using type = typename Conditional<IsConst<SourceType>::value, const T, T>::type; };

template <typename T, unsigned N> constexpr unsigned SizeOfArray(const T (&)[N]) { return N; }

// clang-format on

} // namespace SC

// Defining a constexpr destructor is C++ 20+
#if SC_LANGUAGE_CPP_AT_LEAST_20
#define SC_LANGUAGE_CONSTEXPR_DESTRUCTOR constexpr
#else
#define SC_LANGUAGE_CONSTEXPR_DESTRUCTOR
#endif
