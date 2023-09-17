// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Containers/SmallVector.h"
#include "../Foundation/Strings/String.h"
#include "../Testing/Test.h"
#include "JsonFormatter.h"

namespace SC
{
struct JsonFormatterTest;
}

struct SC::JsonFormatterTest : public SC::TestCase
{
    JsonFormatterTest(SC::TestReport& report) : TestCase(report, "JsonFormatterTest")
    {
        if (test_section("JsonFormatter::value"))
        {
            SmallVector<JsonFormatter::State, 100> nestedStates;

            SmallVector<char, 256> buffer;
            StringFormatOutput     output(StringEncoding::Ascii);
            output.redirectToBuffer(buffer);
            JsonFormatter   writer(nestedStates, output);
            constexpr float fValue = 1.2f;
            SC_TEST_EXPECT(writer.writeFloat(fValue));
            float value;
            (void)StringConverter::popNulltermIfExists(buffer, StringEncoding::Ascii);
            const StringView bufferView(buffer.data(), buffer.size(), false, StringEncoding::Ascii);
            SC_TEST_EXPECT(bufferView.parseFloat(value) and value == fValue);
        }
        if (test_section("JsonFormatter::array"))
        {
            SmallVector<JsonFormatter::State, 100> nestedStates;

            SmallVector<char, 256> buffer;
            StringFormatOutput     output(StringEncoding::Ascii);
            output.redirectToBuffer(buffer);
            {
                JsonFormatter writer(nestedStates, output);
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
            (void)StringConverter::popNulltermIfExists(buffer, StringEncoding::Ascii);
            const StringView bufferView(buffer.data(), buffer.size(), false, StringEncoding::Ascii);
            SC_TEST_EXPECT(bufferView == "[[],123,[\"456\",false,null],-678]"_a8);
        }
        if (test_section("JsonFormatter::object"))
        {
            SmallVector<JsonFormatter::State, 100> nestedStates;

            SmallVector<char, 256> buffer;
            StringFormatOutput     output(StringEncoding::Ascii);
            output.redirectToBuffer(buffer);
            {
                JsonFormatter writer(nestedStates, output);
                SC_TEST_EXPECT(writer.startObject());
                SC_TEST_EXPECT(not writer.writeUint64(123));
                SC_TEST_EXPECT(writer.startObjectField("a"));
                SC_TEST_EXPECT(writer.writeInt32(-1));
                SC_TEST_EXPECT(not writer.writeUint64(123));
                SC_TEST_EXPECT(writer.startObjectField("b"));
                SC_TEST_EXPECT(not writer.startObjectField("b"));
                SC_TEST_EXPECT(writer.startArray());
                SC_TEST_EXPECT(writer.writeUint64(2));
                SC_TEST_EXPECT(writer.writeUint64(3));
                SC_TEST_EXPECT(writer.endArray());
                SC_TEST_EXPECT(not writer.endArray());
                SC_TEST_EXPECT(writer.startObjectField("c"));
                SC_TEST_EXPECT(writer.startArray());
                SC_TEST_EXPECT(writer.startObject());
                SC_TEST_EXPECT(writer.endObject());
                SC_TEST_EXPECT(writer.endArray());
                SC_TEST_EXPECT(writer.startObjectField("d"));
                SC_TEST_EXPECT(writer.startObject());
                SC_TEST_EXPECT(writer.endObject());
                SC_TEST_EXPECT(writer.endObject());
                SC_TEST_EXPECT(not writer.endObject());
            }
            (void)StringConverter::popNulltermIfExists(buffer, StringEncoding::Ascii);
            const StringView bufferView(buffer.data(), buffer.size(), false, StringEncoding::Ascii);
            SC_TEST_EXPECT(bufferView == "{\"a\":-1,\"b\":[2,3],\"c\":[{}],\"d\":{}}"_a8);
        }
    }
};
