// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#if SC_META_ENABLE_AUTO_REFLECTION
#if SC_CPP_LESS_THAN_20
#include "ReflectionAutoAggregates.h"
#else
#include "ReflectionAutoStructured.h"
#endif
#endif

#include "ReflectionFoundation.h"
#include "ReflectionMetaType.h"

namespace SC
{
namespace Reflection
{
struct MetaProperties
{
    MetaType type;          // 1
    int8_t   numSubAtoms;   // 1
    uint16_t order;         // 2
    uint16_t offsetInBytes; // 2
    uint16_t sizeInBytes;   // 2

    constexpr MetaProperties() : type(MetaType::TypeInvalid), numSubAtoms(0), order(0), offsetInBytes(0), sizeInBytes(0)
    {
        static_assert(sizeof(MetaProperties) == 8, "Size must be 8 bytes");
    }

    constexpr MetaProperties(MetaType type, uint8_t order, uint16_t offsetInBytes, uint16_t sizeInBytes,
                             int8_t numSubAtoms)
        : type(type), numSubAtoms(numSubAtoms), order(order), offsetInBytes(offsetInBytes), sizeInBytes(sizeInBytes)
    {}

    [[nodiscard]] constexpr int8_t getLinkIndex() const { return numSubAtoms; }

    [[nodiscard]] constexpr bool setLinkIndex(ssize_t linkIndex)
    {
        if (linkIndex > static_cast<decltype(numSubAtoms)>(MaxValue()))
            return false;
        numSubAtoms = static_cast<decltype(numSubAtoms)>(linkIndex);
        return true;
    }

    [[nodiscard]] constexpr uint32_t getCustomUint32() const
    {
        return (static_cast<uint32_t>(offsetInBytes) << 16) | order;
    }

    constexpr void setCustomUint32(uint32_t N)
    {
        const uint16_t lowN  = N & 0xffff;
        const uint16_t highN = (N >> 16) & 0xffff;
        order                = static_cast<uint8_t>(lowN);
        offsetInBytes        = static_cast<uint8_t>(highN);
    }

    [[nodiscard]] constexpr bool isPrimitiveType() const
    {
        return type >= MetaType::TypeUINT8 && type <= MetaType::TypeDOUBLE64;
    }

    [[nodiscard]] constexpr bool isPrimitiveOrRecursivelyPacked() const
    {
        if (isPrimitiveType())
            return true;
        if (type == Reflection::MetaType::TypeStruct)
        {
            if (getCustomUint32() & Reflection::MetaStructFlags::IsPacked)
            {
                return true;
            }
        }
        return false;
    }
};

struct MetaPrimitive
{
    template <typename MemberVisitor>
    static constexpr void build(MemberVisitor&)
    {}
};

// clang-format off
template <> struct MetaClass<char_t>   : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeUINT8;}};
template <> struct MetaClass<uint8_t>  : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeUINT8;}};
template <> struct MetaClass<uint16_t> : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeUINT16;}};
template <> struct MetaClass<uint32_t> : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeUINT32;}};
template <> struct MetaClass<uint64_t> : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeUINT64;}};
template <> struct MetaClass<int8_t>   : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeINT8;}};
template <> struct MetaClass<int16_t>  : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeINT16;}};
template <> struct MetaClass<int32_t>  : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeINT32;}};
template <> struct MetaClass<int64_t>  : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeINT64;}};
template <> struct MetaClass<float>    : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeFLOAT32;}};
template <> struct MetaClass<double>   : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeDOUBLE64;}};
// clang-format on

template <typename Type>
struct SizedArrayView
{
    uint32_t& size;
    uint32_t  wantedCapacity;
    Type*     output;
    uint32_t  capacity;

    constexpr SizedArrayView(uint32_t& size, Type* output = nullptr, const uint32_t capacity = 0)
        : size(size), wantedCapacity(0), output(nullptr), capacity(0)
    {
        init(output, capacity);
    }

    constexpr void init(Type* initOutput, uint32_t initCapacity)
    {
        size           = 0;
        wantedCapacity = 0;
        output         = initOutput;
        capacity       = initCapacity;
    }

    constexpr void push(const Type& value)
    {
        if (size < capacity)
        {
            output[size] = value;
            size++;
        }
        wantedCapacity++;
    }

    template <typename T>
    constexpr void Struct()
    {
        push(Type::template create<T>());
    }

    template <typename R, typename T, uint32_t N>
    [[nodiscard]] constexpr bool operator()(uint8_t order, const char (&name)[N], R T::*field, size_t offset)
    {
        push(Type::create(order, name, field, offset));
        return true;
    }

    constexpr bool capacityWasEnough() const { return wantedCapacity == size; }
};

