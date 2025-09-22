// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/SerializationText/SerializationJson.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct Test;
} // namespace SC

//! [serializationJsonSnippet1]
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
//! [serializationJsonSnippet1]

namespace SC
{
struct SerializationJsonTest;
}

struct SC::SerializationJsonTest : public SC::TestCase
{
    inline void jsonWrite();
    inline void jsonLoadExact();
    inline void jsonLoadVersioned();
    SerializationJsonTest(SC::TestReport& report) : TestCase(report, "SerializationJsonTest")
    {
        if (test_section("SerializationJson::write"))
        {
            jsonWrite();
        }
        if (test_section("SerializationJson::loadExact"))
        {
            jsonLoadExact();
        }
        if (test_section("SerializationJson::loadVersioned"))
        {
            jsonLoadVersioned();
        }
    }
};

void SC::SerializationJsonTest::jsonWrite()
{
    //! [serializationJsonWriteSnippet]
    constexpr StringView testJSON = R"({"x":2,"y":1.50,"xy":[1,3],"myTest":"asdf","myVector":["Str1","Str2"]})"_a8;
    Test                 test;
    SmallBuffer<256>     buffer;
    StringFormatOutput   output(StringEncoding::Ascii, buffer);

    SC_TEST_EXPECT(SerializationJson::write(test, output));
    // Note: StringFormatOutput will NOT null terminate the string
    const StringView serializedJSON({buffer.data(), buffer.size()}, false, StringEncoding::Ascii);
    SC_TEST_EXPECT(serializedJSON == testJSON);
    //! [serializationJsonWriteSnippet]
}
void SC::SerializationJsonTest::jsonLoadExact()
{
    //! [serializationJsonLoadExactSnippet]
    constexpr StringView testJSON = R"({"x":2,"y":1.50,"xy":[1,3],"myTest":"asdf","myVector":["Str1","Str2"]})"_a8;

    Test test;
    test.x        = 1;
    test.y        = 3.22f;
    test.xy[0]    = 4;
    test.xy[1]    = 4;
    test.myTest   = "KFDOK";
    test.myVector = {"LPDFSOK", "DSAFKO"};
    SC_TEST_EXPECT(SerializationJson::loadExact(test, testJSON));
    SC_TEST_EXPECT(test == Test());
    //! [serializationJsonLoadExactSnippet]
}
void SC::SerializationJsonTest::jsonLoadVersioned()
{
    //! [serializationJsonLoadVersionedSnippet]
    constexpr StringView scrambledJson =
        R"({"y"  :  1.50, "x": 2.0, "myVector"  :  ["Str1","Str2"], "myTest":"asdf"})"_a8;
    Test test;
    test.x = 0;
    test.y = 0;
    (void)test.myVector.resize(1);
    (void)test.myTest.assign("FDFSA"_a8);
    SC_TEST_EXPECT(SerializationJson::loadVersioned(test, scrambledJson));
    SC_TEST_EXPECT(test == Test());
    //! [serializationJsonLoadVersionedSnippet]
}

namespace SC
{
void runSerializationJsonTest(SC::TestReport& report) { SerializationJsonTest test(report); }
} // namespace SC
