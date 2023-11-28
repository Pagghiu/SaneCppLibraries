// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Containers/Array.h"
#include "../../Containers/Vector.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Testing.h"
#include "ReflectionTestPrint.h"

// TODO: Split the Auto reflection tests in ReflectionAuto
#if SC_META_ENABLE_AUTO_REFLECTION
#if SC_LANGUAGE_CPP_LESS_THAN_20
#include "../../../LibrariesExtra/ReflectionAuto/ReflectionAutoAggregates.h"
#else
#include "../../../LibrariesExtra/ReflectionAuto/ReflectionAutoStructured.h"
#endif
#endif
#include "../ReflectionSC.h"
#include "../ReflectionSchemaCompiler.h"

namespace TestNamespace
{
struct SimpleStructure;
struct IntermediateStructure;
struct ComplexStructure;
} // namespace TestNamespace
struct TestNamespace::SimpleStructure
{
    // Base Types
    SC::uint8_t  f0 = 0;
    SC::uint16_t f1 = 1;
    SC::uint32_t f2 = 2;
    SC::uint64_t f3 = 3;
    SC::int8_t   f4 = 4;
    SC::int16_t  f5 = 5;
    SC::int32_t  f6 = 6;
    SC::int64_t  f7 = 7;
    float        f8 = 8;
    double       f9 = 9;

    int arrayOfInt[3] = {1, 2, 3};
};

namespace SC
{
namespace Reflection
{
template <>
struct Reflect<TestNamespace::SimpleStructure> : ReflectStruct<TestNamespace::SimpleStructure>
{
    template <typename MemberVisitor>
    static constexpr bool visit(MemberVisitor&& visitor)
    {
        return visitor(0, "f0", &T::f0, SC_COMPILER_OFFSETOF(T, f0)) and //
               visitor(1, "f1", &T::f1, SC_COMPILER_OFFSETOF(T, f1)) and //
               visitor(2, "f2", &T::f2, SC_COMPILER_OFFSETOF(T, f2)) and //
               visitor(3, "f3", &T::f3, SC_COMPILER_OFFSETOF(T, f3)) and //
               visitor(4, "f4", &T::f4, SC_COMPILER_OFFSETOF(T, f4)) and //
               visitor(5, "f5", &T::f5, SC_COMPILER_OFFSETOF(T, f5)) and //
               visitor(6, "f6", &T::f6, SC_COMPILER_OFFSETOF(T, f6)) and //
               visitor(7, "f7", &T::f7, SC_COMPILER_OFFSETOF(T, f7)) and //
               visitor(8, "f8", &T::f8, SC_COMPILER_OFFSETOF(T, f8)) and //
               visitor(9, "f9", &T::f9, SC_COMPILER_OFFSETOF(T, f9)) and //
               visitor(10, "arrayOfInt", &T::arrayOfInt, SC_COMPILER_OFFSETOF(T, arrayOfInt));
    }
};
} // namespace Reflection
} // namespace SC

struct TestNamespace::IntermediateStructure
{
    SC::Vector<int> vectorOfInt;
    SimpleStructure simpleStructure;
};

SC_REFLECT_STRUCT_VISIT(TestNamespace::IntermediateStructure)
SC_REFLECT_STRUCT_FIELD(1, vectorOfInt)
SC_REFLECT_STRUCT_FIELD(0, simpleStructure)
SC_REFLECT_STRUCT_LEAVE()

struct TestNamespace::ComplexStructure
{
    SC::uint8_t                 f1 = 0;
    SimpleStructure             simpleStructure;
    SimpleStructure             simpleStructure2;
    SC::uint16_t                f4 = 0;
    IntermediateStructure       intermediateStructure;
    SC::Vector<SimpleStructure> vectorOfStructs;
};

SC_REFLECT_STRUCT_VISIT(TestNamespace::ComplexStructure)
SC_REFLECT_STRUCT_FIELD(0, f1)
SC_REFLECT_STRUCT_FIELD(1, simpleStructure)
SC_REFLECT_STRUCT_FIELD(2, simpleStructure2)
SC_REFLECT_STRUCT_FIELD(3, f4)
SC_REFLECT_STRUCT_FIELD(4, intermediateStructure)
SC_REFLECT_STRUCT_FIELD(5, vectorOfStructs)
SC_REFLECT_STRUCT_LEAVE()

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
// Fails on GCC and MSVC in C++ 14 mode
#if !SC_META_ENABLE_AUTO_REFLECTION || !SC_LANGUAGE_CPP_AT_LEAST_20
SC_REFLECT_STRUCT_VISIT(TestNamespace::PackedStructWithArray)
SC_REFLECT_STRUCT_FIELD(0, arrayValue)
SC_REFLECT_STRUCT_FIELD(1, floatValue)
SC_REFLECT_STRUCT_FIELD(2, int64Value)
SC_REFLECT_STRUCT_LEAVE()
#endif

struct TestNamespace::PackedStruct
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

#if !SC_META_ENABLE_AUTO_REFLECTION
SC_REFLECT_STRUCT_VISIT(TestNamespace::PackedStruct)
SC_REFLECT_STRUCT_FIELD(0, x)
SC_REFLECT_STRUCT_FIELD(1, y)
SC_REFLECT_STRUCT_FIELD(2, z)
SC_REFLECT_STRUCT_LEAVE()
#endif

struct TestNamespace::UnpackedStruct
{
    SC::int16_t x = 10;
    float       y = 2;
    float       z = 3;
};

