// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Base/Types.h"

namespace SC
{
// clang-format off
template <bool B, class T = void> struct EnableIf {};
template <class T> struct EnableIf<true, T> { using type = T; };
template <bool B, class T = void> using EnableIfT = typename EnableIf<B, T>::type;

template <typename T, typename U>   struct IsSame       { static constexpr bool value = false; };
template <typename T>               struct IsSame<T, T> { static constexpr bool value = true;  };

template <class T> struct RemoveReference       { using type = T; };
template <class T> struct RemoveReference<T&>   { using type = T; };
template <class T> struct RemoveReference<T&&>  { using type = T; };

template <class T> struct RemovePointer       { using type = T; };
template <class T> struct RemovePointer<T*>   { using type = T; };
template <class T> struct RemovePointer<T**>  { using type = T; };

template <class T> struct AddReference      { using type = T&; };
template <class T> struct AddReference<T&>  { using type = T ; };
template <class T> struct AddReference<T&&> { using type = T ; };

template <class T> struct AddPointer      { using type = typename RemoveReference<T>::type*; };

template <class T> struct RemoveConst           { using type = T;};
template <class T> struct RemoveConst<const T>  { using type = T; };

template <typename T> struct ReturnType;
template <typename R, typename... Args> struct ReturnType<R(Args...)> { using type = R; };
template <typename R, typename... Args> struct ReturnType<R(*)(Args...)> { using type = R; };
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

template <class T> struct IsLValueReference     : false_type { };
template <class T> struct IsLValueReference<T&> : true_type  { };
template <class T> struct IsRValueReference     : false_type { };
template <class T> struct IsRValueReference<T&&>: true_type  { };
template <class T> struct IsReference : IntegralConstant<bool, IsLValueReference<T>::value || IsRValueReference<T>::value> { };

template <bool B, class T, class F> struct Conditional { using type = T; };
template <class T, class F>         struct Conditional<false, T, F> { using type = F; };
template <bool B, class T, class F>
using ConditionalT = typename Conditional<B,T,F>::type;

template <typename SourceType, typename T> struct SameConstnessAs { using type = typename Conditional<IsConst<SourceType>::value, const T, T>::type; };

template <typename T> constexpr T min(T t1, T t2) { return t1 < t2 ? t1 : t2; }
template <typename T> constexpr T max(T t1, T t2) { return t1 > t2 ? t1 : t2; }

template <typename T, size_t N> constexpr size_t SizeOfArray(const T (&)[N]) { return N; }

template <typename T> constexpr T&& move(T& value) { return static_cast<T&&>(value); }
template <typename T> constexpr T&& forward(typename RemoveReference<T>::type& value) { return static_cast<T&&>(value); }
template <typename T> constexpr T&& forward(typename RemoveReference<T>::type&& value)
{
    static_assert(!IsLValueReference<T>::value, "Forward an rvalue as an lvalue is not allowed");
    return static_cast<T&&>(value);
}

template <typename T>
inline void swap(T& t1, T& t2)
{
    T temp = move(t1);
    t1     = move(t2);
    t2     = move(temp);
}

template <typename T>
struct ReferenceWrapper
{
    typename RemoveReference<T>::type* ptr;

    ReferenceWrapper(typename RemoveReference<T>::type& other) : ptr(&other) {}
    ~ReferenceWrapper() {}
    operator const T&() const { return *ptr; }
    operator T&() { return *ptr; }
};

template <typename F>
struct Deferred
{
    Deferred(F&& f) : f(forward<F>(f)) {}
    ~Deferred() { if (armed) f(); }
    void disarm() { armed = false; }

  private:
    F    f;
    bool armed = true;
};

template <typename F> Deferred<F> MakeDeferred(F&& f) { return Deferred<F>(forward<F>(f)); }

// clang-format on

} // namespace SC

namespace SC
{
struct PlacementNew
{
};
} // namespace SC
#if SC_COMPILER_MSVC
inline void* operator new(size_t, void* p, SC::PlacementNew) noexcept { return p; }
inline void  operator delete(void*, SC::PlacementNew) noexcept {}
#else
inline void* operator new(SC::size_t, void* p, SC::PlacementNew) noexcept { return p; }
#endif

#if defined(SC_LANGUAGE_FORCE_STANDARD_CPP)
#if SC_LANGUAGE_FORCE_STANDARD_CPP == 14
#define SC_LANGUAGE_CPP_VERSION 201402L
#elif SC_LANGUAGE_FORCE_STANDARD_CPP == 17
#define SC_LANGUAGE_CPP_VERSION 201703L
#elif SC_LANGUAGE_FORCE_STANDARD_CPP == 20
#define SC_LANGUAGE_CPP_VERSION 202002L
#else
#error "SC_LANGUAGE_FORCE_STANDARD_CPP has invalid value"
#endif
#else

#if SC_COMPILER_MSVC
#define SC_LANGUAGE_CPP_VERSION _MSVC_LANG
#else
#define SC_LANGUAGE_CPP_VERSION __cplusplus
#endif

#endif

#if SC_LANGUAGE_CPP_VERSION >= 202002L

#define SC_LANGUAGE_CPP_LESS_THAN_20 0
#define SC_LANGUAGE_CPP_AT_LEAST_20  1
#define SC_LANGUAGE_CPP_AT_LEAST_17  1
#define SC_LANGUAGE_CPP_AT_LEAST_14  1

#elif SC_LANGUAGE_CPP_VERSION >= 201703L

#define SC_LANGUAGE_CPP_LESS_THAN_20 1
#define SC_LANGUAGE_CPP_AT_LEAST_20  0
#define SC_LANGUAGE_CPP_AT_LEAST_17  1
#define SC_LANGUAGE_CPP_AT_LEAST_14  1

#elif SC_LANGUAGE_CPP_VERSION >= 201402L

#define SC_LANGUAGE_CPP_LESS_THAN_20 1
#define SC_LANGUAGE_CPP_AT_LEAST_20  0
#define SC_LANGUAGE_CPP_AT_LEAST_17  0
#define SC_LANGUAGE_CPP_AT_LEAST_14  1

#else

#define SC_LANGUAGE_CPP_LESS_THAN_20 1
#define SC_LANGUAGE_CPP_AT_LEAST_20  0
#define SC_LANGUAGE_CPP_AT_LEAST_17  0
#define SC_LANGUAGE_CPP_AT_LEAST_14  0

#endif

#undef SC_LANGUAGE_CPP_VERSION

// Defining a constexpr destructor is C++ 20+
#if SC_LANGUAGE_CPP_AT_LEAST_20
#define SC_LANGUAGE_CONSTEXPR_DESTRUCTOR constexpr
#else
#define SC_LANGUAGE_CONSTEXPR_DESTRUCTOR
#endif

#if (not SC_COMPILER_MSVC) or SC_LANGUAGE_CPP_AT_LEAST_20
#define SC_LANGUAGE_LIKELY   [[likely]]
#define SC_LANGUAGE_UNLIKELY [[unlikely]]
#else
#define SC_LANGUAGE_LIKELY
#define SC_LANGUAGE_UNLIKELY
#endif
