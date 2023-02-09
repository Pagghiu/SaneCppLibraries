// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Array.h"
#include "../Foundation/StringBuilder.h"
#include "../Foundation/Test.h"
#include "../Foundation/Vector.h"
#include "ReflectionFlatSchemaCompiler.h"
#include "ReflectionSC.h"

namespace SC
{
namespace Reflection
{
struct TestClassBuilder : public MetaClassBuilder<TestClassBuilder>
{
    typedef AtomBase<TestClassBuilder> Atom;

    struct EmptyPayload
    {
    };
    EmptyPayload payload;
    constexpr TestClassBuilder(Atom* output = nullptr, const int capacity = 0) : MetaClassBuilder(output, capacity) {}
};

using FlatSchemaTest = Reflection::FlatSchemaCompiler<TestClassBuilder>;
} // namespace Reflection
} // namespace SC

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
    template <typename MemberVisitor>
    static constexpr bool visit(MemberVisitor&& visitor)
    {
        return                                               //
            visitor(0, "f1", &T::f1, SC_OFFSETOF(T, f1)) and //
            visitor(1, "f2", &T::f2, SC_OFFSETOF(T, f2)) and //
            visitor(2, "arrayOfInt", &T::arrayOfInt, SC_OFFSETOF(T, arrayOfInt));
    }
};

template <>
struct MetaClass<TestNamespace::IntermediateStructure> : MetaStruct<MetaClass<TestNamespace::IntermediateStructure>>
{
    template <typename MemberVisitor>
    static constexpr bool visit(MemberVisitor&& visitor)
    {
        return //
            visitor(1, "vectorOfInt", &T::vectorOfInt, SC_OFFSETOF(T, vectorOfInt)) and
            visitor(0, "simpleStructure", &T::simpleStructure, SC_OFFSETOF(T, simpleStructure));
    }
};

