#pragma once
#include "Types.h"

namespace SC
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

template <typename T>
constexpr void swap(T& t1, T& t2)
{
    T temp = move(t1);
    t1     = move(t2);
    t2     = move(temp);
}
template <typename T>
struct smaller_than
{
    bool operator()(const T& a, const T& b) { return a < b; }
};

template <typename Iterator, typename Comparison>
void bubble_sort(Iterator first, Iterator last, Comparison comparison)
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

inline constexpr void* operator new(SC::size_t, void* p, SC::PlacementNew) noexcept { return p; }

#if __cplusplus >= 202002L

#define SC_CPP_AT_LEAST_20 1
#define SC_CPP_AT_LEAST_14 1

#elif __cplusplus >= 201402L

#define SC_CPP_AT_LEAST_20 0
#define SC_CPP_AT_LEAST_14 1

#else

#define SC_CPP_AT_LEAST_20 0
#define SC_CPP_AT_LEAST_14 0

#endif

// Using placement new in costructor is C++ 14+
#if SC_CPP_AT_LEAST_14
#define SC_CONSTEXPR_CONSTRUCTOR_NEW constexpr
#else
#define SC_CONSTEXPR_CONSTRUCTOR_NEW
#endif

// Defining a constexpr destructor is C++ 20+
#if SC_CPP_AT_LEAST_20
#define SC_CONSTEXPR_DESTRUCTOR constexpr
#else
#define SC_CONSTEXPR_DESTRUCTOR
#endif
