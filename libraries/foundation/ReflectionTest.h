#pragma once
#include "Array.h"
#include "ReflectionFlatSchemaCompiler.h"
#include "Test.h"
#include "Vector.h"

template <typename T, SC::size_t N>
struct SC::Reflection::AtomsFor<SC::Array<T, N>>
{
    static constexpr Atom::Type getAtomType() { return Atom::TypeSCArray; }
    template <int MAX_ATOMS>
    static constexpr auto getAtoms()
    {
        AtomsArray<AtomWithName<MAX_ATOMS>, MAX_ATOMS> atoms;

        const uint16_t lowN              = N & 0xffff;
        const uint16_t highN             = (N >> 16) & 0xffff;
        atoms.values[atoms.size++]       = {Atom(getAtomType(), lowN, highN, sizeof(SC::Array<T, N>), 0), "SC::Array",
                                            nullptr};
        atoms.values[atoms.size++]       = {Atom(AtomsFor<T>::getAtomType(), 0, 0, sizeof(T), -1), "T",
                                            &AtomsFor<T>::template getAtoms<MAX_ATOMS>};
        atoms.values[0].atom.numChildren = atoms.size - 1;
        return atoms;
    }
};

template <typename T>
struct SC::Reflection::AtomsFor<SC::Vector<T>>
{
    static constexpr Atom::Type getAtomType() { return Atom::TypeSCVector; }
    template <int MAX_ATOMS>
    static constexpr auto getAtoms()
    {
        AtomsArray<AtomWithName<MAX_ATOMS>, MAX_ATOMS> atoms;
        atoms.values[atoms.size++]       = {Atom(getAtomType(), 0, 0, sizeof(SC::Vector<T>), 0), "SC::Vector", nullptr};
        atoms.values[atoms.size++]       = {Atom(AtomsFor<T>::getAtomType(), 0, 0, sizeof(T), -1), "T",
                                            &AtomsFor<T>::template getAtoms<MAX_ATOMS>};
        atoms.values[0].atom.numChildren = atoms.size - 1;
        return atoms;
    }
};

namespace TestNamespace
{
struct SimpleStructure
{
    // Base Types
    SC::uint8_t  f1  = 0;
    SC::uint16_t f2  = 1;
    SC::uint32_t f3  = 2;
    SC::uint64_t f4  = 3;
    SC::int8_t   f5  = 4;
    SC::int16_t  f6  = 5;
    SC::int32_t  f7  = 6;
    SC::int64_t  f8  = 7;
    float        f9  = 8;
    double       f10 = 9;
};
struct IntermediateStructure
{
    SC::Vector<int> vectorOfInt;
    SimpleStructure simpleStructure;
};

struct ComplexStructure
{
    SC::uint8_t                 f1 = 0;
    SimpleStructure             simpleStructure;
    SimpleStructure             simpleStructure2;
    SC::uint16_t                f4 = 0;
    IntermediateStructure       intermediateStructure;
    SC::Vector<SimpleStructure> vectorOfSimpleStruct;
};
} // namespace TestNamespace

#if 0

template <>
struct SC::Reflection::AtomsFor<TestNamespace::SimpleStructure>
{
    static constexpr Atom::Type getAtomType() { return Atom::TypeStruct; }
    template <int MAX_ATOMS>
    static constexpr auto getAtoms()
    {
        typedef TestNamespace::SimpleStructure         T;
        AtomsArray<AtomWithName<MAX_ATOMS>, MAX_ATOMS> atoms;
        atoms.values[atoms.size++]       = {Atom(Atom::TypeStruct, 0, 0, sizeof(T), 2), "SimpleStructure", nullptr};
        atoms.values[atoms.size++]       = AtomWithNameCreate<MAX_ATOMS>(0, "f1", &T::f1, SC_OFFSET_OF(T, f1));
        atoms.values[atoms.size++]       = AtomWithNameCreate<MAX_ATOMS>(1, "f2", &T::f2, SC_OFFSET_OF(T, f2));
        atoms.values[0].atom.numChildren = atoms.size - 1;
        return atoms;
    }
};

template <>
struct SC::Reflection::AtomsFor<TestNamespace::IntermediateStructure>
{
    static constexpr Atom::Type getAtomType() { return Atom::TypeStruct; }
    template <int MAX_ATOMS>
    static constexpr auto getAtoms()
    {
        typedef TestNamespace::IntermediateStructure   T;
        AtomsArray<AtomWithName<MAX_ATOMS>, MAX_ATOMS> atoms;
        atoms.values[atoms.size++] = {Atom(Atom::TypeStruct, 0, 0, sizeof(T), 0), "IntermediateStructure", nullptr};
        atoms.values[atoms.size++] =
            AtomWithNameCreate<MAX_ATOMS>(0, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure));
        atoms.values[atoms.size++] =
            AtomWithNameCreate<MAX_ATOMS>(1, "vectorOfInt", &T::vectorOfInt, SC_OFFSET_OF(T, vectorOfInt));
        atoms.values[0].atom.numChildren = atoms.size - 1;
        return atoms;
    }
};

