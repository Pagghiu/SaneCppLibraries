// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Array.h"
#include "../Containers/Vector.h"
#include "../Containers/VectorMap.h"
#include "../Reflection/Reflection.h"

namespace SC
{
namespace Reflection
{
// Forward Declarations (to avoid depending on Reflection)
template <typename T, typename SFINAESelector>
struct ExtendedTypeInfo;

template <typename T>
struct Reflect;

template <typename T, int N>
struct ExtendedTypeInfo<SC::Array<T, N>, void>
{
    static constexpr bool IsPacked = false;

    static auto size(const SC::Array<T, N>& object) { return object.size(); }
    static auto data(SC::Array<T, N>& object) { return object.data(); }
    static bool resizeWithoutInitializing(SC::Array<T, N>& object, size_t newSize)
    {
        return object.resizeWithoutInitializing(min(newSize, static_cast<size_t>(N)));
    }
    static bool resize(SC::Array<T, N>& object, size_t newSize)
    {
        return object.resize(min(newSize, static_cast<size_t>(N)));
    }
};

template <typename T, int N>
struct Reflect<SC::Array<T, N>>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeVector; }

    template <typename MemberVisitor>
    static constexpr bool build(MemberVisitor& builder)
    {
        // TODO: Figure out a way to get rid of calling VectorArrayVTable here
        if (not VectorArrayVTable<MemberVisitor, SC::Array<T, N>, T, N>::build(builder))
            return false;

        // Add Array type
        constexpr TypeInfo::ArrayInfo arrayInfo = {false, N}; // false == not packed
        if (not builder.addType(MemberVisitor::Type::template createArray<SC::Array<T, N>>("SC::Array", 1, arrayInfo)))
            return false;

        // Add dependent item type
        return builder.addType(MemberVisitor::Type::template createGeneric<T>());
    }
};

template <typename T>
struct Reflect<SC::Vector<T>>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeVector; }

    template <typename MemberVisitor>
    static constexpr bool build(MemberVisitor& builder)
    {
        // TODO: Figure out a way to get rid of calling VectorArrayVTable here
        if (not VectorArrayVTable<MemberVisitor, SC::Vector<T>, T, -1>::build(builder))
            return false;

        // Add Vector type
        constexpr TypeInfo::ArrayInfo arrayInfo = {false, 0}; // false == not packed
        if (not builder.addType(MemberVisitor::Type::template createArray<SC::Vector<T>>("SC::Vector", 1, arrayInfo)))
            return false;

        // Add dependent item type
        return builder.addType(MemberVisitor::Type::template createGeneric<T>());
    }
};

template <typename T>
struct ExtendedTypeInfo<SC::Vector<T>, void>
{
    static constexpr bool IsPacked = false;

    static auto size(const SC::Vector<T>& object) { return object.size(); }
    static auto data(SC::Vector<T>& object) { return object.data(); }
    static bool resizeWithoutInitializing(SC::Vector<T>& object, size_t newSize)
    {
        return object.resizeWithoutInitializing(newSize);
    }
    static bool resize(SC::Vector<T>& object, size_t newSize) { return object.resize(newSize); }
};

template <typename Key, typename Value, typename Container>
struct Reflect<VectorMap<Key, Value, Container>> : ReflectStruct<VectorMap<Key, Value, Container>>
{
    using T = typename SC::VectorMap<Key, Value, Container>;

    template <typename MemberVisitor>
    static constexpr bool visit(MemberVisitor&& builder)
    {
        return builder(0, "items", &T::items, SC_COMPILER_OFFSETOF(T, items));
    }
};

} // namespace Reflection
} // namespace SC
