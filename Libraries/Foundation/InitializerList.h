// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"
namespace std
{
template <class _Ep>
class initializer_list
{
    using size_t = SC::size_t;
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

template <class _Ep>
inline constexpr const _Ep* begin(initializer_list<_Ep> __il) noexcept
{
    return __il.begin();
}

template <class _Ep>
inline constexpr const _Ep* end(initializer_list<_Ep> __il) noexcept
{
    return __il.end();
}

} // namespace std
