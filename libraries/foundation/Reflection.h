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
    const char*                 name            = __PRETTY_FUNCTION__;
    constexpr char              separating_char = '=';
    constexpr int               skip_chars      = 2;
    constexpr int               trim_chars      = 1;
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

enum class AtomType : uint8_t
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
    TypeSCMap    = 15,
};

struct AtomProperties
{
    AtomType type;        // 1
    uint8_t  order;       // 1
    uint16_t offset;      // 2
    uint16_t size;        // 2
    int16_t  numSubAtoms; // 2

    constexpr AtomProperties() : type(AtomType::TypeInvalid), order(0), offset(0), size(0), numSubAtoms(0)
    {
        static_assert(sizeof(AtomProperties) == 8, "Size must be 8 bytes");
    }
    constexpr AtomProperties(AtomType type, uint8_t order, uint16_t offset, uint16_t size, int16_t numSubAtoms)
        : type(type), order(order), offset(offset), size(size), numSubAtoms(numSubAtoms)
    {}
    constexpr void    setLinkIndex(int16_t linkIndex) { numSubAtoms = linkIndex; }
    constexpr int16_t getLinkIndex() const { return numSubAtoms; }
};

struct AtomContainer;
// clang-format off
struct AtomPrimitive { static constexpr void pushAtomsTo( AtomContainer& atoms) { } };

template <typename T> struct AtomsFor : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeInvalid;}};

template <> struct AtomsFor<uint8_t>  : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeUINT8;}};
template <> struct AtomsFor<uint16_t> : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeUINT16;}};
template <> struct AtomsFor<uint32_t> : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeUINT32;}};
template <> struct AtomsFor<uint64_t> : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeUINT64;}};
template <> struct AtomsFor<int8_t>   : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeINT8;}};
template <> struct AtomsFor<int16_t>  : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeINT16;}};
template <> struct AtomsFor<int32_t>  : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeINT32;}};
template <> struct AtomsFor<int64_t>  : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeINT64;}};
template <> struct AtomsFor<float>    : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeFLOAT32;}};
template <> struct AtomsFor<double>   : public AtomPrimitive {static constexpr AtomType getAtomType(){return AtomType::TypeDOUBLE64;}};
// clang-format on

template <typename T, int N>
struct AtomsArray
{
    T   values[N] = {};
    int size      = 0;

    bool exceedsCapacity = true;

    constexpr void push_back(T value)
    {
        if (size + 1 < N)
        {
            values[size++] = value;
        }
        else
        {
            exceedsCapacity = true;
        }
    }
};

struct AtomString
{
    const char* data;
    int         length;
    constexpr AtomString() : data(nullptr), length(0) {}
    template <int N>
    constexpr AtomString(const char (&data)[N]) : data(data), length(N)
    {}
    constexpr AtomString(const char* data, int length) : data(data), length(length) {}
    //    constexpr bool operator==(const AtomString& other) const
    //    {
    //        if (length != other.length)
    //            return false;
    //        for (int i = 0; i < length; ++i)
    //        {
    //            if (data[i] != other.data[i])
    //                return false;
    //        }
    //        return true;
    //    }
};

template <typename T>
struct GetTypeNameAsString
{
#if SC_CPP_AT_LEAST_17
  private:
    // In C++ 17 we try to trim the long string producted by ClassName<T> to reduce executable size
    static constexpr auto TrimClassName()
    {
        constexpr auto                     className = ClNm<T>();
        AtomsArray<char, className.length> trimmedName;
        for (int i = 0; i < className.length; ++i)
        {
            trimmedName.values[i] = className.data[i];
        }
        trimmedName.size = className.length;
        return trimmedName;
    }

    static inline constexpr auto value = TrimClassName();

  public:
    static constexpr AtomString get() { return AtomString(value.values, value.size); }
#else
    static constexpr AtomString get()
    {
        auto className = ClNm<T>();
        return AtomString(className.data, className.length);
    }
#endif
};

struct Atom;

struct AtomContainer
{
    int       size;
    Atom*     output;
    const int capacity;

