#pragma once
#include "ConstexprTypes.h"
#include "FlatSchemaCompiler.h"
#include "Language.h" // IsTriviallyCopyable<T>
#include "Types.h"

namespace SC
{

// These are short names because they end up in symbol table (as we're storing their stringized signature)
struct Nm
{
    const char* data;
    int         length;
    constexpr Nm(const char* data, int length) : data(data), length(length) {}
};

template <typename T>
static constexpr Nm ClNm()
{
    // clang-format off
#if SC_MSVC
    const char*    name            = __FUNCSIG__;
    constexpr char separating_char = '<';
    constexpr int  skip_chars      = 8;
    constexpr int  trim_chars      = 7;
#else
    const char*    name            = __PRETTY_FUNCTION__;
    constexpr char separating_char = '=';
    constexpr int  skip_chars      = 2;
    constexpr int  trim_chars      = 1;
#endif
    // clang-format on
    int         length = 0;
    const char* it     = name;
    while (*it != separating_char)
        it++;
    it += skip_chars;
    while (it[length] != 0)
        length++;
    return Nm(it, length - trim_chars);
}

namespace Reflection
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

    // SC containers types
    TypeSCArray  = 13,
    TypeSCVector = 14,
};
enum class MetaStructFlags : uint32_t
{
    IsTriviallyCopyable = 1 << 0, // Type can be memcpy-ied
    IsPacked            = 1 << 1, // There is no spacing between fields of a type
    IsRecursivelyPacked = 1 << 2, // There is no spacing even between sub-types of all members
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
        if (type == MetaType::TypeStruct)
        {
            if (getCustomUint32() & static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked))
            {
                return true;
            }
        }
        return false;
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

template <typename T>
struct MetaTypeToString
{
#if SC_CPP_AT_LEAST_17
  private:
    // In C++ 17 we trim the long string producted by ClassName<T> to reduce executable size
    [[nodiscard]] static constexpr auto TrimClassName()
    {
        constexpr auto                         className = ClNm<T>();
        ConstexprArray<char, className.length> trimmedName;
        for (int i = 0; i < className.length; ++i)
        {
            trimmedName.values[i] = className.data[i];
        }
        trimmedName.size = className.length;
        return trimmedName;
    }

    // Inline static constexpr requires C++17
    static inline constexpr auto value = TrimClassName();

  public:
    [[nodiscard]] static constexpr ConstexprStringView get() { return ConstexprStringView(value.values, value.size); }
#else
    [[nodiscard]] static constexpr ConstexprStringView get()
    {
        auto className = ClNm<T>();
        return ConstexprStringView(className.data, className.length);
    }
#endif
};

struct Atom;

struct MetaClassBuilder
{
    int       size;
    int       wantedCapacity;
    Atom*     output;
    const int capacity;

    constexpr MetaClassBuilder(Atom* output, const int capacity)
        : size(0), wantedCapacity(0), output(output), capacity(capacity)
    {}

