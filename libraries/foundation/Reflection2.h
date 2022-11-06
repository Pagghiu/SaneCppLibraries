#pragma once
#include "ConstexprTypes.h"

namespace SC
{
namespace Reflection2
{
enum class MetaType : uint8_t
{
    // Invalid sentinel
    TypeInvalid = 0,

    // Struct and Array types
    TypeStruct = 1,
    TypeArray  = 2,

    // Primitive types
    TypeUINT8    = 3,
    TypeUINT16   = 4,
    TypeUINT32   = 5,
    TypeUINT64   = 6,
    TypeINT8     = 7,
    TypeINT16    = 8,
    TypeINT32    = 9,
    TypeINT64    = 10,
    TypeFLOAT32  = 11,
    TypeDOUBLE64 = 12,

    TypeCustom = 13,
};

template <typename T>
struct MetaClass;

struct MetaClassBuilder;
typedef bool (*MetaClassBuildFunc)(MetaClassBuilder& builder);
struct MetaProperties
{
    MetaType type;        // 1
    int8_t   numSubAtoms; // 1
    uint16_t order;       // 2
    uint16_t offset;      // 2
    uint16_t size;        // 2
};
struct Atom
{
    typedef void (*MetaClassBuildFunc)(MetaClassBuilder& builder);

    MetaProperties      properties;
    ConstexprStringView name;
    MetaClassBuildFunc  build;
};

struct MetaClassBuilder
{
    Atom* data     = nullptr;
    int   numAtoms = 0;
};

template <typename Type>
struct MetaStruct;

template <typename Type>
struct MetaStruct<MetaClass<Type>>
{
    typedef Type              T;
    static constexpr MetaType getMetaType() { return MetaType::TypeStruct; }
};

struct FlatSchemaCompiler
{
    typedef FlatSchemaCompilerBase::FlatSchemaCompilerBase<MetaProperties, Atom, MetaClassBuilder> FlatSchemaBase;

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_ATOMS: maximum number of atoms (struct members). When using constexpr it will trim it to actual size.
    template <typename T, int MAX_LINK_BUFFER_SIZE = 20, int MAX_TOTAL_ATOMS = 100>
    static constexpr auto compile()
    {
        constexpr ConstexprArray<Atom, MAX_TOTAL_ATOMS> allAtoms =
            FlatSchemaBase::compileAllAtomsFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_ATOMS>(&MetaClass<T>::build);
        static_assert(allAtoms.size > 0, "Something failed in compileAllAtomsFor");
        FlatSchemaBase::FlatSchema<allAtoms.size> result;
        for (int i = 0; i < allAtoms.size; ++i)
        {
            result.properties.values[i] = allAtoms.values[i].properties;
            result.names.values[i]      = allAtoms.values[i].name;
        }
        result.properties.size = allAtoms.size;
        result.names.size      = allAtoms.size;
        return result;
    }
};
} // namespace Reflection2
} // namespace SC

#define SC_META2_STRUCT_BEGIN(StructName)                                                                              \
    template <>                                                                                                        \
    struct SC::Serialization2::HashFor<StructName>                                                                     \
    {                                                                                                                  \
        static constexpr auto Hash = SC::StringHash(#StructName);                                                      \
    };                                                                                                                 \
    template <>                                                                                                        \
    struct SC::Reflection2::MetaClass<StructName> : SC::Reflection2::MetaStruct<MetaClass<StructName>>                 \
    {                                                                                                                  \
                                                                                                                       \
        template <typename MemberVisitor>                                                                              \
        static constexpr bool visit(MemberVisitor&& visitor)                                                           \
        {                                                                                                              \
            SC_DISABLE_OFFSETOF_WARNING

#define SC_META2_MEMBER(MEMBER) #MEMBER, &T::MEMBER, SC_OFFSET_OF(T, MEMBER)
#define SC_META2_STRUCT_MEMBER(ORDER, MEMBER)                                                                          \
    if (not visitor(ORDER, #MEMBER, &T::MEMBER, SC_OFFSET_OF(T, MEMBER)))                                              \
    {                                                                                                                  \
        return false;                                                                                                  \
    }

#define SC_META2_STRUCT_END()                                                                                          \
    SC_ENABLE_OFFSETOF_WARNING                                                                                         \
    return true;                                                                                                       \
    }                                                                                                                  \
    }                                                                                                                  \
    ;
