// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/SmallVector.h"
#include "../Foundation/String.h"
#include "../Testing/Test.h"
#include "JsonStreamWriter.h"

namespace SC
{
struct JsonStreamWriterTest;
}

struct SC::JsonStreamWriterTest : public SC::TestCase
{
    JsonStreamWriterTest(SC::TestReport& report) : TestCase(report, "JsonStreamWriterTest")
    {
        if (test_section("JsonStreamWriter::value"))
        {
            SmallVector<JsonStreamWriter::State, 100> nestedStates;

            SmallString<255>   buffer;
            StringFormatOutput output(StringEncoding::Ascii);
            output.redirectToBuffer(buffer.data);
            JsonStreamWriter writer(nestedStates, output);
            constexpr float  fValue = 1.2f;
            SC_TEST_EXPECT(writer.writeFloat(fValue));
            float value;
            SC_TEST_EXPECT(buffer.view().parseFloat(value) and value == fValue);
        }
        if (test_section("JsonStreamWriter::array"))
        {
            SmallVector<JsonStreamWriter::State, 100> nestedStates;

            SmallString<255>   buffer;
            StringFormatOutput output(StringEncoding::Ascii);
            output.redirectToBuffer(buffer.data);
            {
                JsonStreamWriter writer(nestedStates, output);
                SC_TEST_EXPECT(writer.startArray());
                SC_TEST_EXPECT(writer.startArray());
                SC_TEST_EXPECT(writer.endArray());
                SC_TEST_EXPECT(writer.writeUint32(123));
                SC_TEST_EXPECT(writer.startArray());
                SC_TEST_EXPECT(writer.writeString("456"_a8));
                SC_TEST_EXPECT(writer.writeBoolean(false));
                SC_TEST_EXPECT(writer.writeNull());
                SC_TEST_EXPECT(writer.endArray());
                SC_TEST_EXPECT(writer.writeInt64(-678));
                SC_TEST_EXPECT(writer.endArray());
            }
            SC_TEST_EXPECT(buffer.view() == "[[],123,[\"456\",false,null],-678]"_a8);
        }
        if (test_section("JsonStreamWriter::object"))
        {
            SmallVector<JsonStreamWriter::State, 100> nestedStates;

            SmallString<255>   buffer;
            StringFormatOutput output(StringEncoding::Ascii);
            output.redirectToBuffer(buffer.data);
            {
                JsonStreamWriter writer(nestedStates, output);
                SC_TEST_EXPECT(writer.startObject());
                SC_TEST_EXPECT(not writer.writeUint64(123));
                SC_TEST_EXPECT(writer.objectFieldName("a"));
                SC_TEST_EXPECT(writer.writeInt32(-1));
                SC_TEST_EXPECT(not writer.writeUint64(123));
                SC_TEST_EXPECT(writer.objectFieldName("b"));
                SC_TEST_EXPECT(not writer.objectFieldName("b"));
                SC_TEST_EXPECT(writer.startArray());
                SC_TEST_EXPECT(writer.writeUint64(2));
                SC_TEST_EXPECT(writer.writeUint64(3));
                SC_TEST_EXPECT(writer.endArray());
                SC_TEST_EXPECT(not writer.endArray());
                SC_TEST_EXPECT(writer.objectFieldName("c"));
                SC_TEST_EXPECT(writer.startArray());
                SC_TEST_EXPECT(writer.startObject());
                SC_TEST_EXPECT(writer.endObject());
                SC_TEST_EXPECT(writer.endArray());
                SC_TEST_EXPECT(writer.objectFieldName("d"));
                SC_TEST_EXPECT(writer.startObject());
                SC_TEST_EXPECT(writer.endObject());
                SC_TEST_EXPECT(writer.endObject());
                SC_TEST_EXPECT(not writer.endObject());
            }
            SC_TEST_EXPECT(buffer.view() == "{\"a\":-1,\"b\":[2,3],\"c\":[{}],\"d\":{}}"_a8);
        }
    }
};
