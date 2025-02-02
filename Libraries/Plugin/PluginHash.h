// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

namespace SC
{
namespace detail
{
template <unsigned int N, unsigned int I>
struct PluginHashImpl
{
    static constexpr unsigned int Hash(const char (&str)[N])
    {
        return (PluginHashImpl<N, I - 1>::Hash(str) ^ static_cast<unsigned int>(str[I - 1])) * 16777619u;
    }
};

template <unsigned int N>
struct PluginHashImpl<N, 1>
{
    static constexpr unsigned int Hash(const char (&str)[N])
    {
        return (2166136261u ^ static_cast<unsigned int>(str[0])) * 16777619u;
    }
};
} // namespace detail

//! @addtogroup group_strings
//! @{

/// @brief Compute compile time FNV hash for a char array
template <unsigned int N>
constexpr unsigned int PluginHash(const char (&str)[N])
{
    return detail::PluginHashImpl<N, N>::Hash(str);
}
//! @}

} // namespace SC
