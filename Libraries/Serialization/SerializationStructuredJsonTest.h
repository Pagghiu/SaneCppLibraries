// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "SerializationStructuredJson.h"
#include "SerializationStructuredReadWriteFast.h"

#include "../Foundation/SmallVector.h"
#include "../Foundation/StringBuilder.h"
#include "../Testing/Test.h"

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
            Test               test;
            SmallString<256>   buffer;
            StringFormatOutput output(StringEncoding::Ascii);
            output.redirectToBuffer(buffer.data);
            SerializationStructuredTemplate::SerializationJsonWriter jsBuffer{output};
            using JsonWriter = SerializationStructuredTemplate::SerializationReadWriteFast<decltype(jsBuffer), Test>;
            JsonWriter serializer;
            SC_TEST_EXPECT(serializer.startSerialization(test, jsBuffer));
            SC_TEST_EXPECT(buffer.view() == simpleJson);
        }
        if (test_section("JsonReaderFast"))
        {
            SerializationStructuredTemplate::SerializationJsonReader jsBuffer{
                simpleJson.getIterator<StringIteratorASCII>(), simpleJson};
            using JsonReader = SerializationStructuredTemplate::SerializationReadWriteFast<
                SerializationStructuredTemplate::SerializationJsonReader, Test>;
            JsonReader serializer;
            Test       test;
            memset(&test, 0, sizeof(test));
            SC_TEST_EXPECT(serializer.startSerialization(test, jsBuffer));
            SC_TEST_EXPECT(test == Test());
        }
    }
};
