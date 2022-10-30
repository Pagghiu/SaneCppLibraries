#pragma once
#include "Array.h"
#include "Reflection.h"
#include "String.h"
#include "Vector.h"

template <typename T, SC::size_t N>
struct SC::Reflection::MetaClass<SC::Array<T, N>>
{
    static constexpr MetaType getMetaType() { return MetaType::TypeSCArray; }
    static constexpr void     build(MetaClassBuilder& builder)
    {
        Atom arrayHeader = {MetaProperties(getMetaType(), 0, 0, sizeof(SC::Array<T, N>), 1), "Array", nullptr};
        arrayHeader.properties.setCustomUint32(N);
        builder.push(arrayHeader);
        builder.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), MetaTypeToString<T>::get(),
                      &MetaClass<T>::build});
    }
};

template <typename T>
struct SC::Reflection::MetaClass<SC::Vector<T>>
{
    static constexpr MetaType getMetaType() { return MetaType::TypeSCVector; }
    static constexpr void     build(MetaClassBuilder& builder)
    {
        auto vectorHeader = Atom::create<SC::Vector<T>>("SC::Vector");
        vectorHeader.properties.setCustomUint32(sizeof(T));
        builder.push(vectorHeader);
        builder.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), MetaTypeToString<T>::get(),
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
    static constexpr void members(MetaClassBuilder& builder) { builder.member(0, SC_META_MEMBER(items)); }
};

} // namespace Reflection
} // namespace SC
