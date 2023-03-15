// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "TaggedUnion.h"
#include "Test.h"

namespace SC
{
struct TaggedUnionTest;

enum TestType
{
    TypeString = 10,
    TypeInt    = 110,
};

union TestUnion
{
    String t0;
    int    t1;

    TestUnion() {}
    ~TestUnion() {}

    using FieldsTypes = TypeList<TaggedField<TestUnion, TestType, decltype(t0), &TestUnion::t0, TypeString>,
                                 TaggedField<TestUnion, TestType, decltype(t1), &TestUnion::t1, TypeInt>>;
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
            auto*                  str1 = defaultConstructed.unionAs<String>();
            SC_TEST_EXPECT(str1 and str1->isEmpty());
            SC_TRUST_RESULT(str1->assign("ASD"));
            auto* ptr2 = defaultConstructed.unionAs<int>();
            SC_TEST_EXPECT(not ptr2);

            // Copy construct
            TaggedUnion<TestUnion> copyConstructed = defaultConstructed;
            auto*                  str2            = copyConstructed.unionAs<String>();
            SC_TEST_EXPECT(str1->data.data() != str2->data.data());
            SC_TEST_EXPECT(*str1 == *str2);

            // Move construct
            TaggedUnion<TestUnion> moveConstructed = move(copyConstructed);
            auto*                  str3            = moveConstructed.unionAs<String>();
            SC_TEST_EXPECT(str1->data.data() != str3->data.data());
            SC_TEST_EXPECT(*str1 == *str3);

            // Copy Assign
            TaggedUnion<TestUnion> copyAssigned;
            copyAssigned.assignValue(1);
            SC_TEST_EXPECT(*copyAssigned.unionAs<int>() == 1);

            // AssignValue - Copy
            String strMove("MOVE");
            copyAssigned.assignValue(strMove);
            SC_TEST_EXPECT(strMove == "MOVE"_a8); // should not be moved
            SC_TEST_EXPECT(copyAssigned.unionAs<String>()->view() == "MOVE"_a8);

            // AssignValue - Move
            copyAssigned.assignValue(2);
            SC_TEST_EXPECT(*copyAssigned.unionAs<int>() == 2);
            copyAssigned.assignValue(move(strMove));
            SC_TEST_EXPECT(strMove.isEmpty()); // should be moved
            SC_TEST_EXPECT(copyAssigned.unionAs<String>()->view() == "MOVE"_a8);

            // AssignValue - Const Copy
            const String str("ASD");
            copyAssigned.assignValue(str);
            SC_TEST_EXPECT(str == "ASD"_a8); // should not be moved
            SC_TEST_EXPECT(copyAssigned.unionAs<String>()->view() == "ASD"_a8);

            // Move Assign
            TaggedUnion<TestUnion> moveAssigned;
            moveAssigned.assignValue(2);
            moveAssigned = move(copyAssigned);
            SC_TEST_EXPECT(copyAssigned.unionAs<String>()->isEmpty());
            SC_TEST_EXPECT(moveAssigned.unionAs<String>()->view() == "ASD"_a8);

            // Access fields directly (a little dangerous but if you know what you're doing..)
            switch (moveAssigned.type)
            {
            case TestType::TypeString: moveAssigned.fields.t0 = "yo"_a8; break;
            case TestType::TypeInt: moveAssigned.fields.t1 = 1; break;
            }
            SC_TEST_EXPECT(moveAssigned.unionAs<String>()->view() == "yo"_a8);
            const TaggedUnion<TestUnion> constAssigned = moveAssigned;

            SC_TEST_EXPECT(constAssigned.unionAs<String>()->view() == "yo"_a8);
        }
    }
};
