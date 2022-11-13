#pragma once
#include "ConstexprTypes.h"
#include "ReflectionClassInfo.h"
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
    MetaType type;        // 1
    int8_t   numSubAtoms; // 1
    uint16_t order;       // 2
    uint16_t offset;      // 2
    uint16_t size;        // 2

    constexpr MetaProperties() : type(MetaType::TypeInvalid), order(0), offset(0), size(0), numSubAtoms(0)
    {
        static_assert(sizeof(MetaProperties) == 8, "Size must be 8 bytes");
    }
    constexpr MetaProperties(MetaType type, uint8_t order, uint16_t offset, uint16_t size, int8_t numSubAtoms)
        : type(type), order(order), offset(offset), size(size), numSubAtoms(numSubAtoms)
    {}
    constexpr void                   setLinkIndex(int8_t linkIndex) { numSubAtoms = linkIndex; }
    [[nodiscard]] constexpr int8_t   getLinkIndex() const { return numSubAtoms; }
    [[nodiscard]] constexpr uint32_t getCustomUint32() const { return (offset << 16) | order; }
    constexpr void                   setCustomUint32(uint32_t N)
    {
        const uint16_t lowN  = N & 0xffff;
        const uint16_t highN = (N >> 16) & 0xffff;
        order                = static_cast<uint8_t>(lowN);
        offset               = static_cast<uint8_t>(highN);
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

template <typename T> struct MetaClass;

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
            SC_DISABLE_OFFSETOF_WARNING

#define SC_META_MEMBER(MEMBER) #MEMBER, &T::MEMBER, SC_OFFSET_OF(T, MEMBER)
#define SC_META_STRUCT_FIELD(ORDER, MEMBER)                                                                            \
    if (not builder(ORDER, #MEMBER, &T::MEMBER, SC_OFFSET_OF(T, MEMBER)))                                              \
    {                                                                                                                  \
        return false;                                                                                                  \
    }

#define SC_META_STRUCT_LEAVE()                                                                                         \
    SC_ENABLE_OFFSETOF_WARNING                                                                                         \
    return true;                                                                                                       \
    }                                                                                                                  \
    }                                                                                                                  \
    ;
