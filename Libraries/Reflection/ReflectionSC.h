// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Array.h"
#include "../Containers/Vector.h"
#include "../Containers/VectorMap.h"
#include "../Memory/Buffer.h"
#include "../Memory/String.h"
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

    [[nodiscard]] static auto size(const SC::Array<T, N>& object) { return object.size(); }
    [[nodiscard]] static auto data(SC::Array<T, N>& object) { return object.data(); }
    [[nodiscard]] static bool resizeWithoutInitializing(SC::Array<T, N>& object, size_t newSize)
    {
        return object.resizeWithoutInitializing(min(newSize, static_cast<size_t>(N)));
    }
    [[nodiscard]] static bool resize(SC::Array<T, N>& object, size_t newSize)
    {
        return object.resize(min(newSize, static_cast<size_t>(N)));
    }
};

template <typename T, int N>
struct Reflect<SC::Array<T, N>>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeVector; }

    template <typename MemberVisitor>
    [[nodiscard]] static constexpr bool build(MemberVisitor& builder)
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
    [[nodiscard]] static constexpr bool build(MemberVisitor& builder)
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
struct ExtendedTypeInfo<SC::Vector<T>>
{
    static constexpr bool IsPacked = false;

    [[nodiscard]] static auto size(const SC::Vector<T>& object) { return object.size(); }
    [[nodiscard]] static auto data(SC::Vector<T>& object) { return object.data(); }
    [[nodiscard]] static bool resizeWithoutInitializing(SC::Vector<T>& object, size_t newSize)
    {
        return object.resizeWithoutInitializing(newSize);
    }
    [[nodiscard]] static bool resize(SC::Vector<T>& object, size_t newSize) { return object.resize(newSize); }
};

template <>
struct Reflect<SC::Buffer>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeVector; }

    template <typename MemberVisitor>
    [[nodiscard]] static constexpr bool build(MemberVisitor& builder)
    {
        // TODO: Figure out a way to get rid of calling VectorArrayVTable here
        if (not VectorArrayVTable<MemberVisitor, SC::Buffer, char, -1>::build(builder))
            return false;

        // Add Vector type
        constexpr TypeInfo::ArrayInfo arrayInfo = {false, 0}; // false == not packed
        if (not builder.addType(MemberVisitor::Type::template createArray<SC::Buffer>("SC::Buffer", 1, arrayInfo)))
            return false;

        // Add dependent item type
        return builder.addType(MemberVisitor::Type::template createGeneric<char>());
    }
};
template <>
struct ExtendedTypeInfo<SC::Buffer>
{
    static constexpr bool IsPacked = false;

    [[nodiscard]] static auto size(const Buffer& object) { return object.size(); }
    [[nodiscard]] static auto data(Buffer& object) { return object.data(); }
    [[nodiscard]] static bool resizeWithoutInitializing(Buffer& object, size_t newSize)
    {
        return object.resizeWithoutInitializing(newSize);
    }
    [[nodiscard]] static bool resize(Buffer& object, size_t newSize) { return object.resize(newSize, 0); }
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