template <>
struct SC::Reflection::AtomsFor<TestNamespace::ComplexStructure>
{
    static constexpr Atom::Type getAtomType() { return Atom::TypeStruct; }
    template <int MAX_ATOMS>
    static constexpr auto getAtoms()
    {
        typedef TestNamespace::ComplexStructure        T;
        AtomsArray<AtomWithName<MAX_ATOMS>, MAX_ATOMS> atoms;
        atoms.values[atoms.size++] = {Atom(Atom::TypeStruct, 0, 0, sizeof(T), 4), "ComplexStructure", nullptr};
        atoms.values[atoms.size++] = AtomWithNameCreate<MAX_ATOMS>(0, "f1", &T::f1, SC_OFFSET_OF(T, f1));
        atoms.values[atoms.size++] =
            AtomWithNameCreate<MAX_ATOMS>(1, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure));
        atoms.values[atoms.size++] = AtomWithNameCreate<MAX_ATOMS>(2, "simpleStructure2", &T::simpleStructure2,
                                                                   SC_OFFSET_OF(T, simpleStructure2));
        atoms.values[atoms.size++] = AtomWithNameCreate<MAX_ATOMS>(3, "f4", &T::f1, SC_OFFSET_OF(T, f4));
        atoms.values[atoms.size++] = AtomWithNameCreate<MAX_ATOMS>(4, "intermediate", &T::intermediateStructure,
                                                                   SC_OFFSET_OF(T, intermediateStructure));
        atoms.values[atoms.size++] = AtomWithNameCreate<MAX_ATOMS>(5, "vectorOfSimpleStruct", &T::vectorOfSimpleStruct,
                                                                   SC_OFFSET_OF(T, vectorOfSimpleStruct));
        atoms.values[0].atom.numChildren = atoms.size - 1;
        return atoms;
    }
};
#else

SC_REFLECT_STRUCT_START(TestNamespace::SimpleStructure)
SC_REFLECT_FIELD(0, f1)
SC_REFLECT_FIELD(1, f2)
SC_REFLECT_FIELD(2, f3)
SC_REFLECT_FIELD(3, f4)
SC_REFLECT_FIELD(4, f5)
SC_REFLECT_FIELD(5, f6)
SC_REFLECT_FIELD(6, f7)
SC_REFLECT_FIELD(7, f8)
SC_REFLECT_FIELD(8, f9)
SC_REFLECT_FIELD(9, f10)
SC_REFLECT_STRUCT_END()

SC_REFLECT_STRUCT_START(TestNamespace::IntermediateStructure)
SC_REFLECT_FIELD(0, vectorOfInt)
SC_REFLECT_FIELD(1, simpleStructure)
SC_REFLECT_STRUCT_END()

SC_REFLECT_STRUCT_START(TestNamespace::ComplexStructure)
SC_REFLECT_FIELD(0, f1)
SC_REFLECT_FIELD(1, simpleStructure)
SC_REFLECT_FIELD(2, simpleStructure2)
SC_REFLECT_FIELD(3, f4)
SC_REFLECT_FIELD(4, intermediateStructure)
SC_REFLECT_FIELD(5, vectorOfSimpleStruct)
SC_REFLECT_STRUCT_END()

#endif

namespace SC
{
struct ReflectionTest;
}

struct SC::ReflectionTest : public SC::TestCase
{
    ReflectionTest(SC::TestReport& report) : TestCase(report, "ReflectionTest")
    {
        using namespace SC;
        using namespace SC::Reflection;
        if (test_section("Print Complex structure"))
        {
            // auto numlinks =
            // countUniqueLinks<10>(AtomsFor<TestNamespace::ComplexStructure>::template
            // getAtoms<10>());
            // SC_RELEASE_ASSERT(numlinks == 3);
            constexpr auto ComplexStructureFlatSchema =
                FlatSchemaCompiler<>::compile<TestNamespace::ComplexStructure>();
            printFlatSchema(ComplexStructureFlatSchema.atoms.values, ComplexStructureFlatSchema.names.values);
        }
    }
    template <int NUM_ATOMS>
    void printFlatSchema(const Reflection::Atom (&atom)[NUM_ATOMS], const char* const (&names)[NUM_ATOMS])
    {
        int atomIndex = 0;
        while (atomIndex < NUM_ATOMS)
        {
            atomIndex += printAtoms(atomIndex, atom + atomIndex, names + atomIndex, 0) + 1;
        }
    }

    int printAtoms(int currentAtomIdx, const Reflection::Atom* atom, const char* const* atomName, int indentation)
    {
        using namespace SC;
        using namespace SC::Reflection;
        SC_RELEASE_ASSERT(atom->type == Reflection::Atom::TypeStruct || atom->type >= Reflection::Atom::TypeSCVector);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("[LinkIndex=%d] %s (%d atoms)\n", currentAtomIdx, *atomName, atom->numChildren);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("{\n");
        for (int idx = 0; idx < atom->numChildren; ++idx)
        {
            auto&       field     = atom[idx + 1];
            const char* fieldName = atomName[idx + 1];
            for (int i = 0; i < indentation + 1; ++i)
                Console::c_printf("\t");
            Console::c_printf("Type=%d\tOffset=%d\tSize=%d\tName=%s", (int)field.type, field.offset, field.size,
                              fieldName);
            if (field.getLinkIndex() >= 0)
            {
                Console::c_printf("\t[LinkIndex=%d]", field.getLinkIndex());
            }
            Console::c_printf("\n");
        }
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("}\n");
        return atom->numChildren;
    }
};
