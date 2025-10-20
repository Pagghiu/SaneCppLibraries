// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Memory/String.h"
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
        if (test_section("appendHex"))
        {
            appendHexTest();
        }
        if (test_section("format"))
        {
            formatTest();
        }
    }

    void appendTest();
    void appendReplaceAllTest();
    void appendHexTest();
    void formatTest();
};

void SC::StringBuilderTest::appendTest()
{
    //! [stringBuilderTestAppendSnippet]
    String buffer(StringEncoding::Ascii);           // or SmallString<N> / Buffer
    auto   builder = StringBuilder::create(buffer); // or StringBuilder::createAppend(buffer)
    SC_TEST_EXPECT(builder.append("Salve"));
    SC_TEST_EXPECT(builder.append(" {1} {0}!!!", "tutti", "a"));
    SC_ASSERT_RELEASE(builder.finalize() == "Salve a tutti!!!");
    //! [stringBuilderTestAppendSnippet]
}

void SC::StringBuilderTest::appendReplaceAllTest()
{
    //! [stringBuilderTestAppendReplaceAllSnippet]
    String buffer(StringEncoding::Ascii);
    {
        auto builder = StringBuilder::create(buffer);
        SC_TEST_EXPECT(builder.appendReplaceAll("123 456 123 10", "123", "1234"));
        SC_TEST_EXPECT(builder.finalize() == "1234 456 1234 10");
    }
    buffer = String();
    {
        auto builder = StringBuilder::create(buffer);
        SC_TEST_EXPECT(builder.appendReplaceAll("088123", "123", "1"));
        SC_TEST_EXPECT(builder.finalize() == "0881");
    }
    //! [stringBuilderTestAppendReplaceAllSnippet]
}

void SC::StringBuilderTest::appendHexTest()
{
    //! [stringBuilderTestAppendHexSnippet]
    uint8_t bytes[4] = {0x12, 0x34, 0x56, 0x78};

    String buffer;
    auto   builder = StringBuilder::create(buffer);
    SC_TEST_EXPECT(builder.appendHex({bytes, sizeof(bytes)}, StringBuilder::AppendHexCase::UpperCase));
    SC_TEST_EXPECT(builder.finalize() == "12345678");
    //! [stringBuilderTestAppendHexSnippet]
}

void SC::StringBuilderTest::formatTest()
{
    //! [stringBuilderFormatSnippet]
    String buffer(StringEncoding::Ascii); // or SmallString<N> / Buffer
    SC_TEST_EXPECT(StringBuilder::format(buffer, "[{1}-{0}]", "Storia", "Bella"));
    SC_TEST_EXPECT(buffer.view() == "[Bella-Storia]");
    //! [stringBuilderFormatSnippet]
}

namespace SC
{
void runStringBuilderTest(SC::TestReport& report) { StringBuilderTest test(report); }
} // namespace SC
