// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Language/Language.h"
#include "../Foundation/Language/Types.h"

namespace SC
{
namespace Reflection
{

template <typename T>
struct MetaClass;

struct MetaStructFlags
{
    static const uint32_t IsPacked = 1 << 1; // IsPacked AND No padding in every contained field (recursively)
};

enum class MetaType : uint8_t
{
    // Invalid sentinel
    TypeInvalid = 0,

    // Primitive types
    TypeUINT8    = 1,
    TypeUINT16   = 2,
    TypeUINT32   = 3,
    TypeUINT64   = 4,
    TypeINT8     = 5,
    TypeINT16    = 6,
    TypeINT32    = 7,
    TypeINT64    = 8,
    TypeFLOAT32  = 9,
    TypeDOUBLE64 = 10,

    TypeStruct = 11,
    TypeArray  = 12,
    TypeVector = 13,
};

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
struct MetaTypeInfo;

template <typename T>
struct MetaTypeInfoStruct
{
    size_t memberSizeSum = 0;
    bool   IsPacked      = false;

    constexpr MetaTypeInfoStruct()
    {
        if (MetaClass<T>::visit(*this))
        {
            IsPacked = memberSizeSum == sizeof(T);
        }
    }
    template <typename R, int N>
    constexpr bool operator()(int order, const char (&name)[N], R T::*member, size_t offset)
    {
        SC_UNUSED(order);
        SC_UNUSED(name);
        SC_UNUSED(member);
        SC_UNUSED(offset);
        if (not MetaTypeInfo<R>().IsPacked)
        {
            return false;
        }
        memberSizeSum += sizeof(R);
        return true;
    }
};

template <typename T, typename SFINAESelector>
struct MetaTypeInfo
{
    static constexpr bool IsPacked = MetaTypeInfoStruct<T>().IsPacked;
};

template <typename T, int N>
struct MetaTypeInfo<T[N]>
{
    static constexpr bool IsPacked = MetaTypeInfo<T>::IsPacked;
};

template <typename T>
struct MetaTypeInfo<T, typename SC::EnableIf<IsPrimitive<T>::value>::type>
{
    static constexpr bool IsPacked = true;
};

} // namespace Reflection
} // namespace SC
