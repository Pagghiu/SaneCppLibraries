// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Memory/Buffer.h"
#include "../Memory/String.h"
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

template <>
struct Reflect<SC::Buffer>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeVector; }

    template <typename MemberVisitor>
    static constexpr bool build(MemberVisitor& builder)
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
struct ExtendedTypeInfo<SC::Buffer, void>
{
    static constexpr bool IsPacked = false;

    static auto size(const Buffer& object) { return object.size(); }
    static auto data(Buffer& object) { return object.data(); }
    static bool resizeWithoutInitializing(Buffer& object, size_t newSize)
    {
        return object.resizeWithoutInitializing(newSize);
    }
    static bool resize(Buffer& object, size_t newSize) { return object.resize(newSize, 0); }
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
