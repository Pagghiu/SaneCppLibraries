// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "StringBuilder.h"
#include "Test.h"

namespace SC
{
struct StringBuilderTest;
}

struct SC::StringBuilderTest : public SC::TestCase
{
    StringBuilderTest(SC::TestReport& report) : TestCase(report, "StringBuilderTest")
    {
        using namespace SC;
        if (test_section("edge_cases"))
        {
            StringBuilder builder;
            SC_TEST_EXPECT(builder.appendFormatASCII(StringView(nullptr, 0, true)));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(builder.appendFormatASCII(""));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(builder.appendFormatASCII("asd"));
            SC_TEST_EXPECT(builder.toString() == "asd");
            SC_TEST_EXPECT(not builder.appendFormatASCII("asd", 1));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(not builder.appendFormatASCII("", 1));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(not builder.appendFormatASCII("{", 1));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(not builder.appendFormatASCII("}", 1));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(not builder.appendFormatASCII("{{", 1));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(not builder.appendFormatASCII("}}", 1));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(builder.appendFormatASCII("{}{{{{", 1));
            SC_TEST_EXPECT(builder.toString() == "1{{");
            SC_TEST_EXPECT(builder.appendFormatASCII("{}}}}}", 1));
            SC_TEST_EXPECT(builder.toString() == "1}}");
            SC_TEST_EXPECT(not builder.appendFormatASCII("{}}}}", 1));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(builder.appendFormatASCII("{{{}", 1));
            SC_TEST_EXPECT(builder.toString() == "{1");
            SC_TEST_EXPECT(builder.appendFormatASCII("{{{}}}-{{{}}}", 1, 2));
            SC_TEST_EXPECT(builder.toString() == "{1}-{2}");
            SC_TEST_EXPECT(not builder.appendFormatASCII("{{{{}}}-{{{}}}", 1, 2));
            SC_TEST_EXPECT(builder.toString().isEmpty());
            SC_TEST_EXPECT(not builder.appendFormatASCII("{{{{}}}-{{{}}}}", 1, 2));
            SC_TEST_EXPECT(builder.toString().isEmpty());
        }
        if (test_section("append"))
        {
            StringBuilder builder;
            SC_TEST_EXPECT(builder.append(StringView("asdf", 3, false)));
            SC_TEST_EXPECT(builder.append("asd"));
            SC_TEST_EXPECT(builder.append(String("asd")));
            SC_TEST_EXPECT(builder.toString() == "asdasdasd");
        }
        if (test_section("appendFormatASCII"))
        {
            StringBuilder builder;
            SC_TEST_EXPECT(not builder.appendFormatASCII("{", 1));
            SC_TEST_EXPECT(not builder.appendFormatASCII("", 123));
            SC_TEST_EXPECT(builder.appendFormatASCII("{}", 123));
            SC_TEST_EXPECT(builder.toString() == "123");
            SC_TEST_EXPECT(builder.appendFormatASCII("_{}", 123));
            SC_TEST_EXPECT(builder.toString() == "_123");
            SC_TEST_EXPECT(builder.appendFormatASCII("_{}_", 123));
            SC_TEST_EXPECT(builder.toString() == "_123_");
            SC_TEST_EXPECT(builder.appendFormatASCII("_{}_TEXT_{}", 123, 12.4));
            SC_TEST_EXPECT(builder.toString() == "_123_TEXT_12.400000");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{:.2f}__", 12.4567f));
            SC_TEST_EXPECT(builder.toString() == "__12.46__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", 12.4567f));
            SC_TEST_EXPECT(builder.toString() == "__12.456700__");
        }
        if (test_section("appendFormatASCII_formats"))
        {
            StringBuilder builder;
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", static_cast<uint64_t>(MaxValue())));
            SC_TEST_EXPECT(builder.toString() == "__18446744073709551615__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", static_cast<int64_t>(MaxValue())));
            SC_TEST_EXPECT(builder.toString() == "__9223372036854775807__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", float(1.2)));
            SC_TEST_EXPECT(builder.toString() == "__1.200000__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", double(1.2)));
            SC_TEST_EXPECT(builder.toString() == "__1.200000__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", ssize_t(-4)));
            SC_TEST_EXPECT(builder.toString() == "__-4__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", size_t(+4)));
            SC_TEST_EXPECT(builder.toString() == "__4__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", int32_t(-4)));
            SC_TEST_EXPECT(builder.toString() == "__-4__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", uint32_t(+4)));
            SC_TEST_EXPECT(builder.toString() == "__4__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", int16_t(-4)));
            SC_TEST_EXPECT(builder.toString() == "__-4__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", uint16_t(+4)));
            SC_TEST_EXPECT(builder.toString() == "__4__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", char('c')));
            SC_TEST_EXPECT(builder.toString() == "__c__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", "asd"));
            SC_TEST_EXPECT(builder.toString() == "__asd__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", StringView("asd")));
            SC_TEST_EXPECT(builder.toString() == "__asd__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", StringView("")));
            SC_TEST_EXPECT(builder.toString() == "____");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", StringView(nullptr, 0, true)));
            SC_TEST_EXPECT(builder.toString() == "____");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", String("asd")));
            SC_TEST_EXPECT(builder.toString() == "__asd__");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", String("")));
            SC_TEST_EXPECT(builder.toString() == "____");
            SC_TEST_EXPECT(builder.appendFormatASCII("__{}__", String()));
            SC_TEST_EXPECT(builder.toString() == "____");
        }
    }
};
