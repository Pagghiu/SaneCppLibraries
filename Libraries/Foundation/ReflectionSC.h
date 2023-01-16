// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Array.h"
#include "Map.h"
#include "Reflection.h"
#include "ReflectionClassInfo.h"
#include "String.h"
#include "Vector.h"

namespace SC
{
namespace Reflection
{

template <typename MemberVisitor, typename Container, typename ItemType, int N>
struct VectorArrayVTable
{
    static constexpr void build(MemberVisitor&) {}
};
template <typename T>
struct ClassInfo<SC::Vector<T>>
{
    static constexpr bool IsPacked = false;
};
template <typename T, int N>
struct ClassInfo<SC::Array<T, N>>
{
    static constexpr bool IsPacked = false;
};
} // namespace Reflection
} // namespace SC

template <typename T, int N>
struct SC::Reflection::MetaClass<SC::Array<T, N>>
{
    static constexpr MetaType getMetaType() { return MetaType::TypeVector; }
    template <typename MemberVisitor>
    static constexpr void build(MemberVisitor& builder)
    {
        typedef typename MemberVisitor::Atom Atom;
        // TODO: It's probably possible to add MemberVisitor as template param of MetaClass to remove this
        VectorArrayVTable<MemberVisitor, SC::Array<T, N>, T, N>::build(builder);
        auto arrayHeader                   = Atom::template create<SC::Array<T, N>>("SC::Array<T, N>");
        arrayHeader.properties.numSubAtoms = 1;
        arrayHeader.properties.setCustomUint32(N);
        builder.atoms.push(arrayHeader);
        builder.atoms.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), TypeToString<T>::get(),
                            &MetaClass<T>::build});
    }
};

template <typename T>
struct SC::Reflection::MetaClass<SC::Vector<T>>
{
    static constexpr MetaType getMetaType() { return MetaType::TypeVector; }

    template <typename MemberVisitor>
    static constexpr void build(MemberVisitor& builder)
    {
        typedef typename MemberVisitor::Atom Atom;
        VectorArrayVTable<MemberVisitor, SC::Vector<T>, T, -1>::build(builder);
        auto vectorHeader                   = Atom::template create<SC::Vector<T>>("SC::Vector");
        vectorHeader.properties.numSubAtoms = 1;
        vectorHeader.properties.setCustomUint32(sizeof(T));
        builder.atoms.push(vectorHeader);
        builder.atoms.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), TypeToString<T>::get(),
                            &MetaClass<T>::build});
    }
};

SC_META_STRUCT_VISIT(SC::String)
SC_META_STRUCT_FIELD(0, data)
SC_META_STRUCT_LEAVE()

namespace SC
{
namespace Reflection
{
template <typename Key, typename Value, typename Container>
struct MetaClass<Map<Key, Value, Container>> : MetaStruct<MetaClass<Map<Key, Value, Container>>>
{
    typedef typename MetaStruct<MetaClass<SC::Map<Key, Value, Container>>>::T T;
    template <typename MemberVisitor>
    static constexpr void visit(MemberVisitor&& builder)
    {
        builder(0, SC_META_MEMBER(items));
    }
};

} // namespace Reflection
} // namespace SC
