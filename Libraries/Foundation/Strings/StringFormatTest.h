// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "StringBuilder.h"

namespace SC
{
struct StringFormatTest;
}

struct SC::StringFormatTest : public SC::TestCase
{
    StringFormatTest(SC::TestReport& report) : TestCase(report, "StringFormatTest")
    {
        using namespace SC;
        if (test_section("edge_cases"))
        {
            String        buffer(StringEncoding::Ascii);
            StringBuilder builder(buffer);
            SC_TEST_EXPECT(builder.append(StringView()));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(builder.append(""));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(builder.append("asd"));
            SC_TEST_EXPECT(buffer == "asd");
            SC_TEST_EXPECT(not builder.format("asd", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not builder.format("", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not builder.format("{", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not builder.format("}", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not builder.format("{{", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not builder.format("}}", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(builder.format("{}{{{{", 1));
            SC_TEST_EXPECT(buffer == "1{{");
            SC_TEST_EXPECT(builder.format("{}}}}}", 1));
            SC_TEST_EXPECT(buffer == "1}}");
            SC_TEST_EXPECT(not builder.format("{}}}}", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(builder.format("{{{}", 1));
            SC_TEST_EXPECT(buffer == "{1");
            SC_TEST_EXPECT(builder.format("{{{}}}-{{{}}}", 1, 2));
            SC_TEST_EXPECT(buffer == "{1}-{2}");
            SC_TEST_EXPECT(not builder.format("{{{{}}}-{{{}}}", 1, 2));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not builder.format("{{{{}}}-{{{}}}}", 1, 2));
            SC_TEST_EXPECT(buffer.isEmpty());
        }
        if (test_section("append"))
        {
            String        buffer(StringEncoding::Ascii);
            StringBuilder builder(buffer);
            SC_TEST_EXPECT(builder.append(StringView("asdf", 3, false, StringEncoding::Ascii)));
            SC_TEST_EXPECT(builder.append("asd"));
            SC_TEST_EXPECT(builder.append(String("asd").view()));
            SC_TEST_EXPECT(buffer == "asdasdasd");
        }
        if (test_section("append"))
        {
            String        buffer(StringEncoding::Ascii);
            StringBuilder builder(buffer);
            SC_TEST_EXPECT(not builder.append("{", 1));
            SC_TEST_EXPECT(not builder.append("", 123));
            SC_TEST_EXPECT(builder.append("{}", 123));
            SC_TEST_EXPECT(buffer == "123");
            SC_TEST_EXPECT(builder.format("_{}", 123));
            SC_TEST_EXPECT(buffer == "_123");
            SC_TEST_EXPECT(builder.format("_{}_", 123));
            SC_TEST_EXPECT(buffer == "_123_");
            SC_TEST_EXPECT(builder.format("_{}_TEXT_{}", 123, 12.4));
            SC_TEST_EXPECT(buffer == "_123_TEXT_12.400000");
            SC_TEST_EXPECT(builder.format("__{:.2}__", 12.4567f));
            SC_TEST_EXPECT(buffer == "__12.46__");
            SC_TEST_EXPECT(builder.format("__{}__", 12.4567f));
            SC_TEST_EXPECT(buffer == "__12.456700__");
        }
        if (test_section("append_formats"))
        {
            String        buffer(StringEncoding::Ascii);
            StringBuilder builder(buffer);
            SC_TEST_EXPECT(builder.append("__{}__", static_cast<uint64_t>(MaxValue())));
            SC_TEST_EXPECT(buffer == "__18446744073709551615__");
            SC_TEST_EXPECT(builder.format("__{}__", static_cast<int64_t>(MaxValue())));
            SC_TEST_EXPECT(buffer == "__9223372036854775807__");
            SC_TEST_EXPECT(builder.format("__{}__", float(1.2)));
            SC_TEST_EXPECT(buffer == "__1.200000__");
            SC_TEST_EXPECT(builder.format("__{}__", double(1.2)));
            SC_TEST_EXPECT(buffer == "__1.200000__");
            SC_TEST_EXPECT(builder.format("__{}__", ssize_t(-4)));
            SC_TEST_EXPECT(buffer == "__-4__");
            SC_TEST_EXPECT(builder.format("__{}__", size_t(+4)));
            SC_TEST_EXPECT(buffer == "__4__");
            SC_TEST_EXPECT(builder.format("__{}__", int32_t(-4)));
            SC_TEST_EXPECT(buffer == "__-4__");
            SC_TEST_EXPECT(builder.format("__{}__", uint32_t(+4)));
            SC_TEST_EXPECT(buffer == "__4__");
            SC_TEST_EXPECT(builder.format("__{}__", int16_t(-4)));
            SC_TEST_EXPECT(buffer == "__-4__");
            SC_TEST_EXPECT(builder.format("__{}__", uint16_t(+4)));
            SC_TEST_EXPECT(buffer == "__4__");
            SC_TEST_EXPECT(builder.format("__{}__", char('c')));
            SC_TEST_EXPECT(buffer == "__c__");
            SC_TEST_EXPECT(builder.format("__{}__", "asd"));
            SC_TEST_EXPECT(buffer == "__asd__");
            SC_TEST_EXPECT(builder.format("__{}__", StringView("asd")));
            SC_TEST_EXPECT(buffer == "__asd__");
            SC_TEST_EXPECT(builder.format("__{}__", StringView("")));
            SC_TEST_EXPECT(buffer == "____");
            SC_TEST_EXPECT(builder.format("__{}__", StringView()));
            SC_TEST_EXPECT(buffer == "____");
            SC_TEST_EXPECT(builder.format("__{}__", String("asd")));
            SC_TEST_EXPECT(buffer == "__asd__");
            SC_TEST_EXPECT(builder.format("__{}__", String("")));
            SC_TEST_EXPECT(buffer == "____");
            SC_TEST_EXPECT(builder.format("__{}__", String()));
            SC_TEST_EXPECT(buffer == "____");
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
            SC_TEST_EXPECT(builder.appendReplaceMultiple(
                "asd\\salve\\bas"_u8, {{"asd"_a8, "un"_a8}, {"bas"_a8, "a_tutti"_a8}, {"\\"_a8, "/"}}));
            SC_TEST_EXPECT(buffer == "un/salve/a_tutti");
        }
    }
};
