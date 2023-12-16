// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../StringBuilder.h"
#include "../../Testing/Testing.h"
#include "../String.h"

namespace SC
{
struct StringBuilderTest;
}

struct SC::StringBuilderTest : public SC::TestCase
{
    StringBuilderTest(SC::TestReport& report) : TestCase(report, "StringBuilderTest")
    {
        // StringBuilder::format and StringBuilder::append (with format args) are tested in StringFormatTest
        using namespace SC;
        if (test_section("append"))
        {
            String        buffer(StringEncoding::Ascii);
            StringBuilder builder(buffer);
            SC_TEST_EXPECT(builder.append(StringView({"asdf", 3}, false, StringEncoding::Ascii)));
            SC_TEST_EXPECT(builder.append("asd"));
            SC_TEST_EXPECT(builder.append(String("asd").view()));
            SC_TEST_EXPECT(buffer == "asdasdasd");
        }
        if (test_section("appendReplaceAll"))
        {
            String        buffer(StringEncoding::Ascii);
            StringBuilder builder(buffer);
            SC_TEST_EXPECT(builder.appendReplaceAll("123 456 123 10", "123", "1234"));
            SC_TEST_EXPECT(buffer == "1234 456 1234 10");
            buffer = String();
            SC_TEST_EXPECT(builder.appendReplaceAll("088123", "123", "1"));
            SC_TEST_EXPECT(buffer == "0881");
        }
        if (test_section("appendReplaceMultiple"))
        {
            String        buffer(StringEncoding::Utf8);
            StringBuilder builder(buffer);
            SC_TEST_EXPECT(
                builder.appendReplaceMultiple("asd\\salve\\bas"_u8, {{"asd", "un"}, {"bas", "a_tutti"}, {"\\", "/"}}));
            SC_TEST_EXPECT(buffer == "un/salve/a_tutti");
        }
        if (test_section("appendHex"))
        {
            uint8_t bytes[4] = {0x12, 0x34, 0x56, 0x78};

            String        s;
            StringBuilder b(s);
            SC_TEST_EXPECT(b.appendHex({bytes, sizeof(bytes)}) and s.view() == "12345678");
        }
    }
};

namespace SC
{
void runStringBuilderTest(SC::TestReport& report) { StringBuilderTest test(report); }
} // namespace SC
