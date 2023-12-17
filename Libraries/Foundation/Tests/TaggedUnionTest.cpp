// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../TaggedUnion.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct TaggedUnionTest;

enum TestType
{
    TypeString = 10,
    TypeInt    = 110,
};

struct TestUnion
{
    template <TestType E, typename T>
    using AssociateTag = TaggedType<TestType, E, T>; // Helper to save some typing
    using FieldsTypes  = TypeTraits::TypeList<       // List of types for the union
        AssociateTag<TypeString, String>,           // Associate String to TypeString
        AssociateTag<TypeInt, int>>;                // Associate int to TypeInt
};
} // namespace SC

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
        }
    }
};

namespace SC
{
void runTaggedUnionTest(SC::TestReport& report) { TaggedUnionTest test(report); }
} // namespace SC
