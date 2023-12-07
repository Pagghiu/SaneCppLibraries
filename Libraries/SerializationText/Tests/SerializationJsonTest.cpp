// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../SerializationJson.h"

#include "../../Containers/SmallVector.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct Test;
} // namespace SC

struct SC::Test
{
    int    x      = 2;
    float  y      = 1.5f;
    int    xy[2]  = {1, 3};
    String myTest = "asdf"_a8;

    Vector<String> myVector = {"Str1"_a8, "Str2"_a8};

    bool operator==(const Test& other) const
    {
        return x == other.x and y == other.y and                   //
               xy[0] == other.xy[0] and xy[1] == other.xy[1] and   //
               myTest == other.myTest and myVector.size() == 2 and //
               myVector.size() == other.myVector.size() and        //
               myVector[0] == other.myVector[0] and myVector[1] == other.myVector[1];
    }
};
SC_REFLECT_STRUCT_VISIT(SC::Test)
SC_REFLECT_STRUCT_FIELD(0, x)
SC_REFLECT_STRUCT_FIELD(1, y)
SC_REFLECT_STRUCT_FIELD(2, xy)
SC_REFLECT_STRUCT_FIELD(3, myTest)
SC_REFLECT_STRUCT_FIELD(4, myVector)
SC_REFLECT_STRUCT_LEAVE()

namespace SC
{
struct SerializationJsonTest;
}

struct SC::SerializationJsonTest : public SC::TestCase
{
    SerializationJsonTest(SC::TestReport& report) : TestCase(report, "SerializationJsonTest")
    {
        constexpr StringView testJSON = R"({"x":2,"y":1.50,"xy":[1,3],"myTest":"asdf","myVector":["Str1","Str2"]})"_a8;
        if (test_section("SerializationJson::write"))
        {
            Test                   test;
            SmallVector<char, 256> buffer;
            StringFormatOutput     output(StringEncoding::Ascii, buffer);

            SC_TEST_EXPECT(SerializationJson::write(test, output));
            const StringView serializedJSON(buffer.data(), buffer.size() - 1, false, StringEncoding::Ascii);
            SC_TEST_EXPECT(serializedJSON == testJSON);
        }
        if (test_section("SerializationJson::loadExact"))
        {
            Test test;
            memset(&test, 0, sizeof(test));
            SC_TEST_EXPECT(SerializationJson::loadExact(test, testJSON));
            SC_TEST_EXPECT(test == Test());
        }
        if (test_section("SerializationJson::loadVersioned"))
        {
            constexpr StringView scrambledJson =
                R"({"y"  :  1.50, "x": 2.0, "myVector"  :  ["Str1","Str2"], "myTest":"asdf"})"_a8;
            Test test;
            test.x = 0;
            test.y = 0;
            (void)test.myVector.resize(1);
            (void)test.myTest.assign("FDFSA"_a8);
            SC_TEST_EXPECT(SerializationJson::loadVersioned(test, scrambledJson));
            SC_TEST_EXPECT(test == Test());
        }
    }
};

namespace SC
{
void runSerializationJsonTest(SC::TestReport& report) { SerializationJsonTest test(report); }
} // namespace SC