template <typename MemberVisitor>
struct AtomBase
{
    typedef void (*MetaClassBuildFunc)(MemberVisitor& builder);

    MetaProperties     properties;
    SymbolStringView   name;
    MetaClassBuildFunc build;

    constexpr AtomBase() : build(nullptr) {}
    constexpr AtomBase(const MetaProperties properties, SymbolStringView name, MetaClassBuildFunc build)
        : properties(properties), name(name), build(build)
    {}

    template <typename R, typename T, int N>
    [[nodiscard]] static constexpr AtomBase create(uint8_t order, const char (&name)[N], R T::*, size_t offset)
    {
        return {MetaProperties(MetaClass<R>::getMetaType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1),
                SymbolStringView(name, N), &MetaClass<R>::build};
    }

    template <typename T>
    [[nodiscard]] static constexpr AtomBase create(SymbolStringView name = TypeToString<T>::get())
    {
        AtomBase atom = {MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), name, &MetaClass<T>::build};
        if (MetaTypeInfo<T>::IsPacked)
        {
            atom.properties.setCustomUint32(MetaStructFlags::IsPacked);
        }
        return atom;
    }
};

template <typename MemberVisitor>
struct MetaClassBuilder
{
    struct EmptyVTables
    {
    };
    EmptyVTables                    vtables;
    typedef AtomBase<MemberVisitor> Atom;

    uint32_t             atomsSize;
    uint32_t             initialSize;
    SizedArrayView<Atom> atoms;
    constexpr MetaClassBuilder(Atom* output = nullptr, const uint32_t capacity = 0)
        : atomsSize(0), initialSize(0), atoms(atomsSize, output, capacity)
    {}

    template <typename R, typename T, int N>
    [[nodiscard]] constexpr bool operator()(uint8_t order, const char (&name)[N], R T::*field, size_t offset)
    {
        return atoms(order, name, field, offset);
    }
};

template <typename T, size_t N>
struct MetaClass<T[N]>
{
    static constexpr MetaType getMetaType() { return MetaType::TypeArray; }

    template <typename MemberVisitor>
    static constexpr void build(MemberVisitor& builder)
    {
        typename MemberVisitor::Atom arrayHeader = {MetaProperties(getMetaType(), 0, 0, sizeof(T[N]), 1), "Array",
                                                    nullptr};
        arrayHeader.properties.setCustomUint32(N);
        builder.atoms.push(arrayHeader);
        builder.atoms.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), TypeToString<T>::get(),
                            &MetaClass<T>::build});
    }
};
template <typename Type>
struct MetaStruct;

template <typename Type>
struct MetaStruct<MetaClass<Type>>
{
    typedef Type T;

    [[nodiscard]] static constexpr MetaType getMetaType() { return MetaType::TypeStruct; }

    template <typename MemberVisitor>
    static constexpr void build(MemberVisitor& builder)
    {
        builder.atoms.template Struct<T>();
        MetaClass<Type>::visit(builder);
    }

    template <typename MemberVisitor>
    struct VisitObjectAdapter
    {
        MemberVisitor& builder;
        Type&          object;

        template <typename R, int N>
        constexpr bool operator()(int order, const char (&name)[N], R Type::*field, size_t offset) const
        {
            SC_UNUSED(offset);
            return builder(order, name, object.*field);
        }
    };

    template <typename MemberVisitor>
    static constexpr bool visitObject(MemberVisitor&& builder, Type& object)
    {
        return MetaClass<Type>::visit(VisitObjectAdapter<MemberVisitor>{builder, object});
    }
};

} // namespace Reflection
} // namespace SC

#define SC_META_STRUCT_VISIT(StructName)                                                                               \
    template <>                                                                                                        \
    struct SC::Reflection::MetaClass<StructName> : SC::Reflection::MetaStruct<MetaClass<StructName>>                   \
    {                                                                                                                  \
        template <typename MemberVisitor>                                                                              \
        static constexpr bool visit(MemberVisitor&& builder)                                                           \
        {                                                                                                              \
            SC_DISABLE_OFFSETOF_WARNING                                                                                \
            return true

#define SC_META_MEMBER(MEMBER)              #MEMBER, &T::MEMBER, SC_OFFSETOF(T, MEMBER)
#define SC_META_STRUCT_FIELD(ORDER, MEMBER) and builder(ORDER, #MEMBER, &T::MEMBER, SC_OFFSETOF(T, MEMBER))

#define SC_META_STRUCT_LEAVE()                                                                                         \
    ;                                                                                                                  \
    SC_ENABLE_OFFSETOF_WARNING                                                                                         \
    }                                                                                                                  \
    }                                                                                                                  \
    ;