template <>
struct MetaClass<TestNamespace::ComplexStructure> : MetaStruct<MetaClass<TestNamespace::ComplexStructure>>
{
    template <typename MemberVisitor>
    static constexpr bool visit(MemberVisitor&& visitor)
    {
        return                                                                                                        //
            visitor(0, "f1", &T::f1, SC_OFFSETOF(T, f1)) and                                                          //
            visitor(1, "simpleStructure", &T::simpleStructure, SC_OFFSETOF(T, simpleStructure)) and                   //
            visitor(2, "simpleStructure2", &T::simpleStructure2, SC_OFFSETOF(T, simpleStructure2)) and                //
            visitor(3, "f4", &T::f4, SC_OFFSETOF(T, f4)) and                                                          //
            visitor(4, "intermediateStructure", &T::intermediateStructure, SC_OFFSETOF(T, intermediateStructure)) and //
            visitor(5, "vectorOfStructs", &T::vectorOfStructs, SC_OFFSETOF(T, vectorOfStructs));
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

SC_META_STRUCT_VISIT(TestNamespace::PackedStructWithArray)
SC_META_STRUCT_FIELD(0, arrayValue)
SC_META_STRUCT_FIELD(1, floatValue)
SC_META_STRUCT_FIELD(2, int64Value)
SC_META_STRUCT_LEAVE()

struct TestNamespace::PackedStruct
{
    float x, y, z = 0.0f;
};

SC_META_STRUCT_VISIT(TestNamespace::PackedStruct)
SC_META_STRUCT_FIELD(0, x)
SC_META_STRUCT_FIELD(1, y)
SC_META_STRUCT_FIELD(2, z)
SC_META_STRUCT_LEAVE()

struct TestNamespace::UnpackedStruct
{
    SC::int16_t x = 10;
    float       y = 2;
    float       z = 3;
};

SC_META_STRUCT_VISIT(TestNamespace::UnpackedStruct)
SC_META_STRUCT_FIELD(0, x)
SC_META_STRUCT_FIELD(1, y)
SC_META_STRUCT_FIELD(2, z)
SC_META_STRUCT_LEAVE()

struct TestNamespace::NestedUnpackedStruct
{
    UnpackedStruct unpackedMember;
};
SC_META_STRUCT_VISIT(TestNamespace::NestedUnpackedStruct)
SC_META_STRUCT_FIELD(0, unpackedMember);
SC_META_STRUCT_LEAVE()

struct TestNamespace::StructWithArrayPacked
{
    PackedStruct packedMember[3];
};
SC_META_STRUCT_VISIT(TestNamespace::StructWithArrayPacked)
SC_META_STRUCT_FIELD(0, packedMember);
SC_META_STRUCT_LEAVE()

struct TestNamespace::StructWithArrayUnpacked
{
    NestedUnpackedStruct unpackedMember[3];
};
SC_META_STRUCT_VISIT(TestNamespace::StructWithArrayUnpacked)
SC_META_STRUCT_FIELD(0, unpackedMember);
SC_META_STRUCT_LEAVE()

namespace SC
{
// TODO: Move printFlatSchema somewhere else
template <int NUM_ATOMS, typename MetaProperties>
inline void printFlatSchema(Console& console, const MetaProperties (&atom)[NUM_ATOMS],
                            const SC::ConstexprStringView (&names)[NUM_ATOMS])
{
    int atomIndex = 0;
    while (atomIndex < NUM_ATOMS)
    {
        atomIndex += printAtoms(console, atomIndex, atom + atomIndex, names + atomIndex, 0) + 1;
    }
}

template <typename MetaProperties>
inline int printAtoms(Console& console, int currentAtomIdx, const MetaProperties* atom,
                      const SC::ConstexprStringView* atomName, int indentation)
{
    String        buffer(StringEncoding::Ascii);
    StringBuilder builder(buffer);
    (void)builder.append("[{:02}]", currentAtomIdx);
    for (int i = 0; i < indentation; ++i)
        (void)builder.append("\t");
    (void)builder.append("[LinkIndex={:2}] {} ({} atoms)\n", currentAtomIdx,
                         StringView(atomName->data, atomName->length, false, StringEncoding::Ascii), atom->numSubAtoms);
    for (int i = 0; i < indentation; ++i)
        (void)builder.append("\t");
    (void)builder.append("{\n");
    for (int idx = 0; idx < atom->numSubAtoms; ++idx)
    {
        auto& field     = atom[idx + 1];
        auto  fieldName = atomName[idx + 1];
        (void)builder.append("[{:02}]", currentAtomIdx + idx + 1);

        for (int i = 0; i < indentation + 1; ++i)
            (void)builder.append("\t");

        (void)builder.append("Type={}\tOffset={}\tSize={}\tName={}", (int)field.type, field.offsetInBytes,
                             field.sizeInBytes,
                             StringView(fieldName.data, fieldName.length, false, StringEncoding::Ascii));
        if (field.getLinkIndex() >= 0)
        {
            (void)builder.append("\t[LinkIndex={}]", field.getLinkIndex());
        }
        (void)builder.append("\n");
    }
    for (int i = 0; i < indentation; ++i)
        (void)builder.append("\t");

    (void)builder.append("}\n");
    console.print(builder.view());
    return atom->numSubAtoms;
}
} // namespace SC

struct SC::ReflectionTest : public SC::TestCase
{

    [[nodiscard]] static constexpr bool constexprEquals(const StringView str1, const StringView other)
    {
        if (str1.sizeInBytes() != other.sizeInBytes())
            return false;
        for (size_t i = 0; i < str1.sizeInBytes(); ++i)
            if (str1.bytesWithoutTerminator()[i] != other.bytesWithoutTerminator()[i])
                return false;
        return true;
    }
    ReflectionTest(SC::TestReport& report) : TestCase(report, "ReflectionTest")
    {
        // clang++ -Xclang -ast-dump -Xclang -ast-dump-filter=SimpleStructure -std=c++14
        // Libraries/Reflection/ReflectionTest.h clang -cc1 -xc++ -fsyntax-only -code-completion-at
        // libraries/Reflection/ReflectionTest.h:94:12 libraries/Reflection/ReflectionTest.h -std=c++14 echo '#include
        // "libraries/Reflection/ReflectionTest.h"\nTestNamespace::SimpleStructure\n::\n"' | clang -cc1 -xc++
        // -fsyntax-only -code-completion-at -:3:3 - -std=c++14
        using namespace SC;
        using namespace SC::Reflection;
        if (test_section("Packing"))
        {
            constexpr auto packedStructWithArray      = FlatSchemaTest::compile<TestNamespace::PackedStructWithArray>();
            constexpr auto packedStructWithArrayFlags = packedStructWithArray.properties.values[0].getCustomUint32();
            static_assert(packedStructWithArrayFlags & MetaStructFlags::IsPacked,
                          "nestedPacked struct should be recursively packed");

            constexpr auto packedStruct      = FlatSchemaTest::compile<TestNamespace::PackedStruct>();
            constexpr auto packedStructFlags = packedStruct.properties.values[0].getCustomUint32();
            static_assert(packedStructFlags & MetaStructFlags::IsPacked,
                          "nestedPacked struct should be recursively packed");

            constexpr auto unpackedStruct      = FlatSchemaTest::compile<TestNamespace::UnpackedStruct>();
            constexpr auto unpackedStructFlags = unpackedStruct.properties.values[0].getCustomUint32();
            static_assert(not(unpackedStructFlags & MetaStructFlags::IsPacked),
                          "Unpacked struct should be recursively packed");

            constexpr auto nestedUnpackedStruct      = FlatSchemaTest::compile<TestNamespace::NestedUnpackedStruct>();
            constexpr auto nestedUnpackedStructFlags = nestedUnpackedStruct.properties.values[0].getCustomUint32();
            static_assert(not(nestedUnpackedStructFlags & MetaStructFlags::IsPacked),
                          "nestedPacked struct should not be recursively packed");

            constexpr auto structWithArrayPacked      = FlatSchemaTest::compile<TestNamespace::StructWithArrayPacked>();
            constexpr auto structWithArrayPackedFlags = structWithArrayPacked.properties.values[0].getCustomUint32();
            static_assert(structWithArrayPackedFlags & MetaStructFlags::IsPacked,
                          "structWithArrayPacked struct should not be recursively packed");

            constexpr auto structWithArrayUnpacked = FlatSchemaTest::compile<TestNamespace::StructWithArrayUnpacked>();
            constexpr auto structWithArrayUnpackedFlags =
                structWithArrayUnpacked.properties.values[0].getCustomUint32();
            static_assert(not(structWithArrayUnpackedFlags & MetaStructFlags::IsPacked),
                          "structWithArrayUnpacked struct should not be recursively packed");
        }
        if (test_section("Print Simple structure"))
        {
            constexpr auto SimpleStructureFlatSchema = FlatSchemaTest::compile<TestNamespace::SimpleStructure>();
            printFlatSchema(report.console, SimpleStructureFlatSchema.properties.values,
                            SimpleStructureFlatSchema.names.values);
        }
        if (test_section("Print Complex structure"))
        {
            constexpr auto       className    = TypeToString<TestNamespace::ComplexStructure>::get();
            constexpr StringView classNameStr = "TestNamespace::ComplexStructure";
            static_assert(constexprEquals(StringView(className.data, className.length, false, StringEncoding::Ascii),
                                          classNameStr),
                          "Please update SC::ClNm for your compiler");
            constexpr auto       intName    = TypeToString<int>::get();
            constexpr StringView intNameStr = "int";
            static_assert(
                constexprEquals(StringView(intName.data, intName.length, false, StringEncoding::Ascii), intNameStr),
                "Please update SC::ClNm for your compiler");
            constexpr auto ComplexStructureFlatSchema = FlatSchemaTest::compile<TestNamespace::ComplexStructure>();
            printFlatSchema(report.console, ComplexStructureFlatSchema.properties.values,
                            ComplexStructureFlatSchema.names.values);
        }
    }
};
