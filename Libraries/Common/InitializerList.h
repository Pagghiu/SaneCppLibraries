// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_INITIALIZER_LIST_DEFINITION_H
#if SC_FOUNDATION_INITIALIZER_LIST_DEFINITION_H != 1
#error "InitializerList.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_INITIALIZER_LIST_DEFINITION_H 1 // Increment to indicate a new version of the file

#include "CompilerMacrosStdCpp.h" // SC_INCLUDE_STD_CPP

#if SC_INCLUDE_STD_CPP
#include <initializer_list>
#else
namespace std
{
template <class _Ep>
class initializer_list
{
    using size_t = decltype(sizeof(0));
    const _Ep* __begin_;
    size_t     __size_;

    constexpr initializer_list(const _Ep* __b, size_t __s) noexcept : __begin_(__b), __size_(__s) {}

  public:
    using value_type = _Ep;
    using size_type  = size_t;

    using iterator       = const _Ep*;
    using const_iterator = const _Ep*;

    constexpr initializer_list() noexcept : __begin_(nullptr), __size_(0) {}
    constexpr initializer_list(const _Ep* _First_arg, const _Ep* _Last_arg) noexcept
        : __begin_(_First_arg), __size_(static_cast<size_t>(_Last_arg - _First_arg))
    {}

    constexpr size_t     size() const noexcept { return __size_; }
    constexpr const _Ep* begin() const noexcept { return __begin_; }
    constexpr const _Ep* end() const noexcept { return __begin_ + __size_; }
};

} // namespace std
#endif

#endif