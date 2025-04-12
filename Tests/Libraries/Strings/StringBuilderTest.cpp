// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Testing/Testing.h"

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
            appendTest();
        }
        if (test_section("appendReplaceAll"))
        {
            appendReplaceAllTest();
        }
        if (test_section("appendReplaceMultiple"))
        {
            appendReplaceMultipleTest();
        }
        if (test_section("appendHex"))
        {
            appendHexTest();
        }
    }

    void appendTest();
    void appendReplaceAllTest();
    void appendReplaceMultipleTest();
    void appendHexTest();
};

void SC::StringBuilderTest::appendTest()
{
    //! [stringBuilderTestAppendSnippet]
    String        buffer(StringEncoding::Ascii);
    StringBuilder builder(buffer);
    SC_TEST_EXPECT(builder.append(StringView({"asdf", 3}, false, StringEncoding::Ascii)));
    SC_TEST_EXPECT(builder.append("asd"));
    SC_TEST_EXPECT(builder.append(String("asd").view()));
    SC_TEST_EXPECT(buffer == "asdasdasd");
    //! [stringBuilderTestAppendSnippet]
}

void SC::StringBuilderTest::appendReplaceAllTest()
{
    //! [stringBuilderTestAppendReplaceAllSnippet]
    String        buffer(StringEncoding::Ascii);
    StringBuilder builder(buffer);
    SC_TEST_EXPECT(builder.appendReplaceAll("123 456 123 10", "123", "1234"));
    SC_TEST_EXPECT(buffer == "1234 456 1234 10");
    buffer = String();
    SC_TEST_EXPECT(builder.appendReplaceAll("088123", "123", "1"));
    SC_TEST_EXPECT(buffer == "0881");
    //! [stringBuilderTestAppendReplaceAllSnippet]
}

void SC::StringBuilderTest::appendReplaceMultipleTest()
{
    //! [stringBuilderTestAppendReplaceMultipleSnippet]
    String        buffer(StringEncoding::Utf8);
    StringBuilder sb(buffer);
    SC_TEST_EXPECT(sb.appendReplaceMultiple("asd\\salve\\bas"_u8, {{"asd", "un"}, {"bas", "a_tutti"}, {"\\", "/"}}));
    SC_TEST_EXPECT(buffer == "un/salve/a_tutti");
    //! [stringBuilderTestAppendReplaceMultipleSnippet]
}

void SC::StringBuilderTest::appendHexTest()
{
    //! [stringBuilderTestAppendHexSnippet]
    uint8_t bytes[4] = {0x12, 0x34, 0x56, 0x78};

    String        buffer;
    StringBuilder builder(buffer);
    SC_TEST_EXPECT(builder.appendHex({bytes, sizeof(bytes)}, StringBuilder::AppendHexCase::UpperCase));
    SC_TEST_EXPECT(buffer.view() == "12345678");
    //! [stringBuilderTestAppendHexSnippet]
}

namespace SC
{
void runStringBuilderTest(SC::TestReport& report) { StringBuilderTest test(report); }
} // namespace SC
