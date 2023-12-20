// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Array.h"
#include "../Containers/Vector.h"
#include "../Containers/VectorMap.h"
#include "../Strings/String.h"
#include "Reflection.h"

//-----------------------------------------------------------------------------------------------------------
// SC Types Support
//-----------------------------------------------------------------------------------------------------------
namespace SC
{
namespace Reflection
{

template <typename MemberVisitor, typename Container, typename ItemType, int N>
struct VectorArrayVTable
{
    [[nodiscard]] static constexpr bool build(MemberVisitor&) { return true; }
};

template <typename T, int N>
struct ExtendedTypeInfo<SC::Array<T, N>>
{
    static constexpr bool IsPacked = false;
};

template <typename T, int N>
struct Reflect<SC::Array<T, N>>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeVector; }

    template <typename MemberVisitor>
    [[nodiscard]] static constexpr bool build(MemberVisitor& builder)
    {
        using Type = typename MemberVisitor::Type;

        // TODO: Figure out a way to get rid of calling VectorArrayVTable here
        if (not VectorArrayVTable<MemberVisitor, SC::Array<T, N>, T, N>::build(builder))
            return false;

        // Add Array type
        if (not builder.addType(
                Type::template createArray<SC::Array<T, N>>("SC::Array<T, N>", 1, TypeInfo::ArrayInfo{false, N})))
            return false;

        // Add dependent item type
        if (not builder.addType(Type::template createGeneric<T>()))
            return false;
        return true;
    }
};

template <typename T>
struct Reflect<SC::Vector<T>>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeVector; }

    template <typename MemberVisitor>
    [[nodiscard]] static constexpr bool build(MemberVisitor& builder)
    {
        using Type = typename MemberVisitor::Type;

        // TODO: Figure out a way to get rid of calling VectorArrayVTable here
        if (not VectorArrayVTable<MemberVisitor, SC::Vector<T>, T, -1>::build(builder))
            return false;

        // Add Vector type
        if (not builder.addType(
                Type::template createArray<SC::Vector<T>>("SC::Vector", 1, TypeInfo::ArrayInfo{false, 0})))
            return false;

        // Add dependent item type
        if (not builder.addType(Type::template createGeneric<T>()))
            return false;

        return true;
    }
};

template <typename T>
struct ExtendedTypeInfo<SC::Vector<T>>
{
    static constexpr bool IsPacked = false;
};

template <typename Key, typename Value, typename Container>
struct Reflect<VectorMap<Key, Value, Container>> : ReflectStruct<VectorMap<Key, Value, Container>>
{
    using T = typename SC::VectorMap<Key, Value, Container>;

    template <typename MemberVisitor>
    [[nodiscard]] static constexpr bool visit(MemberVisitor&& builder)
    {
        return builder(0, "items", &T::items, SC_COMPILER_OFFSETOF(T, items));
    }
};

// TODO: Rethink if enumerations should not be collapsed to their underlying primitive type
template <>
struct Reflect<SC::StringEncoding> : Reflect<uint8_t>
{
    static_assert(sizeof(SC::StringEncoding) == sizeof(uint8_t), "size");
};

} // namespace Reflection
} // namespace SC

SC_REFLECT_STRUCT_VISIT(SC::String)
SC_REFLECT_STRUCT_FIELD(0, encoding) // TODO: Maybe encoding should be merged in data header
SC_REFLECT_STRUCT_FIELD(1, data)
SC_REFLECT_STRUCT_LEAVE()
