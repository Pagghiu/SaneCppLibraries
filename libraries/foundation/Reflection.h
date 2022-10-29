#pragma once
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
#if SC_MSVC
    const char*    name            = __FUNCSIG__;
    constexpr char separating_char = '<';
    constexpr int  skip_chars      = 8;
    constexpr int  trim_chars      = 7;
#else
    const char*                     name            = __PRETTY_FUNCTION__;
    constexpr char                  separating_char = '=';
    constexpr int                   skip_chars      = 2;
    constexpr int                   trim_chars      = 1;
#endif
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
    constexpr void setCustomUint32(uint32_t N)
    {
        const uint16_t lowN  = N & 0xffff;
        const uint16_t highN = (N >> 16) & 0xffff;
        order                = static_cast<uint8_t>(lowN);
        offset               = static_cast<uint8_t>(highN);
    }
    constexpr uint32_t getCustomUint32() const { return (offset << 16) | order; }
    constexpr void     setLinkIndex(int8_t linkIndex) { numSubAtoms = linkIndex; }
    constexpr int8_t   getLinkIndex() const { return numSubAtoms; }
};

struct MetaClassBuilder;
// clang-format off
struct MetaPrimitive { static constexpr void build( MetaClassBuilder& builder) { } };

template <typename T> struct MetaClass : public MetaPrimitive {static constexpr MetaType getMetaType(){return MetaType::TypeInvalid;}};

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

template <typename T, int N>
struct MetaArray
{
    T   values[N] = {};
    int size      = 0;
};

struct MetaStringView
{
    const char* data;
    int         length;
    constexpr MetaStringView() : data(nullptr), length(0) {}
    template <int N>
    constexpr MetaStringView(const char (&data)[N]) : data(data), length(N)
    {}
    constexpr MetaStringView(const char* data, int length) : data(data), length(length) {}
};

template <typename T>
struct MetaTypeToString
{
#if SC_CPP_AT_LEAST_17
  private:
    // In C++ 17 we trim the long string producted by ClassName<T> to reduce executable size
    static constexpr auto TrimClassName()
    {
        constexpr auto                    className = ClNm<T>();
        MetaArray<char, className.length> trimmedName;
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
    static constexpr MetaStringView get() { return MetaStringView(value.values, value.size); }
#else
    static constexpr MetaStringView get()
    {
        auto className = ClNm<T>();
        return MetaStringView(className.data, className.length);
    }
#endif
};

struct Atom;

struct MetaClassBuilder
{
    int       size;
    Atom*     output;
    const int capacity;

    constexpr MetaClassBuilder(Atom* output, const int capacity) : size(0), output(output), capacity(capacity) {}

    inline constexpr void push(const Atom& value);
    template <typename T, int N>
    inline constexpr void Struct(const char (&name)[N]);
    template <typename T>
    inline constexpr void Struct();
    template <typename R, typename T, int N>
    inline constexpr void member(int order, const char (&name)[N], R T::*, size_t offset);
};

struct Atom
{
    typedef void (*MetaClassBuildFunc)(MetaClassBuilder& builder);

    MetaProperties     properties;
    MetaStringView     name;
    MetaClassBuildFunc build;

    constexpr Atom() : build(nullptr) {}
    constexpr Atom(const MetaProperties properties, MetaStringView name, MetaClassBuildFunc build)
        : properties(properties), name(name), build(build)
    {}
    template <int MAX_ATOMS>
    constexpr MetaArray<Atom, MAX_ATOMS> getAtoms() const;

    template <typename R, typename T, int N>
    static constexpr Atom create(int order, const char (&name)[N], R T::*, size_t offset)
    {
        return {MetaProperties(MetaClass<R>::getMetaType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1),
                MetaStringView(name, N), &MetaClass<R>::build};
    }

    template <typename T>
    static constexpr Atom create(MetaStringView name = MetaTypeToString<T>::get())
    {
        return {MetaProperties(MetaClass<T>::getMetaType(), 0, 0, sizeof(T), -1), name, &MetaClass<T>::build};
    }

    template <typename T, int N>
    static constexpr Atom create(const char (&name)[N])
    {
        return create<T>(MetaStringView(name, N));
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
    typedef Type              T;
    static constexpr MetaType getMetaType() { return MetaType::TypeStruct; }
    static constexpr void     build(MetaClassBuilder& builder)
    {
        builder.Struct<T>();
        MetaClass<Type>::members(builder);
    }
};

inline constexpr void MetaClassBuilder::push(const Atom& value)
{
    if (size < capacity)
    {
        if (output != nullptr)
        {
            output[size] = value;
        }
        size++;
    }
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

template <int MAX_ATOMS>
inline constexpr auto MetaBuild(Atom::MetaClassBuildFunc build)
{
    MetaArray<Atom, MAX_ATOMS> atoms;
    MetaClassBuilder           container(atoms.values, MAX_ATOMS);
    build(container);
    if (container.size <= MAX_ATOMS)
    {
        atoms.values[0].properties.numSubAtoms = container.size - 1;
        atoms.size                             = container.size;
    }
    return atoms;
}

template <int MAX_ATOMS>
constexpr MetaArray<Atom, MAX_ATOMS> Atom::getAtoms() const
{
    return MetaBuild<MAX_ATOMS>(build);
}

template <typename T, int MAX_ATOMS>
constexpr MetaArray<Atom, MAX_ATOMS> MetaClassGetAtoms()
{
    return MetaBuild<MAX_ATOMS>(&MetaClass<T>::build);
}
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
        static constexpr void members(MetaClassBuilder& builder)                                                       \
        {                                                                                                              \
            SC_DISABLE_OFFSETOF_WARNING

#define SC_META_MEMBER(MEMBER)               #MEMBER, &T::MEMBER, SC_OFFSET_OF(T, MEMBER)
#define SC_META_STRUCT_MEMBER(ORDER, MEMBER) builder.member(ORDER, #MEMBER, &T::MEMBER, SC_OFFSET_OF(T, MEMBER));

#define SC_META_STRUCT_END()                                                                                           \
    SC_ENABLE_OFFSETOF_WARNING                                                                                         \
    }                                                                                                                  \
    }                                                                                                                  \
    ;
