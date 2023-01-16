// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Language.h"
#include "Types.h"

namespace SC
{
namespace Reflection
{
template <typename T>
struct MetaClass;
// clang-format off
template <typename T> struct IsPrimitive : false_type {};

template <> struct IsPrimitive<uint8_t>  : true_type  {};
template <> struct IsPrimitive<uint16_t> : true_type  {};
template <> struct IsPrimitive<uint32_t> : true_type  {};
template <> struct IsPrimitive<uint64_t> : true_type  {};
template <> struct IsPrimitive<int8_t>   : true_type  {};
template <> struct IsPrimitive<int16_t>  : true_type  {};
template <> struct IsPrimitive<int32_t>  : true_type  {};
template <> struct IsPrimitive<int64_t>  : true_type  {};
template <> struct IsPrimitive<float>    : true_type  {};
template <> struct IsPrimitive<double>   : true_type  {};
template <> struct IsPrimitive<char_t>   : true_type  {};
// clang-format on

template <typename T, typename SFINAESelector = void>
struct ClassInfo;

template <typename T>
struct ClassInfoStruct
{
    size_t memberSizeSum = 0;
    bool   IsPacked      = false;

    constexpr ClassInfoStruct()
    {
        if (MetaClass<T>::visit(*this))
        {
            IsPacked = memberSizeSum == sizeof(T);
        }
    }
    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset)
    {
        if (not ClassInfo<R>().IsPacked)
        {
            return false;
        }
        memberSizeSum += sizeof(R);
        return true;
    }
};

template <typename T, typename SFINAESelector>
struct ClassInfo
{
    static constexpr bool IsPacked = ClassInfoStruct<T>().IsPacked;
};

template <typename T, int N>
struct ClassInfo<T[N]>
{
    static constexpr bool IsPacked = ClassInfo<T>::IsPacked;
};

template <typename T>
struct ClassInfo<T, typename SC::EnableIf<IsPrimitive<T>::value>::type>
{
    static constexpr bool IsPacked = true;
};

} // namespace Reflection
} // namespace SC
