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
            SC_TEST_EXPECT(builder.append(StringView(nullptr, 0, true, StringEncoding::Ascii)));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(builder.append(""));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(builder.append("asd"));
            SC_TEST_EXPECT(builder.releaseString() == "asd");
            SC_TEST_EXPECT(not builder.append("asd", 1));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(not builder.append("", 1));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(not builder.append("{", 1));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(not builder.append("}", 1));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(not builder.append("{{", 1));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(not builder.append("}}", 1));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(builder.append("{}{{{{", 1));
            SC_TEST_EXPECT(builder.releaseString() == "1{{");
            SC_TEST_EXPECT(builder.append("{}}}}}", 1));
            SC_TEST_EXPECT(builder.releaseString() == "1}}");
            SC_TEST_EXPECT(not builder.append("{}}}}", 1));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(builder.append("{{{}", 1));
            SC_TEST_EXPECT(builder.releaseString() == "{1");
            SC_TEST_EXPECT(builder.append("{{{}}}-{{{}}}", 1, 2));
            SC_TEST_EXPECT(builder.releaseString() == "{1}-{2}");
            SC_TEST_EXPECT(not builder.append("{{{{}}}-{{{}}}", 1, 2));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
            SC_TEST_EXPECT(not builder.append("{{{{}}}-{{{}}}}", 1, 2));
            SC_TEST_EXPECT(builder.releaseString().isEmpty());
        }
        if (test_section("append"))
        {
            StringBuilder builder;
            SC_TEST_EXPECT(builder.append(StringView("asdf", 3, false, StringEncoding::Ascii)));
            SC_TEST_EXPECT(builder.append("asd"));
            SC_TEST_EXPECT(builder.append(String("asd")));
            SC_TEST_EXPECT(builder.releaseString() == "asdasdasd");
        }
        if (test_section("append"))
        {
            StringBuilder builder;
            SC_TEST_EXPECT(not builder.append("{", 1));
            SC_TEST_EXPECT(not builder.append("", 123));
            SC_TEST_EXPECT(builder.append("{}", 123));
            SC_TEST_EXPECT(builder.releaseString() == "123");
            SC_TEST_EXPECT(builder.append("_{}", 123));
            SC_TEST_EXPECT(builder.releaseString() == "_123");
            SC_TEST_EXPECT(builder.append("_{}_", 123));
            SC_TEST_EXPECT(builder.releaseString() == "_123_");
            SC_TEST_EXPECT(builder.append("_{}_TEXT_{}", 123, 12.4));
            SC_TEST_EXPECT(builder.releaseString() == "_123_TEXT_12.400000");
            SC_TEST_EXPECT(builder.append("__{:.2}__", 12.4567f));
            SC_TEST_EXPECT(builder.releaseString() == "__12.46__");
            SC_TEST_EXPECT(builder.append("__{}__", 12.4567f));
            SC_TEST_EXPECT(builder.releaseString() == "__12.456700__");
        }
        if (test_section("append_formats"))
        {
            StringBuilder builder;
            SC_TEST_EXPECT(builder.append("__{}__", static_cast<uint64_t>(MaxValue())));
            SC_TEST_EXPECT(builder.releaseString() == "__18446744073709551615__");
            SC_TEST_EXPECT(builder.append("__{}__", static_cast<int64_t>(MaxValue())));
            SC_TEST_EXPECT(builder.releaseString() == "__9223372036854775807__");
            SC_TEST_EXPECT(builder.append("__{}__", float(1.2)));
            SC_TEST_EXPECT(builder.releaseString() == "__1.200000__");
            SC_TEST_EXPECT(builder.append("__{}__", double(1.2)));
            SC_TEST_EXPECT(builder.releaseString() == "__1.200000__");
            SC_TEST_EXPECT(builder.append("__{}__", ssize_t(-4)));
            SC_TEST_EXPECT(builder.releaseString() == "__-4__");
            SC_TEST_EXPECT(builder.append("__{}__", size_t(+4)));
            SC_TEST_EXPECT(builder.releaseString() == "__4__");
            SC_TEST_EXPECT(builder.append("__{}__", int32_t(-4)));
            SC_TEST_EXPECT(builder.releaseString() == "__-4__");
            SC_TEST_EXPECT(builder.append("__{}__", uint32_t(+4)));
            SC_TEST_EXPECT(builder.releaseString() == "__4__");
            SC_TEST_EXPECT(builder.append("__{}__", int16_t(-4)));
            SC_TEST_EXPECT(builder.releaseString() == "__-4__");
            SC_TEST_EXPECT(builder.append("__{}__", uint16_t(+4)));
            SC_TEST_EXPECT(builder.releaseString() == "__4__");
            SC_TEST_EXPECT(builder.append("__{}__", char('c')));
            SC_TEST_EXPECT(builder.releaseString() == "__c__");
            SC_TEST_EXPECT(builder.append("__{}__", "asd"));
            SC_TEST_EXPECT(builder.releaseString() == "__asd__");
            SC_TEST_EXPECT(builder.append("__{}__", StringView("asd")));
            SC_TEST_EXPECT(builder.releaseString() == "__asd__");
            SC_TEST_EXPECT(builder.append("__{}__", StringView("")));
            SC_TEST_EXPECT(builder.releaseString() == "____");
            SC_TEST_EXPECT(builder.append("__{}__", StringView(nullptr, 0, true, StringEncoding::Ascii)));
            SC_TEST_EXPECT(builder.releaseString() == "____");
            SC_TEST_EXPECT(builder.append("__{}__", String("asd")));
            SC_TEST_EXPECT(builder.releaseString() == "__asd__");
            SC_TEST_EXPECT(builder.append("__{}__", String("")));
            SC_TEST_EXPECT(builder.releaseString() == "____");
            SC_TEST_EXPECT(builder.append("__{}__", String()));
            SC_TEST_EXPECT(builder.releaseString() == "____");
        }
    }
};