    inline constexpr void push(const Atom& value);
    template <typename T, int N>
    inline constexpr void Struct(const char (&name)[N]);
    template <typename T>
    inline constexpr void Struct();
    template <typename R, typename T, int N>
    inline constexpr void member(int order, const char (&name)[N], R T::*, size_t offset);
    template <typename R, typename T, int N>
    [[nodiscard]] constexpr bool operator()(int order, const char (&name)[N], R T::*f, size_t offset)
    {
        member(order, name, f, offset);
        return true;
    }
};

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
    template <int MAX_ATOMS>
    [[nodiscard]] constexpr ConstexprArray<Atom, MAX_ATOMS> getAtoms() const;

    [[nodiscard]] constexpr int countAtoms() const
    {
        MetaClassBuilder builder(nullptr, 0);
        if (build != nullptr)
        {
            build(builder);
            return builder.wantedCapacity;
        }
        return 0;
    }

    template <typename R, typename T, int N>
    [[nodiscard]] static constexpr Atom create(int order, const char (&name)[N], R T::*, size_t offset)
    {
        return {MetaProperties(MetaClass<R>::getMetaType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1),
                ConstexprStringView(name, N), &MetaClass<R>::build};
    }

    template <typename T>
    [[nodiscard]] static constexpr Atom create(ConstexprStringView name = MetaTypeToString<T>::get())
    {
        return {MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), name, &MetaClass<T>::build};
    }

    template <typename T, int N>
    [[nodiscard]] static constexpr Atom create(const char (&name)[N])
    {
        return create<T>(ConstexprStringView(name, N));
    }

    [[nodiscard]] constexpr bool operator==(const Atom& other) const { return build == other.build; }

    [[nodiscard]] constexpr bool areAllMembersPacked(int numAtoms) const
    {
        uint32_t nextOffset = 0;
        for (int idx = 0; idx < numAtoms; ++idx)
        {
            const Atom* member = this + idx + 1;
            if (member->properties.offset != nextOffset)
            {
                return false;
            }
            nextOffset += member->properties.size;
        }
        return nextOffset == properties.size;
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
        builder.push(arrayHeader);
        builder.push({MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), MetaTypeToString<T>::get(),
                      &MetaClass<T>::build});
    }
};
template <typename Type>
struct MetaStruct;

template <typename Type>
struct MetaStruct<MetaClass<Type>>
{
    typedef Type                            T;
    [[nodiscard]] static constexpr MetaType getMetaType() { return MetaType::TypeStruct; }

    static constexpr void build(MetaClassBuilder& builder)
    {
        builder.Struct<T>();
        MetaClass<Type>::members(builder);
        if (builder.capacity > 0)
        {
            uint32_t flags = 0;
            if (IsTriviallyCopyable<T>::value)
                flags |= static_cast<uint32_t>(MetaStructFlags::IsTriviallyCopyable);
            if (builder.output->areAllMembersPacked(builder.size - 1))
                flags |= static_cast<uint32_t>(MetaStructFlags::IsPacked);
            builder.output[0].properties.setCustomUint32(flags);
        }
    }
};

inline constexpr void MetaClassBuilder::push(const Atom& value)
{
    if (size < capacity)
    {
        output[size] = value;
        size++;
    }
    wantedCapacity++;
}
template <typename T, int N>
inline constexpr void MetaClassBuilder::Struct(const char (&name)[N])
{
    push(Atom::create<T>(name));
}
template <typename T>
inline constexpr void MetaClassBuilder::Struct()
{
    push(Atom::create<T>());
}
template <typename R, typename T, int N>
inline constexpr void MetaClassBuilder::member(int order, const char (&name)[N], R T::*field, size_t offset)
{
    push(Atom::create(order, name, field, offset));
}

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
        markPackedStructs(result, 0);
        return result;
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

#if SC_CLANG

#define SC_DISABLE_OFFSETOF_WARNING                                                                                    \
    _Pragma("clang diagnostic push");                                                                                  \
    _Pragma("clang diagnostic ignored \"-Winvalid-offsetof\"");
#define SC_ENABLE_OFFSETOF_WARNING _Pragma("clang diagnostic pop");

#elif SC_GCC
#define SC_DISABLE_OFFSETOF_WARNING                                                                                    \
    _Pragma("GCC diagnostic push");                                                                                    \
    _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"");
#define SC_ENABLE_OFFSETOF_WARNING _Pragma("GCC diagnostic pop");

#else

#define SC_DISABLE_OFFSETOF_WARNING
#define SC_ENABLE_OFFSETOF_WARNING

#endif

#define SC_META_STRUCT_BEGIN(StructName)                                                                               \
    template <>                                                                                                        \
    struct SC::Reflection::MetaClass<StructName> : SC::Reflection::MetaStruct<MetaClass<StructName>>                   \
    {                                                                                                                  \
        static constexpr auto Hash = SC::StringHash(#StructName);                                                      \
                                                                                                                       \
        template <typename MemberVisitor>                                                                              \
        static constexpr bool members(MemberVisitor&& builder)                                                         \
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
