// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "ConstexprTypes.h"
#include "ReflectionClassInfo.h"
#if SC_CPP_LESS_THAN_20
#include "ReflectionAutoAggregates.h"
#else
#include "ReflectionAutoStructured.h"
#endif
#include "Types.h"

namespace SC
{
namespace Reflection
{
struct MetaStructFlags
{
    static const uint32_t IsPacked = 1 << 1; // IsPacked AND No padding in every contained field (recursively)
};

enum class MetaType : uint8_t
{
    // Invalid sentinel
    TypeInvalid = 0,

    // Primitive types
    TypeUINT8    = 1,
    TypeUINT16   = 2,
    TypeUINT32   = 3,
    TypeUINT64   = 4,
    TypeINT8     = 5,
    TypeINT16    = 6,
    TypeINT32    = 7,
    TypeINT64    = 8,
    TypeFLOAT32  = 9,
    TypeDOUBLE64 = 10,

    TypeStruct = 11,
    TypeArray  = 12,
    TypeVector = 13,
};

struct MetaProperties
{
    MetaType type;          // 1
    int8_t   numSubAtoms;   // 1
    uint16_t order;         // 2
    uint16_t offsetInBytes; // 2
    uint16_t sizeInBytes;   // 2

    constexpr MetaProperties() : type(MetaType::TypeInvalid), order(0), offsetInBytes(0), sizeInBytes(0), numSubAtoms(0)
    {
        static_assert(sizeof(MetaProperties) == 8, "Size must be 8 bytes");
    }
    constexpr MetaProperties(MetaType type, uint8_t order, uint16_t offsetInBytes, uint16_t sizeInBytes,
                             int8_t numSubAtoms)
        : type(type), order(order), offsetInBytes(offsetInBytes), sizeInBytes(sizeInBytes), numSubAtoms(numSubAtoms)
    {}
    constexpr void                   setLinkIndex(int8_t linkIndex) { numSubAtoms = linkIndex; }
    [[nodiscard]] constexpr int8_t   getLinkIndex() const { return numSubAtoms; }
    [[nodiscard]] constexpr uint32_t getCustomUint32() const { return (offsetInBytes << 16) | order; }
    constexpr void                   setCustomUint32(uint32_t N)
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

// clang-format off
struct MetaPrimitive { template<typename MemberVisitor>  static constexpr void build( MemberVisitor& builder) { } };

template<typename data_tlist, int N>
struct CallVisitorFor
{
    template<typename Visitor>
    constexpr static bool visit(Visitor&& visitor)
    {
        typedef luple::tlist_get_t<data_tlist, N-1> R;
        return CallVisitorFor<data_tlist, N-1>::visit(forward(visitor)) and visitor.template visit<N-1, R>();
    }
};
template<typename data_tlist>
struct CallVisitorFor<data_tlist,  0>
{
    template<typename Visitor>
    constexpr static bool visit(Visitor&& visitor)
    {
        return true;
    }
};


#if SC_CPP_AT_LEAST_20
template<typename T, typename MemberVisitor, int NumMembers>
struct MetaClassLoopholeVisitor
{
    MemberVisitor& builder;

    template<int Order, typename R>
    constexpr bool visit()
    {
        R T::* ptr = nullptr;
        constexpr auto fieldOffset = MemberOffsetOf<T, R, Order, NumMembers>();
        return builder(Order, "", ptr, fieldOffset);
    }
};
template <typename T> struct MetaClassAutomaticStructured
{
    using TypeList = loophole_structured::TypeListFor<T>;
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
        return CallVisitorFor<TypeList, TypeList::size>::visit(MetaClassLoopholeVisitor<T, MemberVisitor, TypeList::size>{builder});
    }
    
    template <typename MemberVisitor>
    struct VisitObjectAdapter
    {
        MemberVisitor& builder;
        T& object;
        int order = 0;
        
        template <typename FirstType, typename... Types>
        constexpr bool operator()(FirstType& first, Types&... types)
        {
            return builder(order++, "", first) and operator()(types...);
        }
    
        constexpr bool operator()() { return true; }
    };
    
