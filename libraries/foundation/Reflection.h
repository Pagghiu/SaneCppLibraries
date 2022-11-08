#pragma once
#include "ConstexprTypes.h"
#include "FlatSchemaCompiler.h"
#include "Types.h"

namespace SC
{
namespace Reflection
{

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
};

struct MetaClassBuilder;
// clang-format off
struct MetaPrimitive { static constexpr void build( MetaClassBuilder& builder) { } };

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

struct Atom;

struct Atom
{
    typedef void (*MetaClassBuildFunc)(MetaClassBuilder& builder);

    MetaProperties      properties;
    ConstexprStringView name;
    MetaClassBuildFunc  build;

    constexpr Atom() : build(nullptr) {}
    constexpr Atom(const MetaProperties properties, ConstexprStringView name, MetaClassBuildFunc build)
        : properties(properties), name(name), build(build)
    {}

    template <typename R, typename T, int N>
    [[nodiscard]] static constexpr Atom create(int order, const char (&name)[N], R T::*, size_t offset)
    {
        return {MetaProperties(MetaClass<R>::getMetaType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1),
                ConstexprStringView(name, N), &MetaClass<R>::build};
    }

    template <typename T>
    [[nodiscard]] static constexpr Atom create(ConstexprStringView name = TypeToString<T>::get())
    {
        return {MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), name, &MetaClass<T>::build};
    }
};

template <typename Type>
struct MetaArrayView
{
    int   size;
    int   wantedCapacity;
    Type* output;
    int   capacity;

    constexpr MetaArrayView(Type* output = nullptr, const int capacity = 0)
        : size(0), wantedCapacity(0), output(nullptr), capacity(0)
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
        push(Atom::create<T>());
    }

    template <typename R, typename T, int N>
    [[nodiscard]] constexpr bool operator()(int order, const char (&name)[N], R T::*field, size_t offset)
    {
        push(Atom::create(order, name, field, offset));
        return true;
    }

    constexpr bool capacityWasEnough() const { return wantedCapacity == size; }
};
struct VectorVTable
{
    enum class DropEccessItems
    {
        No,
        Yes
    };

    typedef bool (*FunctionGetSegmentSpan)(MetaProperties property, Span<void> object, Span<void>& itemBegin);
    typedef bool (*FunctionGetSegmentSpanConst)(MetaProperties property, Span<const void> object,
                                                Span<const void>& itemBegin);

    typedef bool (*FunctionResize)(Span<void> object, Reflection::MetaProperties property, uint64_t sizeInBytes,
                                   DropEccessItems dropEccessItems);
    typedef bool (*FunctionResizeWithoutInitialize)(Span<void> object, Reflection::MetaProperties property,
                                                    uint64_t sizeInBytes, DropEccessItems dropEccessItems);
    FunctionGetSegmentSpan          getSegmentSpan;
    FunctionGetSegmentSpanConst     getSegmentSpanConst;
    FunctionResize                  resize;
    FunctionResizeWithoutInitialize resizeWithoutInitialize;
    uint32_t                        linkID;
    constexpr VectorVTable()
        : getSegmentSpan(nullptr), getSegmentSpanConst(nullptr), resize(nullptr), resizeWithoutInitialize(nullptr),
          linkID(0)
    {}
};
struct MetaClassBuilder
{
    MetaArrayView<Atom>         atoms;
    MetaArrayView<VectorVTable> vectorVtable;
    uint32_t                    initialSize;
    constexpr MetaClassBuilder(Atom* output = nullptr, const int capacity = 0) : atoms(output, capacity), initialSize(0)
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
    static constexpr void     build(MetaClassBuilder& builder)
    {
        Atom arrayHeader = {MetaProperties(getMetaType(), 0, 0, sizeof(T[N]), 1), "Array", nullptr};
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

    static constexpr void build(MetaClassBuilder& builder)
    {
        builder.atoms.Struct<T>();
        MetaClass<Type>::visit(builder);
    }
};

template <int MAX_VTABLES>
struct ReflectionVTables
{
    ConstexprArray<VectorVTable, MAX_VTABLES> vector;
};

struct FlatSchemaCompiler
{
    static const int MAX_VTABLES = 100;
    enum class MetaStructFlags : uint32_t
    {
        IsPacked            = 1 << 1, // No padding between members of a Struct
        IsRecursivelyPacked = 1 << 2, // IsPacked AND No padding in every contained field (recursively)
    };
    struct MetaClassBuilder1 : public MetaClassBuilder
    {
        ReflectionVTables<MAX_VTABLES> payload;
        constexpr MetaClassBuilder1(Atom* output = nullptr, const int capacity = 0) : MetaClassBuilder(output, capacity)
        {
            if (capacity > 0)
            {
                vectorVtable.init(payload.vector.values, MAX_VTABLES);
            }
        }
    };
    typedef FlatSchemaCompilerBase::FlatSchemaCompilerBase<MetaProperties, Atom, MetaClassBuilder1> FlatSchemaBase;

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_ATOMS: maximum number of atoms (struct members). When using constexpr it will trim it to actual size.
    template <typename T, int MAX_LINK_BUFFER_SIZE = 20, int MAX_TOTAL_ATOMS = 100>
    static constexpr auto compile()
    {
        constexpr auto schema =
            FlatSchemaBase::compileAllAtomsFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_ATOMS>(&MetaClass<T>::build);
        static_assert(schema.atoms.size > 0, "Something failed in compileAllAtomsFor");
        FlatSchemaBase::FlatSchema<schema.atoms.size> result;
        for (int i = 0; i < schema.atoms.size; ++i)
        {
            result.properties.values[i] = schema.atoms.values[i].properties;
            result.names.values[i]      = schema.atoms.values[i].name;
        }
        result.properties.size = schema.atoms.size;
        result.names.size      = schema.atoms.size;
        result.payload         = schema.payload;
        // TODO: This is really ugly
        while (schema.payload.vector.values[result.payload.vector.size++].resize != nullptr)
            ;
        markPackedStructs(result, 0);
        return result;
    }
    [[nodiscard]] static constexpr bool areAllMembersPacked(const MetaProperties* properties, int numAtoms)
    {
        uint32_t totalSize = 0;
        for (int idx = 0; idx < numAtoms; ++idx)
        {
            totalSize += properties[idx + 1].size;
        }
        return totalSize == properties->size;
    }

