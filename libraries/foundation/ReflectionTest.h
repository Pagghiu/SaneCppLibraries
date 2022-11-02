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

namespace TestNamespace
{
struct PackedStructWithArray;
struct PackedStruct;
struct UnpackedStruct;
struct NestedUnpackedStruct;
struct StructWithArrayPacked;
struct StructWithArrayUnpacked;
} // namespace TestNamespace

struct TestNamespace::PackedStructWithArray
{
    SC::uint8_t arrayValue[4] = {0, 1, 2, 3};
    float       floatValue    = 1.5f;
    SC::int64_t int64Value    = -13;
};
SC_META_STRUCT_BEGIN(TestNamespace::PackedStructWithArray)
SC_META_STRUCT_MEMBER(0, arrayValue)
SC_META_STRUCT_MEMBER(1, floatValue)
SC_META_STRUCT_MEMBER(2, int64Value)
SC_META_STRUCT_END()

struct TestNamespace::PackedStruct
{
    float x, y, z;
    PackedStruct() : x(0), y(0), z(0) {}
};

SC_META_STRUCT_BEGIN(TestNamespace::PackedStruct)
SC_META_STRUCT_MEMBER(0, x);
SC_META_STRUCT_MEMBER(1, y);
SC_META_STRUCT_MEMBER(2, z);
SC_META_STRUCT_END()

struct TestNamespace::UnpackedStruct
{
    SC::int16_t x;
    float       y, z;

    UnpackedStruct()
    {
        x = 10;
        y = 2;
        z = 3;
    }
};

SC_META_STRUCT_BEGIN(TestNamespace::UnpackedStruct)
SC_META_STRUCT_MEMBER(0, x);
SC_META_STRUCT_MEMBER(1, y);
SC_META_STRUCT_MEMBER(2, z);
SC_META_STRUCT_END()

struct TestNamespace::NestedUnpackedStruct
{
    UnpackedStruct unpackedMember;
};
SC_META_STRUCT_BEGIN(TestNamespace::NestedUnpackedStruct)
SC_META_STRUCT_MEMBER(0, unpackedMember);
SC_META_STRUCT_END()

struct TestNamespace::StructWithArrayPacked
{
    PackedStruct packedMember[3];
};
SC_META_STRUCT_BEGIN(TestNamespace::StructWithArrayPacked)
SC_META_STRUCT_MEMBER(0, packedMember);
SC_META_STRUCT_END()

struct TestNamespace::StructWithArrayUnpacked
{
    NestedUnpackedStruct unpackedMember[3];
};
SC_META_STRUCT_BEGIN(TestNamespace::StructWithArrayUnpacked)
SC_META_STRUCT_MEMBER(0, unpackedMember);
SC_META_STRUCT_END()

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
        if (test_section("Packing"))
        {

            constexpr auto packedStructWithArray =
                FlatSchemaCompiler<>::compile<TestNamespace::PackedStructWithArray>();
            constexpr auto packedStructWithArrayFlags = packedStructWithArray.properties.values[0].getCustomUint32();
            static_assert(packedStructWithArrayFlags & static_cast<uint32_t>(MetaStructFlags::IsPacked),
                          "Packed struct should be packed");
            //            auto packedStructWithArrayRuntime = packedStructWithArray;
            //            FlatSchemaCompiler<>::markRecursiveProperties(packedStructWithArrayRuntime, 0);
            //            auto packedStructWithArrayRuntimeFlags =
            //            packedStructWithArrayRuntime.properties.values[0].getCustomUint32();
            //            SC_TEST_EXPECT(packedStructWithArrayRuntimeFlags &
            //            static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked));
            static_assert(packedStructWithArrayFlags & static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked),
                          "nestedPacked struct should be recursively packed");
            static_assert(packedStructWithArrayFlags & static_cast<uint32_t>(MetaStructFlags::IsTriviallyCopyable),
                          "Packed struct should be trivially copyable");

            constexpr auto packedStruct      = FlatSchemaCompiler<>::compile<TestNamespace::PackedStruct>();
            constexpr auto packedStructFlags = packedStruct.properties.values[0].getCustomUint32();
            static_assert(packedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsPacked),
                          "Packed struct should be packed");
            static_assert(packedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked),
                          "nestedPacked struct should be recursively packed");
            static_assert(packedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsTriviallyCopyable),
                          "Packed struct should be trivially copyable");

            constexpr auto unpackedStruct      = FlatSchemaCompiler<>::compile<TestNamespace::UnpackedStruct>();
            constexpr auto unpackedStructFlags = unpackedStruct.properties.values[0].getCustomUint32();
            static_assert(not(unpackedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsPacked)),
                          "Unpacked struct should not be packed");
            static_assert(not(unpackedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked)),
                          "Unpacked struct should be recursively packed");
            static_assert(unpackedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsTriviallyCopyable),
                          "Unpacked struct should be trivially copyable");

            constexpr auto nestedUnpackedStruct = FlatSchemaCompiler<>::compile<TestNamespace::NestedUnpackedStruct>();
            constexpr auto nestedUnpackedStructFlags = nestedUnpackedStruct.properties.values[0].getCustomUint32();
            static_assert((nestedUnpackedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsPacked)),
                          "nestedPacked struct should be packed");
            static_assert(not(nestedUnpackedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked)),
                          "nestedPacked struct should not be recursiely packed");
            static_assert(nestedUnpackedStructFlags & static_cast<uint32_t>(MetaStructFlags::IsTriviallyCopyable),
                          "nestedPacked struct should be trivially copyable");

            constexpr auto structWithArrayPacked =
                FlatSchemaCompiler<>::compile<TestNamespace::StructWithArrayPacked>();
            constexpr auto structWithArrayPackedFlags = structWithArrayPacked.properties.values[0].getCustomUint32();
            static_assert((structWithArrayPackedFlags & static_cast<uint32_t>(MetaStructFlags::IsPacked)),
                          "structWithArrayPacked struct should be packed");
            static_assert(structWithArrayPackedFlags & static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked),
                          "structWithArrayPacked struct should not be recursiely packed");
            static_assert(structWithArrayPackedFlags & static_cast<uint32_t>(MetaStructFlags::IsTriviallyCopyable),
                          "structWithArrayPacked struct should be trivially copyable");

            constexpr auto structWithArrayUnpacked =
                FlatSchemaCompiler<>::compile<TestNamespace::StructWithArrayUnpacked>();
            constexpr auto structWithArrayUnpackedFlags =
                structWithArrayUnpacked.properties.values[0].getCustomUint32();
            static_assert((structWithArrayUnpackedFlags & static_cast<uint32_t>(MetaStructFlags::IsPacked)),
                          "structWithArrayUnpacked struct should be packed");
            static_assert(
                not(structWithArrayUnpackedFlags & static_cast<uint32_t>(MetaStructFlags::IsRecursivelyPacked)),
                "structWithArrayUnpacked struct should not be recursiely packed");
            static_assert(structWithArrayUnpackedFlags & static_cast<uint32_t>(MetaStructFlags::IsTriviallyCopyable),
                          "structWithArrayUnpacked struct should be trivially copyable");
        }
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
