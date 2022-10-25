#pragma once
#include "Array.h"
#include "ReflectionFlatSchemaCompiler.h"
#include "Test.h"
#include "Vector.h"

template <typename T, SC::size_t N>
struct SC::Reflection::AtomsFor<SC::Array<T, N>>
{
    static constexpr AtomType getAtomType() { return AtomType::TypeSCArray; }
    static constexpr void     build(AtomsBuilder& atoms)
    {
        const uint16_t lowN  = N & 0xffff;
        const uint16_t highN = (N >> 16) & 0xffff;
        atoms.push({AtomProperties(getAtomType(), lowN, highN, sizeof(SC::Array<T, N>), 0), "SC::Array", nullptr});
        atoms.push({AtomProperties(AtomsFor<T>::getAtomType(), 0, 0, sizeof(T), -1), "T", &AtomsFor<T>::build});
    }
};

template <typename T>
struct SC::Reflection::AtomsFor<SC::Vector<T>>
{
    static constexpr AtomType getAtomType() { return AtomType::TypeSCVector; }
    static constexpr void     build(AtomsBuilder& atoms)
    {
        atoms.push(Atom::create<SC::Vector<T>>("SC::Vector"));
        atoms.push({AtomProperties(AtomsFor<T>::getAtomType(), 0, 0, sizeof(T), -1), "T", &AtomsFor<T>::build});
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

namespace SC
{
namespace Reflection
{

template <>
struct AtomsFor<TestNamespace::SimpleStructure> : AtomStruct<AtomsFor<TestNamespace::SimpleStructure>>
{
    static constexpr void members(AtomsBuilder& atoms)
    {
        atoms.member(0, SC_ATOM_MEMBER(T, f1));
        atoms.member(1, SC_ATOM_MEMBER(T, f2));
    }
};

template <>
struct AtomsFor<TestNamespace::IntermediateStructure> : AtomStruct<AtomsFor<TestNamespace::IntermediateStructure>>
{
    static constexpr void members(AtomsBuilder& atoms)
    {
        atoms.member(0, SC_ATOM_MEMBER(T, simpleStructure));
        atoms.member(1, SC_ATOM_MEMBER(T, vectorOfInt));
    }
};

template <>
struct AtomsFor<TestNamespace::ComplexStructure> : AtomStruct<AtomsFor<TestNamespace::ComplexStructure>>
{
    static constexpr void members(AtomsBuilder& atoms)
    {
        atoms.member(0, SC_ATOM_MEMBER(T, f1));
        atoms.member(1, SC_ATOM_MEMBER(T, simpleStructure));
        atoms.member(2, SC_ATOM_MEMBER(T, simpleStructure2));
        atoms.member(3, SC_ATOM_MEMBER(T, f4));
        atoms.member(4, SC_ATOM_MEMBER(T, intermediateStructure));
        atoms.member(5, SC_ATOM_MEMBER(T, vectorOfStructs));
    }
};
} // namespace Reflection
} // namespace SC

namespace SC
{
struct ReflectionTest;
}

struct SC::ReflectionTest : public SC::TestCase
{
    [[nodiscard]] static constexpr bool constexprEquals(const StringView str1, const StringView other)
    {
        if (str1.sizeInBytesWithoutTerminator() != other.sizeInBytesWithoutTerminator())
            return false;
        for (size_t i = 0; i < str1.sizeInBytesWithoutTerminator(); ++i)
            if (str1.bytesWithoutTerminator()[i] != other.bytesWithoutTerminator()[i])
                return false;
        return true;
    }
    ReflectionTest(SC::TestReport& report) : TestCase(report, "ReflectionTest")
    {
        using namespace SC;
        using namespace SC::Reflection;
        if (test_section("Print Complex structure"))
        {
            constexpr auto       className    = GetTypeNameAsString<TestNamespace::ComplexStructure>::get();
            constexpr StringView classNameStr = "TestNamespace::ComplexStructure";
            static_assert(constexprEquals(StringView(className.data, className.length, false), classNameStr),
                          "Please update SC::ClNm for your compiler");
            // auto numlinks =
            // countUniqueLinks<10>(AtomsFor<TestNamespace::ComplexStructure>::template
            // getAtoms<10>());
            // SC_RELEASE_ASSERT(numlinks == 3);
            constexpr auto ComplexStructureFlatSchema =
                FlatSchemaCompiler<>::compile<TestNamespace::ComplexStructure>();
            printFlatSchema(ComplexStructureFlatSchema.atoms.values, ComplexStructureFlatSchema.names.values);
            // constexpr auto SimpleStructureFlatSchema =
            // FlatSchemaCompiler<>::compile<TestNamespace::SimpleStructure>();
            // printFlatSchema(SimpleStructureFlatSchema.atoms.values,
            // SimpleStructureFlatSchema.names.values);
        }
    }
    template <int NUM_ATOMS>
    void printFlatSchema(const Reflection::AtomProperties (&atom)[NUM_ATOMS],
                         const Reflection::AtomString (&names)[NUM_ATOMS])
    {
        int atomIndex = 0;
        while (atomIndex < NUM_ATOMS)
        {
            atomIndex += printAtoms(atomIndex, atom + atomIndex, names + atomIndex, 0) + 1;
        }
    }

    int printAtoms(int currentAtomIdx, const Reflection::AtomProperties* atom, const Reflection::AtomString* atomName,
                   int indentation)
    {
        using namespace SC;
        using namespace SC::Reflection;
        SC_RELEASE_ASSERT(atom->type == Reflection::AtomType::TypeStruct ||
                          atom->type >= Reflection::AtomType::TypeSCVector);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("[LinkIndex=%d] %.*s (%d atoms)\n", currentAtomIdx, atomName->length, atomName->data,
                          atom->numSubAtoms);
        for (int i = 0; i < indentation; ++i)
            Console::c_printf("\t");
        Console::c_printf("{\n");
        for (int idx = 0; idx < atom->numSubAtoms; ++idx)
        {
            auto& field     = atom[idx + 1];
            auto  fieldName = atomName[idx + 1];
            for (int i = 0; i < indentation + 1; ++i)
                Console::c_printf("\t");
            Console::c_printf("Type=%d\tOffset=%d\tSize=%d\tName=%.*s", (int)field.type, field.offset, field.size,
                              fieldName.length, fieldName.data);
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