    template <typename MemberVisitor>
    static constexpr bool visitObject(MemberVisitor&& builder, T& object)
    {
        constexpr auto NumMembers = loophole_structured::CountNumMembers<T>(0);
        return Reflection::MemberApply<NumMembers>(object, VisitObjectAdapter<MemberVisitor>{builder, object});
    }
};


template<typename Class>
struct MetaClass : public  MetaClassAutomaticStructured<Class>{};
// we are using a specific macro for auto member binding to keep explicit track of which fields are being actually "automatically" tracked
#define SC_META_STRUCT_AUTO_BINDINGS(Class)
#elif SC_META_ENABLE_CPP14_AUTO_REFLECTION
template<typename T, typename MemberVisitor, int NumMembers>
struct MetaClassLoopholeVisitor
{
    MemberVisitor& builder;
    int currentOffset = 0;

    template<int Order, typename R>
    constexpr bool visit()
    {
        R T::* ptr = nullptr;
        // Simulate offsetof(T, f) under assumption that user is not manipulating members packing.
        currentOffset = (currentOffset + alignof(R) - 1) & ~(alignof(R) - 1);
        const auto fieldOffset = currentOffset;
        currentOffset += sizeof(R);
        return builder(Order, "", ptr, fieldOffset);
    }
};
template <typename T> struct MetaClassAutomaticAggregates
{
    using TypeList = loophole_aggregates::TypeListFor<T>;
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
        return CallVisitorFor<TypeList, TypeList::size>::visit(MetaClassLoopholeVisitor<T, MemberVisitor, TypeList::size>{builder});
    }
    
    template <typename MemberVisitor>
    struct VisitObjectAdapter
    {
        MemberVisitor& builder;
        T& object;
        
        // Cannot be constexpr as we're reinterpret_cast-ing
        template <typename R, int N>
        /*constexpr*/ bool operator()(int order, const char (&name)[N], R T::*field, size_t offset) const
        {
            R& member =  *reinterpret_cast<R*>(reinterpret_cast<uint8_t*>(&object) + offset);
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
template<typename Class>
struct MetaClass : public  MetaClassAutomaticAggregates<Class>{};
// we are using a specific macro for auto member binding to keep explicit track of which fields are being actually "automatically" tracked
#define SC_META_STRUCT_AUTO_BINDINGS(Class)
#else

#define SC_META_STRUCT_AUTO_BINDINGS(Class) static_assert(0, "You need SC_META_ENABLE_CPP14_AUTO_REFLECTION or C++ 20 enabled to use this feature");

#endif

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
struct MetaArrayView
{
    int&  size;
    int   wantedCapacity;
    Type* output;
    int   capacity;

    constexpr MetaArrayView(int& size, Type* output = nullptr, const int capacity = 0)
        : size(size), wantedCapacity(0), output(nullptr), capacity(0)
    {
        init(output, capacity);
    }

    constexpr void init(Type* initOutput, int initCapacity)
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

    template <typename R, typename T, int N>
    [[nodiscard]] constexpr bool operator()(int order, const char (&name)[N], R T::*field, size_t offset)
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

    MetaProperties      properties;
    ConstexprStringView name;
    MetaClassBuildFunc  build;

    constexpr AtomBase() : build(nullptr) {}
    constexpr AtomBase(const MetaProperties properties, ConstexprStringView name, MetaClassBuildFunc build)
        : properties(properties), name(name), build(build)
    {}

    template <typename R, typename T, int N>
    [[nodiscard]] static constexpr AtomBase create(int order, const char (&name)[N], R T::*, size_t offset)
    {
        return {MetaProperties(MetaClass<R>::getMetaType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1),
                ConstexprStringView(name, N), &MetaClass<R>::build};
    }

    template <typename T>
    [[nodiscard]] static constexpr AtomBase create(ConstexprStringView name = TypeToString<T>::get())
    {
        AtomBase atom = {MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), name, &MetaClass<T>::build};
        if (ClassInfo<T>::IsPacked)
        {
            atom.properties.setCustomUint32(MetaStructFlags::IsPacked);
        }
        return atom;
    }
};

template <typename MemberVisitor>
struct MetaClassBuilder
{
    typedef AtomBase<MemberVisitor> Atom;

    int                 atomsSize;
    MetaArrayView<Atom> atoms;
    uint32_t            initialSize;
    constexpr MetaClassBuilder(Atom* output = nullptr, const int capacity = 0)
        : atomsSize(0), atoms(atomsSize, output, capacity), initialSize(0)
    {}

    template <typename R, typename T, int N>
    [[nodiscard]] constexpr bool operator()(int order, const char (&name)[N], R T::*field, size_t offset)
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
