// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Libraries/Reflection/Reflection.h"
#include "ReflectionAuto.h"

namespace SC
{
namespace Reflection
{
namespace AutoAggregates
{
template <typename T, int... NN>
constexpr int EnumerateAggregates(...)
{
    return sizeof...(NN) - 1;
}

template <typename T, int... NN>
constexpr auto EnumerateAggregates(int) -> decltype(T{{Auto::c_op<T, NN>{}}...}, 0)
{
    return EnumerateAggregates<T, NN..., sizeof...(NN)>(0);
}

template <typename T>
using TypeListFor = typename Auto::type_list<T, Auto::MakeIntegerSequence<int, EnumerateAggregates<T>(0)>>::type;

template <typename T, typename MemberVisitor, int NumMembers>
struct DescribeLoopholeVisitor
{
    MemberVisitor& builder;
    size_t         currentOffset = 0;

    template <int Order, typename R>
    constexpr bool visit()
    {
        R T::*ptr = nullptr;
        // Simulate offsetof(T, f) under assumption that user is not manipulating members packing.
        currentOffset          = (currentOffset + alignof(R) - 1) & ~(alignof(R) - 1);
        const auto fieldOffset = currentOffset;
        currentOffset += sizeof(R);
        return builder(Order, ptr, "", fieldOffset);
    }
};

template <typename T>
struct DescribeAutomaticAggregates
{
    using TypeList = AutoAggregates::TypeListFor<T>;
    [[nodiscard]] static constexpr TypeCategory getCategory() { return TypeCategory::TypeStruct; }

    template <typename MemberVisitor>
    [[nodiscard]] static constexpr bool build(MemberVisitor& builder)
    {
        if (not builder.addType(MemberVisitor::Type::template createStruct<T>()))
            return false;
        if (not visit(builder))
            return false;
        return true;
    }

    template <typename MemberVisitor>
    static constexpr bool visit(MemberVisitor&& builder)
    {
        return Auto::TypeListVisit<TypeList, TypeList::size>::visit(
            DescribeLoopholeVisitor<T, MemberVisitor, TypeList::size>{builder});
    }
};

} // namespace AutoAggregates

template <typename Class>
struct Reflect : public AutoAggregates::DescribeAutomaticAggregates<Class>
{
};

} // namespace Reflection
} // namespace SC
