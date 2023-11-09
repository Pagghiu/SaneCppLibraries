// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Compiler.h"
#include "../Foundation/PrimitiveTypes.h"

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
template <typename T> struct IsPrimitive { static constexpr bool value = false; };

template <> struct IsPrimitive<uint8_t>  { static constexpr bool value = true; };
template <> struct IsPrimitive<uint16_t> { static constexpr bool value = true; };
template <> struct IsPrimitive<uint32_t> { static constexpr bool value = true; };
template <> struct IsPrimitive<uint64_t> { static constexpr bool value = true; };
template <> struct IsPrimitive<int8_t>   { static constexpr bool value = true; };
template <> struct IsPrimitive<int16_t>  { static constexpr bool value = true; };
template <> struct IsPrimitive<int32_t>  { static constexpr bool value = true; };
template <> struct IsPrimitive<int64_t>  { static constexpr bool value = true; };
template <> struct IsPrimitive<float>    { static constexpr bool value = true; };
template <> struct IsPrimitive<double>   { static constexpr bool value = true; };
template <> struct IsPrimitive<char>     { static constexpr bool value = true; };
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
        SC_COMPILER_UNUSED(order);
        SC_COMPILER_UNUSED(name);
        SC_COMPILER_UNUSED(member);
        SC_COMPILER_UNUSED(offset);
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
