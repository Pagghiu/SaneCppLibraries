#pragma once
// #include <initializer_list>
//  Just in case
#ifndef _LIBCPP_INITIALIZER_LIST
#define _LIBCPP_INITIALIZER_LIST
#include "Types.h"
namespace std
{

typedef SC::size_t size_t;
#define _LIBCPP_TEMPLATE_VIS
#define _LIBCPP_INLINE_VISIBILITY
#define _LIBCPP_CONSTEXPR_AFTER_CXX11 constexpr
#define _NOEXCEPT                     noexcept
template <class _Ep>
class _LIBCPP_TEMPLATE_VIS initializer_list
{
    const _Ep* __begin_;
    size_t     __size_;

    _LIBCPP_INLINE_VISIBILITY
    _LIBCPP_CONSTEXPR_AFTER_CXX11
    initializer_list(const _Ep* __b, size_t __s) _NOEXCEPT : __begin_(__b), __size_(__s) {}

  public:
    typedef _Ep        value_type;
    typedef const _Ep& reference;
    typedef const _Ep& const_reference;
    typedef size_t     size_type;

    typedef const _Ep* iterator;
    typedef const _Ep* const_iterator;

    _LIBCPP_INLINE_VISIBILITY
    _LIBCPP_CONSTEXPR_AFTER_CXX11
    initializer_list() _NOEXCEPT : __begin_(nullptr), __size_(0) {}
    _LIBCPP_INLINE_VISIBILITY
    _LIBCPP_CONSTEXPR_AFTER_CXX11
    initializer_list(const _Ep* _First_arg, const _Ep* _Last_arg) _NOEXCEPT : __begin_(_First_arg),
                                                                              __size_(_Last_arg - _First_arg)
    {}

    _LIBCPP_INLINE_VISIBILITY
    _LIBCPP_CONSTEXPR_AFTER_CXX11
    size_t size() const _NOEXCEPT { return __size_; }

    _LIBCPP_INLINE_VISIBILITY
    _LIBCPP_CONSTEXPR_AFTER_CXX11
    const _Ep* begin() const _NOEXCEPT { return __begin_; }

    _LIBCPP_INLINE_VISIBILITY
    _LIBCPP_CONSTEXPR_AFTER_CXX11
    const _Ep* end() const _NOEXCEPT { return __begin_ + __size_; }
};

template <class _Ep>
inline _LIBCPP_INLINE_VISIBILITY _LIBCPP_CONSTEXPR_AFTER_CXX11 const _Ep* begin(initializer_list<_Ep> __il) _NOEXCEPT
{
    return __il.begin();
}

template <class _Ep>
inline _LIBCPP_INLINE_VISIBILITY _LIBCPP_CONSTEXPR_AFTER_CXX11 const _Ep* end(initializer_list<_Ep> __il) _NOEXCEPT
{
    return __il.end();
}

} // namespace std
#undef _LIBCPP_TEMPLATE_VIS
#undef _LIBCPP_INLINE_VISIBILITY
#undef _LIBCPP_CONSTEXPR_AFTER_CXX11
#undef _NOEXCEPT
#endif
