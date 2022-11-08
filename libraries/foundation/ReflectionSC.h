#pragma once
#include "Array.h"
#include "Map.h"
#include "Reflection.h"
#include "String.h"
#include "Vector.h"

namespace SC
{
namespace Reflection
{
template <typename Container, typename ItemType, int N = -1>
struct VectorArrayVTable
{
    template <typename MemberVisitor>
    constexpr static void build(MemberVisitor& builder)
    {
        if (builder.vectorVtable.capacity > 0)
        {
            VectorVTable vector;
            vector.resize = &resize;
            assignResizeWithoutInitialize(vector);
            vector.getSegmentSpan      = &getSegmentSpan<void>;
            vector.getSegmentSpanConst = &getSegmentSpan<const void>;
            vector.linkID              = builder.initialSize + builder.atoms.size;
            builder.vectorVtable.push(vector);
        }
    }

    static bool resize(Span<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
                       VectorVTable::DropEccessItems dropEccessItems)
    {
        if (object.size >= sizeof(void*))
        {
            auto& vectorByte = *static_cast<Container*>(object.data);
            auto  numItems   = sizeInBytes / sizeof(ItemType);
            if (N >= 0)
                numItems = min(numItems, static_cast<decltype(numItems)>(N));
            return vectorByte.resize(numItems);
        }
        else
        {
            return false;
        }
    }
    static bool resizeWithoutInitialize(Span<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
                                        VectorVTable::DropEccessItems dropEccessItems)
    {
        if (object.size >= sizeof(void*))
        {
            auto& vectorByte = *static_cast<Container*>(object.data);
            auto  numItems   = sizeInBytes / sizeof(ItemType);
            if (N >= 0)
                numItems = min(numItems, static_cast<decltype(numItems)>(N));
            return vectorByte.resizeWithoutInitializing(numItems);
        }
        else
        {
            return false;
        }
    }

    template <typename VoidType>
    [[nodiscard]] static constexpr bool getSegmentSpan(Reflection::MetaProperties property, Span<VoidType> object,
                                                       Span<VoidType>& itemBegin)
    {
        if (object.size >= sizeof(void*))
        {
            typedef typename SameConstnessAs<VoidType, Container>::type VectorType;
            auto& vectorByte = *static_cast<VectorType*>(object.data);
            itemBegin        = Span<VoidType>(vectorByte.data(), vectorByte.size() * sizeof(ItemType));
            return true;
        }
        else
        {
            return false;
        }
    }
    template <typename Q = ItemType>
    [[nodiscard]] static typename EnableIf<not IsTriviallyCopyable<Q>::value, void>::type //
        constexpr assignResizeWithoutInitialize(VectorVTable& vector)
    {}

    template <typename Q = ItemType>
    [[nodiscard]] static typename EnableIf<IsTriviallyCopyable<Q>::value, void>::type //
        constexpr assignResizeWithoutInitialize(VectorVTable& vector)
    {
        vector.resizeWithoutInitialize = &resizeWithoutInitialize;
    }
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
        VectorArrayVTable<SC::Array<T, N>, T, N>::template build<MemberVisitor>(builder);
        Atom arrayHeader                   = Atom::create<SC::Array<T, N>>("SC::Array<T, N>");
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
        VectorArrayVTable<SC::Vector<T>, T>::template build<MemberVisitor>(builder);
        Atom vectorHeader                   = Atom::create<SC::Vector<T>>("SC::Vector");
        vectorHeader.properties.numSubAtoms = 1;
        vectorHeader.properties.setCustomUint32(sizeof(T));
        builder.atoms.push(vectorHeader);
        builder.atoms.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), TypeToString<T>::get(),
                            &MetaClass<T>::build});
    }
};

SC_META_STRUCT_BEGIN(SC::String)
SC_META_STRUCT_MEMBER(0, data)
SC_META_STRUCT_END()

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
