#pragma once
#include "Array.h"
#include "ReflectionFlatSchemaCompiler.h"
#include "Test.h"
#include "Vector.h"

template <typename T, SC::size_t N>
struct SC::Reflection::AtomsFor<SC::Array<T, N>>
{
    static constexpr AtomType getAtomType() { return AtomType::TypeSCArray; }
    static constexpr void     pushAtomsTo(AtomContainer& atoms)
    {
        const uint16_t lowN  = N & 0xffff;
        const uint16_t highN = (N >> 16) & 0xffff;
        atoms.push({AtomProperties(getAtomType(), lowN, highN, sizeof(SC::Array<T, N>), 0), "SC::Array", nullptr});
        atoms.push({AtomProperties(AtomsFor<T>::getAtomType(), 0, 0, sizeof(T), -1), "T", &AtomsFor<T>::pushAtomsTo});
    }
};

template <typename T>
struct SC::Reflection::AtomsFor<SC::Vector<T>>
{
    static constexpr AtomType getAtomType() { return AtomType::TypeSCVector; }
    static constexpr void     pushAtomsTo(AtomContainer& atoms)
    {
        atoms.push(Atom::create<SC::Vector<T>>("SC::Vector"));
        atoms.push({AtomProperties(AtomsFor<T>::getAtomType(), 0, 0, sizeof(T), -1), "T", &AtomsFor<T>::pushAtomsTo});
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
    SC::Vector<SimpleStructure> vectorOfStructs;
};
} // namespace TestNamespace

#if 0
template <>
struct SC::Reflection::AtomsFor<TestNamespace::SimpleStructure> : public SC::Reflection::AtomStruct
{
    static constexpr void pushAtomsTo(AtomContainer& atoms)
    {
        typedef TestNamespace::SimpleStructure T;
        atoms.push(Atom::create<T>("TestNamespace::SimpleStructure"));
        atoms.push(Atom::create(0, "f1", &T::f1, SC_OFFSET_OF(T, f1)));
        atoms.push(Atom::create(1, "f2", &T::f2, SC_OFFSET_OF(T, f2)));
    }
};
template <>
struct SC::Reflection::AtomsFor<TestNamespace::IntermediateStructure> : public SC::Reflection::AtomStruct
{
    static constexpr void pushAtomsTo(AtomContainer& atoms)
    {
        typedef TestNamespace::IntermediateStructure T;
        atoms.push(Atom::create<T>("TestNamespace::IntermediateStructure"));
        atoms.push(Atom::create(0, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure)));
        atoms.push(Atom::create(1, "vectorOfInt", &T::vectorOfInt, SC_OFFSET_OF(T, vectorOfInt)));
    }
};

template <>
struct SC::Reflection::AtomsFor<TestNamespace::ComplexStructure> : public SC::Reflection::AtomStruct
{
    static constexpr void pushAtomsTo(AtomContainer& atoms)
    {
        typedef TestNamespace::ComplexStructure T;
        atoms.push(Atom::create<T>("TestNamespace::ComplexStructure"));
        atoms.push(Atom::create(0, "f1", &T::f1, SC_OFFSET_OF(T, f1)));
        atoms.push(Atom::create(1, "simpleStructure", &T::simpleStructure, SC_OFFSET_OF(T, simpleStructure)));
        atoms.push(Atom::create(2, "simpleStructure2", &T::simpleStructure2, SC_OFFSET_OF(T, simpleStructure2)));
        atoms.push(Atom::create(3, "f4", &T::f1, SC_OFFSET_OF(T, f4)));
        atoms.push(Atom::create(4, "intermediate", &T::intermediateStructure, SC_OFFSET_OF(T, intermediateStructure)));
        atoms.push(Atom::create(5, "vectorOfStructs", &T::vectorOfStructs, SC_OFFSET_OF(T, vectorOfStructs)));
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
SC_REFLECT_FIELD(5, vectorOfStructs)
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
            //            constexpr auto SimpleStructureFlatSchema =
            //            FlatSchemaCompiler<>::compile<TestNamespace::SimpleStructure>();
            //            printFlatSchema(SimpleStructureFlatSchema.atoms.values,
            //            SimpleStructureFlatSchema.names.values);
        }
    }
    template <int NUM_ATOMS>
    void printFlatSchema(const Reflection::AtomProperties (&atom)[NUM_ATOMS], const char* const (&names)[NUM_ATOMS])
    {
        int atomIndex = 0;
        while (atomIndex < NUM_ATOMS)
        {
            atomIndex += printAtoms(atomIndex, atom + atomIndex, names + atomIndex, 0) + 1;
        }
    }

    int printAtoms(int currentAtomIdx, const Reflection::AtomProperties* atom, const char* const* atomName,
                   int indentation)
    {
        using namespace SC;
        using namespace SC::Reflection;
        SC_RELEASE_ASSERT(atom->type == Reflection::AtomType::TypeStruct ||
                          atom->type >= Reflection::AtomType::TypeSCVector);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("[LinkIndex=%d] %s (%d atoms)\n", currentAtomIdx, *atomName, atom->numSubAtoms);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("{\n");
        for (int idx = 0; idx < atom->numSubAtoms; ++idx)
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
        return atom->numSubAtoms;
    }
};
