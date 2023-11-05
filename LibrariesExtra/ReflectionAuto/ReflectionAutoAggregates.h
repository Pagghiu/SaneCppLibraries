// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "ReflectionAuto.h"
#include "../../Libraries/Reflection/ReflectionMetaType.h"

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
struct MetaClassLoopholeVisitor
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
        return builder(Order, "", ptr, fieldOffset);
    }
};

template <typename T>
struct MetaClassAutomaticAggregates
{
    using TypeList = AutoAggregates::TypeListFor<T>;
    [[nodiscard]] static constexpr MetaType getMetaType() { return MetaType::TypeStruct; }

    template <typename MemberVisitor>
    static constexpr void build(MemberVisitor& builder)
    {
        builder.atoms.template Struct<T>();
        visit(builder);
    }

    template <typename MemberVisitor>
    static constexpr bool visit(MemberVisitor&& builder)
    {
        return Auto::MetaTypeListVisit<TypeList, TypeList::size>::visit(
            MetaClassLoopholeVisitor<T, MemberVisitor, TypeList::size>{builder});
    }

    template <typename MemberVisitor>
    struct VisitObjectAdapter
    {
        MemberVisitor& builder;
        T&             object;

        // Cannot be constexpr as we're reinterpret_cast-ing
        template <typename R, int N>
        /*constexpr*/ bool operator()(int order, const char (&name)[N], R T::*, size_t offset) const
        {
            R& member = *reinterpret_cast<R*>(reinterpret_cast<uint8_t*>(&object) + offset);
            return builder(order, name, member);
        }
    };

    // Cannot be constexpr as we're reinterpret_cast-ing
    template <typename MemberVisitor>
    static /*constexpr*/ bool visitObject(MemberVisitor&& builder, T& object)
    {
        return visit(VisitObjectAdapter<MemberVisitor>{builder, object});
    }
};

} // namespace AutoAggregates

template <typename Class>
struct MetaClass : public AutoAggregates::MetaClassAutomaticAggregates<Class>
{
};

} // namespace Reflection
} // namespace SC
