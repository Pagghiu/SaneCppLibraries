#pragma once
#include "types.h"

namespace sanecpp
{
template <bool B, class T = void>
struct enable_if
{
};

template <class T>
struct enable_if<true, T>
{
    typedef T type;
};

template <typename T, typename U>
struct is_same
{
    static constexpr bool value = false;
};

template <typename T>
struct is_same<T, T>
{
    static constexpr bool value = true;
};

template <class T>
struct remove_reference
{
    typedef T type;
};
template <class T>
struct remove_reference<T&>
{
    typedef T type;
};
template <class T>
struct remove_reference<T&&>
{
    typedef T type;
};

template <class T, T v>
struct integral_constant
{
    static constexpr T value = v;

    using value_type = T;
    using type       = integral_constant;

    constexpr            operator value_type() const noexcept { return value; }
    constexpr value_type operator()() const noexcept { return value; }
};

template <typename _Tp>
struct is_trivially_copyable : public integral_constant<bool, __is_trivially_copyable(_Tp)>
{
};

template <typename T>
constexpr T&& move(T& value)
{
    return static_cast<T&&>(value);
}

template <typename T>
constexpr T&& forward(T& value)
{
    return static_cast<typename remove_reference<T>::type&&>(value);
}
struct PlacementNew
{
};

template <typename T>
constexpr T min(T t1, T t2)
{
    return t1 < t2 ? t1 : t2;
}

template <typename T>
constexpr T max(T t1, T t2)
{
    return t1 > t2 ? t1 : t2;
}
} // namespace sanecpp

inline constexpr void* operator new(sanecpp::size_t, void* p, sanecpp::PlacementNew) noexcept { return p; }

#if __cplusplus >= 202002L

#define SANECPP_CPP_AT_LEAST_20 1
#define SANECPP_CPP_AT_LEAST_14 1

#elif __cplusplus >= 201402L

#define SANECPP_CPP_AT_LEAST_20 0
#define SANECPP_CPP_AT_LEAST_14 1

#else

#define SANECPP_CPP_AT_LEAST_20 0
#define SANECPP_CPP_AT_LEAST_14 0

#endif

// Using placement new in costructor is C++ 14+
#if SANECPP_CPP_AT_LEAST_14
#define SANECPP_CONSTEXPR_CONSTRUCTOR_NEW constexpr
#else
#define SANECPP_CONSTEXPR_CONSTRUCTOR_NEW
#endif

// Defining a constexpr destructor is C++ 20+
#if SANECPP_CPP_AT_LEAST_20
#define SANECPP_CONSTEXPR_DESTRUCTOR constexpr
#else
#define SANECPP_CONSTEXPR_DESTRUCTOR
#endif