  private:
    template <int MAX_TOTAL_ATOMS>
    static constexpr bool markPackedStructs(FlatSchemaBase::FlatSchema<MAX_TOTAL_ATOMS>& result, int startIdx)
    {
        MetaProperties& atom = result.properties.values[startIdx];
        if (atom.isPrimitiveType())
        {
            return true; // packed by definition
        }
        else if (atom.type == MetaType::TypeStruct)
        {
            // packed if is itself packed and all of its non primitive members are packed
            if (not(atom.getCustomUint32() & static_cast<uint32_t>(MetaStructFlags::IsPacked)))
            {
                if (areAllMembersPacked(&atom, atom.numSubAtoms))
                {
                    atom.setCustomUint32(atom.getCustomUint32() | static_cast<uint32_t>(MetaStructFlags::IsPacked));
                }
            }
            const auto structFlags         = atom.getCustomUint32();
            bool       isRecursivelyPacked = true;
            if (not(structFlags & static_cast<uint32_t>(MetaStructFlags::IsPacked)))
            {
                isRecursivelyPacked = false;
            }
            for (int idx = 0; idx < atom.numSubAtoms; ++idx)
            {
                const MetaProperties& member = result.properties.values[startIdx + 1 + idx];
                if (not member.isPrimitiveType())
                {
                    if (not markPackedStructs(result, member.getLinkIndex()))
                    {
                        isRecursivelyPacked = false;
                    }
                }
            }
            if (isRecursivelyPacked)
            {
                atom.setCustomUint32(structFlags | static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked));
            }
            return isRecursivelyPacked;
        }
        int             newIndex = startIdx + 1;
        MetaProperties& itemAtom = result.properties.values[startIdx + 1];
        if (itemAtom.getLinkIndex() > 0)
            newIndex = itemAtom.getLinkIndex();
        // We want to visit the inner type anyway
        const bool innerResult = markPackedStructs(result, newIndex);
        if (atom.type == MetaType::TypeArray)
        {
            return innerResult; // C-arrays are packed if their inner type is packed
        }
        else
        {
            return false; // Vector & co will break packed state
        }
    }
};
} // namespace Reflection
} // namespace SC

#define SC_META_STRUCT_BEGIN(StructName)                                                                               \
    template <>                                                                                                        \
    struct SC::Reflection::MetaClass<StructName> : SC::Reflection::MetaStruct<MetaClass<StructName>>                   \
    {                                                                                                                  \
        static constexpr auto Hash = SC::StringHash(#StructName);                                                      \
                                                                                                                       \
        template <typename MemberVisitor>                                                                              \
        static constexpr bool visit(MemberVisitor&& builder)                                                           \
        {                                                                                                              \
            SC_DISABLE_OFFSETOF_WARNING

#define SC_META_MEMBER(MEMBER) #MEMBER, &T::MEMBER, SC_OFFSET_OF(T, MEMBER)
#define SC_META_STRUCT_MEMBER(ORDER, MEMBER)                                                                           \
    if (not builder(ORDER, #MEMBER, &T::MEMBER, SC_OFFSET_OF(T, MEMBER)))                                              \
    {                                                                                                                  \
        return false;                                                                                                  \
    }

#define SC_META_STRUCT_END()                                                                                           \
    SC_ENABLE_OFFSETOF_WARNING                                                                                         \
    return true;                                                                                                       \
    }                                                                                                                  \
    }                                                                                                                  \
    ;
