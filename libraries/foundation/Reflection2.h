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

constexpr bool IsPrimitiveType(MetaType type) { return type >= MetaType::TypeUINT8 && type <= MetaType::TypeDOUBLE64; }
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
struct MetaAtom
{
    MetaProperties      properties;
    ConstexprStringView name;
    MetaClassBuildFunc  build;
};
struct MetaClassBuilder
{
    MetaAtom* data     = nullptr;
    int       numAtoms = 0;
};
template <typename Type>
struct MetaStruct;

template <typename Type>
struct MetaStruct<MetaClass<Type>>
{
    typedef Type              T;
    static constexpr MetaType getMetaType() { return MetaType::TypeStruct; }
};
} // namespace Reflection2
} // namespace SC

#define SC_META2_STRUCT_BEGIN(StructName)                                                                              \
    template <>                                                                                                        \
    struct SC::Reflection2::MetaClass<StructName> : SC::Reflection2::MetaStruct<MetaClass<StructName>>                 \
    {                                                                                                                  \
        static constexpr auto Hash = SC::StringHash(#StructName);                                                      \
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
