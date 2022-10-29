#pragma once
#include "Array.h"
#include "ReflectionFlatSchemaCompiler.h"
#include "ReflectionSC.h"
#include "Test.h"
#include "Vector.h"

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

    int arrayOfInt[3] = {1, 2, 3};
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
struct MetaClass<TestNamespace::SimpleStructure> : MetaStruct<MetaClass<TestNamespace::SimpleStructure>>
{
    static constexpr void members(MetaClassBuilder& builder)
    {
        builder.member(0, SC_META_MEMBER(f1));
        builder.member(1, SC_META_MEMBER(f2));
        builder.member(2, SC_META_MEMBER(arrayOfInt));
    }
};

template <>
struct MetaClass<TestNamespace::IntermediateStructure> : MetaStruct<MetaClass<TestNamespace::IntermediateStructure>>
{
    static constexpr void members(MetaClassBuilder& builder)
    {
        builder.member(0, SC_META_MEMBER(simpleStructure));
        builder.member(1, SC_META_MEMBER(vectorOfInt));
    }
};

template <>
struct MetaClass<TestNamespace::ComplexStructure> : MetaStruct<MetaClass<TestNamespace::ComplexStructure>>
{
    static constexpr void members(MetaClassBuilder& builder)
    {
        builder.member(0, SC_META_MEMBER(f1));
        builder.member(1, SC_META_MEMBER(simpleStructure));
        builder.member(2, SC_META_MEMBER(simpleStructure2));
        builder.member(3, SC_META_MEMBER(f4));
        builder.member(4, SC_META_MEMBER(intermediateStructure));
        builder.member(5, SC_META_MEMBER(vectorOfStructs));
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
        // clang++ -Xclang -ast-dump -Xclang -ast-dump-filter=SimpleStructure -std=c++14
        // libraries/foundation/ReflectionTest.h clang -cc1 -xc++ -fsyntax-only -code-completion-at
        // libraries/Foundation/ReflectionTest.h:94:12 libraries/Foundation/ReflectionTest.h -std=c++14 echo '#include
        // "libraries/Foundation/ReflectionTest.h"\nTestNamespace::SimpleStructure\n::\n"' | clang -cc1 -xc++
        // -fsyntax-only -code-completion-at -:3:3 - -std=c++14
        using namespace SC;
        using namespace SC::Reflection;
        if (test_section("Print Complex structure"))
        {
            constexpr auto       className    = MetaTypeToString<TestNamespace::ComplexStructure>::get();
            constexpr StringView classNameStr = "TestNamespace::ComplexStructure";
            static_assert(constexprEquals(StringView(className.data, className.length, false), classNameStr),
                          "Please update SC::ClNm for your compiler");
            // auto numlinks =
            // countUniqueLinks<10>(MetaClass<TestNamespace::ComplexStructure>::template
            // getAtoms<10>());
            // SC_RELEASE_ASSERT(numlinks == 3);
            constexpr auto ComplexStructureFlatSchema =
                FlatSchemaCompiler<>::compile<TestNamespace::ComplexStructure>();
            printFlatSchema(ComplexStructureFlatSchema.properties.values, ComplexStructureFlatSchema.names.values);
            // constexpr auto SimpleStructureFlatSchema =
            // FlatSchemaCompiler<>::compile<TestNamespace::SimpleStructure>();
            // printFlatSchema(SimpleStructureFlatSchema.atoms.values,
            // SimpleStructureFlatSchema.names.values);
        }
    }
    template <int NUM_ATOMS>
    static void printFlatSchema(const Reflection::MetaProperties (&atom)[NUM_ATOMS],
                                const Reflection::MetaStringView (&names)[NUM_ATOMS])
    {
        int atomIndex = 0;
        while (atomIndex < NUM_ATOMS)
        {
            atomIndex += printAtoms(atomIndex, atom + atomIndex, names + atomIndex, 0) + 1;
        }
    }

    static int printAtoms(int currentAtomIdx, const Reflection::MetaProperties* atom,
                          const Reflection::MetaStringView* atomName, int indentation)
    {
        using namespace SC;
        using namespace SC::Reflection;
        SC_RELEASE_ASSERT(atom->type == Reflection::MetaType::TypeStruct ||
                          atom->type == Reflection::MetaType::TypeArray ||
                          atom->type >= Reflection::MetaType::TypeSCVector);
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