#if !SC_META_ENABLE_AUTO_REFLECTION
SC_REFLECT_STRUCT_VISIT(TestNamespace::UnpackedStruct)
SC_REFLECT_STRUCT_FIELD(0, x)
SC_REFLECT_STRUCT_FIELD(1, y)
SC_REFLECT_STRUCT_FIELD(2, z)
SC_REFLECT_STRUCT_LEAVE()
#endif

struct TestNamespace::NestedUnpackedStruct
{
    UnpackedStruct unpackedMember;
};
#if !SC_META_ENABLE_AUTO_REFLECTION
SC_REFLECT_STRUCT_VISIT(TestNamespace::NestedUnpackedStruct)
SC_REFLECT_STRUCT_FIELD(0, unpackedMember);
SC_REFLECT_STRUCT_LEAVE()
#endif

struct TestNamespace::StructWithArrayPacked
{
    PackedStruct packedMember[3];
};
// Fails on Clang and GCC in C++ 14 mode
#if !SC_META_ENABLE_AUTO_REFLECTION || !SC_LANGUAGE_CPP_AT_LEAST_20
SC_REFLECT_STRUCT_VISIT(TestNamespace::StructWithArrayPacked)
SC_REFLECT_STRUCT_FIELD(0, packedMember);
SC_REFLECT_STRUCT_LEAVE()
#endif

struct TestNamespace::StructWithArrayUnpacked
{
    NestedUnpackedStruct unpackedMember[3];
};
#if !SC_META_ENABLE_AUTO_REFLECTION
SC_REFLECT_STRUCT_VISIT(TestNamespace::StructWithArrayUnpacked)
SC_REFLECT_STRUCT_FIELD(0, unpackedMember);
SC_REFLECT_STRUCT_LEAVE()
#endif

struct SC::ReflectionTest : public SC::TestCase
{
    ReflectionTest(SC::TestReport& report) : TestCase(report, "ReflectionTest")
    {
        using namespace SC;
        using namespace SC::Reflection;

        if (test_section("Print Simple structure"))
        {
            constexpr auto SimpleStructureFlatSchema = Schema::compile<TestNamespace::SimpleStructure>();
            printFlatSchema(report.console, SimpleStructureFlatSchema.typeInfos.values,
                            SimpleStructureFlatSchema.typeNames.values);
        }
        if (test_section("Print Complex structure"))
        {
            constexpr auto ComplexStructureFlatSchema = Schema::compile<TestNamespace::ComplexStructure>();
            printFlatSchema(report.console, ComplexStructureFlatSchema.typeInfos.values,
                            ComplexStructureFlatSchema.typeNames.values);
        }

        constexpr auto packedStructWithArray      = Schema::compile<TestNamespace::PackedStructWithArray>();
        constexpr auto packedStructWithArrayFlags = packedStructWithArray.typeInfos.values[0].structInfo;
        static_assert(packedStructWithArrayFlags.isPacked, "nestedPacked struct should be recursively packed");

        constexpr auto packedStruct      = Schema::compile<TestNamespace::PackedStruct>();
        constexpr auto packedStructFlags = packedStruct.typeInfos.values[0].structInfo;
        static_assert(packedStructFlags.isPacked, "nestedPacked struct should be recursively packed");

        constexpr auto unpackedStruct      = Schema::compile<TestNamespace::UnpackedStruct>();
        constexpr auto unpackedStructFlags = unpackedStruct.typeInfos.values[0].structInfo;
        static_assert(not(unpackedStructFlags.isPacked), "Unpacked struct should be recursively packed");

        constexpr auto nestedUnpackedStruct      = Schema::compile<TestNamespace::NestedUnpackedStruct>();
        constexpr auto nestedUnpackedStructFlags = nestedUnpackedStruct.typeInfos.values[0].structInfo;
        static_assert(not(nestedUnpackedStructFlags.isPacked), "nestedPacked struct should not be recursively packed");

        constexpr auto structWithArrayPacked      = Schema::compile<TestNamespace::StructWithArrayPacked>();
        constexpr auto structWithArrayPackedFlags = structWithArrayPacked.typeInfos.values[0].structInfo;
        static_assert(structWithArrayPackedFlags.isPacked,
                      "structWithArrayPacked struct should not be recursively packed");

        constexpr auto structWithArrayUnpacked      = Schema::compile<TestNamespace::StructWithArrayUnpacked>();
        constexpr auto structWithArrayUnpackedFlags = structWithArrayUnpacked.typeInfos.values[0].structInfo;
        static_assert(not(structWithArrayUnpackedFlags.isPacked),
                      "structWithArrayUnpacked struct should not be recursively packed");

        constexpr auto       className        = TypeToString<TestNamespace::ComplexStructure>::get();
        constexpr StringView classNameExected = "TestNamespace::ComplexStructure";
        constexpr StringView classNameView(className.data, className.length, false, StringEncoding::Ascii);
        static_assert(classNameView == classNameExected, "Please update SC::ClNm for your compiler");
        constexpr auto intName     = TypeToString<int>::get();
        constexpr auto intNameView = StringView(intName.data, intName.length, false, StringEncoding::Ascii);
        static_assert(intNameView == "int", "Please update SC::ClNm for your compiler");

        static_assert(not ExtendedTypeInfo<String>::IsPacked, "String should not be packed");
    }
};

namespace SC
{
void runReflectionTest(SC::TestReport& report) { ReflectionTest test(report); }
} // namespace SC