    constexpr AtomContainer(Atom* output, const int capacity) : size(0), output(output), capacity(capacity) {}

    inline constexpr bool push(const Atom& value);
    template <typename T, int N>
    inline constexpr bool Struct(const char (&name)[N]);
    template <typename T>
    inline constexpr bool Struct();
    template <typename R, typename T, int N>
    inline constexpr bool member(int order, const char (&name)[N], R T::*, size_t offset);
};

struct Atom
{
    typedef void (*AtomsPushFunc)(AtomContainer& atoms);

    AtomProperties properties;
    AtomString     nameString;
    AtomsPushFunc  pushAtomsTo;

    constexpr Atom() : pushAtomsTo(nullptr) {}
    constexpr Atom(const AtomProperties properties, AtomString nameString, AtomsPushFunc pushAtomsTo)
        : properties(properties), nameString(nameString), pushAtomsTo(pushAtomsTo)
    {}
    template <int MAX_ATOMS>
    constexpr AtomsArray<Atom, MAX_ATOMS> getAtoms() const;

    template <typename R, typename T, int N>
    static constexpr Atom create(int order, const char (&name)[N], R T::*, size_t offset)
    {
        return {AtomProperties(AtomsFor<R>::getAtomType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1),
                AtomString(name, N), &AtomsFor<R>::pushAtomsTo};
    }

    template <typename T>
    static constexpr Atom create(AtomString name = GetTypeNameAsString<T>::get())
    {
        return {AtomProperties(AtomsFor<T>::getAtomType(), 0, 0, sizeof(T), -1), name, &AtomsFor<T>::pushAtomsTo};
    }

    template <typename T, int N>
    static constexpr Atom create(const char (&name)[N])
    {
        return create<T>(AtomString(name, N));
    }
};

template <typename Type>
struct AtomStruct;

template <typename Type>
struct AtomStruct<AtomsFor<Type>>
{
    typedef Type              T;
    static constexpr AtomType getAtomType() { return AtomType::TypeStruct; }
    static constexpr void     pushAtomsTo(AtomContainer& atoms)
    {
        atoms.Struct<T>();
        AtomsFor<Type>::members(atoms);
    }
};

inline constexpr bool AtomContainer::push(const Atom& value)
{
    if (size < capacity)
    {
        if (output != nullptr)
        {
            output[size] = value;
        }
        size++;
        return true;
    }
    else
    {
        return false;
    }
}
template <typename T, int N>
inline constexpr bool AtomContainer::Struct(const char (&name)[N])
{
    return push(Atom::create<T>(name));
}
template <typename T>
inline constexpr bool AtomContainer::Struct()
{
    return push(Atom::create<T>());
}
template <typename R, typename T, int N>
inline constexpr bool AtomContainer::member(int order, const char (&name)[N], R T::*field, size_t offset)
{
    return push(Atom::create(order, name, field, offset));
}

template <int MAX_ATOMS>
inline constexpr auto GetAtomsFromFunc(Atom::AtomsPushFunc pushAtomsTo)
{
    AtomsArray<Atom, MAX_ATOMS> atoms;
    AtomContainer              container(atoms.values, MAX_ATOMS);
    pushAtomsTo(container);
    if (container.size <= MAX_ATOMS)
    {
        atoms.values[0].properties.numSubAtoms = container.size - 1;
        atoms.size                             = container.size;
    }
    return atoms;
}

template <int MAX_ATOMS>
constexpr AtomsArray<Atom, MAX_ATOMS> Atom::getAtoms() const
{
    return GetAtomsFromFunc<MAX_ATOMS>(pushAtomsTo);
}

template <typename T, int MAX_ATOMS>
constexpr AtomsArray<Atom, MAX_ATOMS> GetAtomsFor()
{
    return GetAtomsFromFunc<MAX_ATOMS>(&AtomsFor<T>::pushAtomsTo);
}
} // namespace Reflection
} // namespace SC

#define SC_ATOM_MEMBER(TYPE, MEMBER) #MEMBER, &TYPE::MEMBER, SC_OFFSET_OF(TYPE, MEMBER)
