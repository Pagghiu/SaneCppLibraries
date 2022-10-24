#pragma once
#include "Types.h"

namespace SC
{
namespace Reflection
{
struct Atom
{
    enum Type : uint8_t
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

    Type     type;        // 1
    uint8_t  order;       // 1
    uint16_t offset;      // 2
    uint16_t size;        // 2
    int16_t  numChildren; // 2

    constexpr Atom() : type(Atom::TypeInvalid), order(0), offset(0), size(0), numChildren(0)
    {
        static_assert(sizeof(Atom) == 8, "Size must be 8 bytes");
    }
    constexpr Atom(Type type, uint8_t order, uint16_t offset, uint16_t size, int16_t numChildren)
        : type(type), order(order), offset(offset), size(size), numChildren(numChildren)
    {}
    constexpr void    setLinkIndex(int16_t linkIndex) { numChildren = linkIndex; }
    constexpr int16_t getLinkIndex() const { return numChildren; }
};

template <typename T, int N>
struct AtomsArray
{
    T   values[N] = {};
    int size      = 0;
};

template <int MAX_ATOMS>
struct AtomWithName
{
    typedef AtomsArray<AtomWithName, MAX_ATOMS> (*GetAtomsFunction)();

    Atom             atom;
    const char*      name;
    GetAtomsFunction getAtoms;

    constexpr AtomWithName() : name(nullptr), getAtoms(nullptr) {}
    constexpr AtomWithName(const Atom atom, const char* name, GetAtomsFunction getAtoms)
        : atom(atom), name(name), getAtoms(getAtoms)
    {}
};

// clang-format off
struct AtomsEmpty { template <int MAX_ATOMS> static constexpr auto getAtoms(){ return AtomsArray<AtomWithName<MAX_ATOMS>, MAX_ATOMS>(); }};
template <typename T> struct AtomsFor : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeInvalid;}};

template <> struct AtomsFor<uint8_t>  : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeUINT8;}};
template <> struct AtomsFor<uint16_t> : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeUINT16;}};
template <> struct AtomsFor<uint32_t> : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeUINT32;}};
template <> struct AtomsFor<uint64_t> : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeUINT64;}};
template <> struct AtomsFor<int8_t>   : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeINT8;}};
template <> struct AtomsFor<int16_t>  : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeINT16;}};
template <> struct AtomsFor<int32_t>  : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeINT32;}};
template <> struct AtomsFor<int64_t>  : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeINT64;}};
template <> struct AtomsFor<float>    : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeFLOAT32;}};
template <> struct AtomsFor<double>   : public AtomsEmpty {static constexpr Atom::Type getAtomType(){return Atom::TypeDOUBLE64;}};
// clang-format on

template <int MAX_ATOMS, typename R, typename T>
constexpr AtomWithName<MAX_ATOMS> AtomWithNameCreate(int order, const char* name, R T::*, size_t offset)
{
    return {Atom(AtomsFor<R>::getAtomType(), order, static_cast<SC::uint16_t>(offset), sizeof(R), -1), name,
            &AtomsFor<R>::template getAtoms<MAX_ATOMS>};
}

} // namespace Reflection
} // namespace SC
#define SC_REFLECT_STRUCT_START(StructName)                                                                            \
    template <>                                                                                                        \
    struct SC::Reflection::AtomsFor<StructName>                                                                        \
    {                                                                                                                  \
        typedef StructName          T;                                                                                 \
        static constexpr Atom::Type getAtomType() { return Atom::TypeStruct; }                                         \
        template <int MAX_ATOMS>                                                                                       \
        static constexpr auto getAtoms()                                                                               \
        {                                                                                                              \
            AtomsArray<AtomWithName<MAX_ATOMS>, MAX_ATOMS> atoms;                                                      \
            atoms.values[atoms.size++] = {Atom(Atom::TypeStruct, 0, 0, sizeof(T), 0), #StructName, nullptr};

#define SC_REFLECT_FIELD(Order, Field)                                                                                 \
    atoms.values[atoms.size++] = AtomWithNameCreate<MAX_ATOMS>(Order, #Field, &T::Field, SC_OFFSET_OF(T, Field));

#define SC_REFLECT_STRUCT_END()                                                                                        \
    atoms.values[0].atom.numChildren = atoms.size - 1;                                                                 \
    return atoms;                                                                                                      \
    }                                                                                                                  \
    }                                                                                                                  \
    ;
