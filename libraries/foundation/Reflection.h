#pragma once
#include "Types.h"

namespace SC
{
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
struct AtomStruct {     static constexpr AtomType getAtomType() { return AtomType::TypeStruct; } };

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
struct Atom;

struct AtomContainer
{
    int       size;
    Atom*     output;
    const int capacity;

    constexpr AtomContainer(Atom* output, const int capacity) : size(0), output(output), capacity(capacity) {}

    inline constexpr bool push(const Atom& value);
};

struct Atom
{
    typedef void (*AtomsPushFunc)(AtomContainer& atoms);

    AtomProperties properties;
    const char*    name;
    AtomsPushFunc  pushAtomsTo;

    constexpr Atom() : name(nullptr), pushAtomsTo(nullptr) {}
    constexpr Atom(const AtomProperties properties, const char* name, AtomsPushFunc pushAtomsTo)
        : properties(properties), name(name), pushAtomsTo(pushAtomsTo)
    {}
    template <int MAX_ATOMS>
    constexpr AtomsArray<Atom, MAX_ATOMS> getAtoms() const;

    template <typename R, typename T>
    static constexpr Atom create(int order, const char* name, R T::*, size_t offset)
    {
        return {AtomProperties(AtomsFor<R>::getAtomType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1),
                name, &AtomsFor<R>::pushAtomsTo};
    }
    template <typename T>
    static constexpr Atom create(const char* name)
    {
        return {AtomProperties(AtomsFor<T>::getAtomType(), 0, 0, sizeof(T), -1), name, &AtomsFor<T>::pushAtomsTo};
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

template <int MAX_ATOMS>
inline constexpr auto GetAtomsFromFunc(Atom::AtomsPushFunc pushAtomsTo)
{
    AtomsArray<Atom, MAX_ATOMS> atoms;
    AtomContainer               container(atoms.values, MAX_ATOMS);
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

#define SC_REFLECT_STRUCT_START(StructName)                                                                            \
    template <>                                                                                                        \
    struct SC::Reflection::AtomsFor<StructName> : public AtomStruct                                                    \
    {                                                                                                                  \
        static constexpr void pushAtomsTo(AtomContainer& atoms)                                                        \
        {                                                                                                              \
            typedef StructName T;                                                                                      \
            atoms.push(Atom::create<T>(#StructName));

#define SC_REFLECT_FIELD(Order, Field) atoms.push(Atom::create(Order, #Field, &T::Field, SC_OFFSET_OF(T, Field)));

#define SC_REFLECT_STRUCT_END()                                                                                        \
    }                                                                                                                  \
    }                                                                                                                  \
    ;
