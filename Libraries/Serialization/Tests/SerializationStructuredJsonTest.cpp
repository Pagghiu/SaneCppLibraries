// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../SerializationStructuredJson.h"
#include "../SerializationStructuredReadWrite.h"

#include "../../Containers/SmallVector.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Test.h"

namespace SC
{
struct Test;
} // namespace SC

struct SC::Test
{
    int            x        = 2;
    float          y        = 1.5f;
    int            xy[2]    = {1, 3};
    String         myTest   = "asdf"_a8;
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
SC_META_STRUCT_VISIT(SC::Test)
SC_META_STRUCT_FIELD(0, x)
SC_META_STRUCT_FIELD(1, y)
SC_META_STRUCT_FIELD(2, xy)
SC_META_STRUCT_FIELD(3, myTest)
SC_META_STRUCT_FIELD(4, myVector)
SC_META_STRUCT_LEAVE()

namespace SC
{
struct SerializationStructuredJsonTest;
}

struct SC::SerializationStructuredJsonTest : public SC::TestCase
{
    SerializationStructuredJsonTest(SC::TestReport& report) : TestCase(report, "SerializationStructuredJsonTest")
    {
        constexpr StringView simpleJson =
            R"({"x":2,"y":1.50,"xy":[1,3],"myTest":"asdf","myVector":["Str1","Str2"]})"_a8;
        if (test_section("JsonWriterFast"))
        {
            Test                                                     test;
            SmallVector<char, 256>                                   buffer;
            StringFormatOutput                                       output(StringEncoding::Ascii, buffer);
            SerializationStructuredTemplate::SerializationJsonWriter writer(output);
            SC_TEST_EXPECT(SerializationStructuredTemplate::serialize(test, writer));
            (void)StringConverter::popNulltermIfExists(buffer, StringEncoding::Ascii);
            const StringView bufferView(buffer.data(), buffer.size(), false, StringEncoding::Ascii);
            SC_TEST_EXPECT(bufferView == simpleJson);
        }
        if (test_section("JsonReaderFast"))
        {
            Test test;
            memset(&test, 0, sizeof(test));
            SerializationStructuredTemplate::SerializationJsonReader reader(simpleJson);
            SC_TEST_EXPECT(SerializationStructuredTemplate::serialize(test, reader));
            SC_TEST_EXPECT(test == Test());
        }
        if (test_section("JsonReaderVersioned"))
        {
            constexpr StringView scrambledJson =
                R"({"y"  :  1.50, "x": 2.0, "myVector"  :  ["Str1","Str2"], "myTest":"asdf"})"_a8;
            Test test;
            test.x = 0;
            test.y = 0;
            (void)test.myVector.resize(1);
            (void)test.myTest.assign("FDFSA"_a8);
            SerializationStructuredTemplate::SerializationJsonReader reader(scrambledJson);
            SC_TEST_EXPECT(SerializationStructuredTemplate::loadVersioned(test, reader));
            SC_TEST_EXPECT(test == Test());
        }
    }
};

namespace SC
{
void runSerializationStructuredJsonTest(SC::TestReport& report) { SerializationStructuredJsonTest test(report); }
} // namespace SC
