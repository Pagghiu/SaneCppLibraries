// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../TaggedUnion.h"
#include "../../Libraries/Strings/String.h"
#include "../../Libraries/Testing/Testing.h"

//! [TaggedUnionTestSnippet]
namespace SC
{
struct TaggedUnionTest;

// Create an arbitrary enumeration with some values
enum TestType
{
    TypeString = 10,
    TypeInt    = 110,
};

// Create the union definition containing a FieldTypes nested type
struct TestUnion
{
    // Helper to save some typing
    template <TestType E, typename T>
    using Tag = TaggedType<TestType, E, T>;

    // FieldsTypes MUST be defined to be a TypeList of TaggedType(s)
    using FieldsTypes = TypeTraits::TypeList< // List all TargetType associations
        Tag<TypeString, String>,              // Associate TypeString with String
        Tag<TypeInt, int>>;                   // Associate TypeInt with init
};

void taggedUnionUsageSnippet(Console& console)
{
    // Create the tagged union on the TestUnion definition
    TaggedUnion<TestUnion> test; // default initialized to first type (String)

    // Access / Change type
    String* ptr = test.field<TypeString>();
    if (ptr) // If TypeString is not active type, ptr will be == nullptr
    {
        *ptr = "SomeValue";
    }
    test.changeTo<TypeInt>() = 2; // Change active type to TypeInt (compile time known)

    // Switch on currently active type (TypeInt)
    switch (test.getType())
    {
    case TypeString: console.print("String = {}", *test.field<TypeString>()); break;
    case TypeInt: console.print("Int = {}", *test.field<TypeInt>()); break;
    }

    // Set current active type at runtime back to TypeString
    test.setType(TypeString);
    *test.field<TypeString>() = "Some new string";
}
} // namespace SC
//! [TaggedUnionTestSnippet]

struct SC::TaggedUnionTest : public SC::TestCase
{
    TaggedUnionTest(SC::TestReport& report) : TestCase(report, "TaggedUnionTest")
    {
        using namespace SC;
        if (test_section("Rule of 5"))
        {
            TaggedUnion<TestUnion> defaultConstructed;

            auto* str1 = defaultConstructed.field<TypeString>();
            SC_TEST_EXPECT(str1 and str1->isEmpty());
            SC_TRUST_RESULT(str1->assign("ASD"));
            auto* ptr2 = defaultConstructed.field<TypeInt>();
            SC_TEST_EXPECT(not ptr2);

            // Copy construct
            TaggedUnion<TestUnion> copyConstructed = defaultConstructed;

            auto* str2 = copyConstructed.field<TypeString>();
            SC_TEST_EXPECT(*str1 == *str2);

            // Move construct
            TaggedUnion<TestUnion> moveConstructed = move(copyConstructed);

            auto* str3 = moveConstructed.field<TypeString>();
            SC_TEST_EXPECT(*str1 == *str3);

            // Copy Assign
            TaggedUnion<TestUnion> copyAssigned;
            copyAssigned.assign<TypeInt>(1);
            SC_TEST_EXPECT(*copyAssigned.field<TypeInt>() == 1);

            // assign - Copy
            String strMove("MOVE");
            copyAssigned.assign<TypeString>(strMove);
            SC_TEST_EXPECT(strMove == "MOVE"); // should not be moved
            SC_TEST_EXPECT(copyAssigned.field<TypeString>()->view() == "MOVE");

            // assign - Move
            copyAssigned.assign<TypeInt>(2);
            SC_TEST_EXPECT(*copyAssigned.field<TypeInt>() == 2);
            copyAssigned.assign<TypeString>(move(strMove));
            SC_TEST_EXPECT(strMove.isEmpty()); // should be moved
            SC_TEST_EXPECT(copyAssigned.field<TypeString>()->view() == "MOVE");

            // assign - Const Copy
            const String str("ASD");
            copyAssigned.assign<TypeString>(str);
            SC_TEST_EXPECT(str == "ASD"); // should not be moved
            SC_TEST_EXPECT(copyAssigned.field<TypeString>()->view() == "ASD");

            // Move Assign
            TaggedUnion<TestUnion> moveAssigned;
            moveAssigned.assign<TypeInt>(2);
            moveAssigned = move(copyAssigned);
#ifndef __clang_analyzer__
            SC_TEST_EXPECT(copyAssigned.field<TypeString>()->isEmpty());
#endif // not __clang_analyzer__
            SC_TEST_EXPECT(moveAssigned.field<TypeString>()->view() == "ASD");

            switch (moveAssigned.getType())
            {
            case TestType::TypeString: *moveAssigned.field<TestType::TypeString>() = "yo"; break;
            case TestType::TypeInt: *moveAssigned.field<TestType::TypeInt>() = 1; break;
            }
            SC_TEST_EXPECT(moveAssigned.field<TypeString>()->view() == "yo");
            const TaggedUnion<TestUnion> constAssigned = moveAssigned;

            SC_TEST_EXPECT(constAssigned.field<TypeString>()->view() == "yo");

            // Change type at runtime
            moveAssigned.setType(TypeInt);
            SC_TEST_EXPECT(*moveAssigned.field<TypeInt>() == 0);
        }
    }
};

namespace SC
{
void runTaggedUnionTest(SC::TestReport& report) { TaggedUnionTest test(report); }
} // namespace SC
