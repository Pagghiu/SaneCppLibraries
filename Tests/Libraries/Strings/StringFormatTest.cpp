// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Limits.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct StringFormatTest;
}

struct SC::StringFormatTest : public SC::TestCase
{
    StringFormatTest(SC::TestReport& report) : TestCase(report, "StringFormatTest")
    {
        // Using StringBuilder in tests as it's just a tiny wrapper on StringFormat
        using namespace SC;
        if (test_section("edge_cases"))
        {
            String buffer(StringEncoding::Ascii);
            {
                auto builder = StringBuilder::create(buffer);
                SC_TEST_EXPECT(builder.append(StringView()));
                SC_TEST_EXPECT(buffer.isEmpty());
                SC_TEST_EXPECT(builder.append(""));
                SC_TEST_EXPECT(buffer.isEmpty());
                SC_TEST_EXPECT(builder.append("asd"));
                SC_TEST_EXPECT(builder.finalize() == "asd");
            }
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "asd", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "{", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "}", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "{{", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "}}", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(StringBuilder::format(buffer, "{}{{{{", 1));
            SC_TEST_EXPECT(buffer == "1{{");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "{}}}}}", 1));
            SC_TEST_EXPECT(buffer == "1}}");
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "{}}}}", 1));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(StringBuilder::format(buffer, "{{{}", 1));
            SC_TEST_EXPECT(buffer == "{1");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "{{{}}}-{{{}}}", 1, 2));
            SC_TEST_EXPECT(buffer == "{1}-{2}");
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "{{{{}}}-{{{}}}", 1, 2));
            SC_TEST_EXPECT(buffer.isEmpty());
            SC_TEST_EXPECT(not StringBuilder::format(buffer, "{{{{}}}-{{{}}}}", 1, 2));
            SC_TEST_EXPECT(buffer.isEmpty());
        }
        if (test_section("append"))
        {
            String buffer(StringEncoding::Ascii);
            {
                auto builder = StringBuilder::create(buffer);
                SC_TEST_EXPECT(not builder.append("{", 1));
                SC_TEST_EXPECT(not builder.append("", 123));
                SC_TEST_EXPECT(builder.append("{}", 123));
                SC_TEST_EXPECT(builder.finalize() == "123");
            }
            SC_TEST_EXPECT(StringBuilder::format(buffer, "_{}", 123));
            SC_TEST_EXPECT(buffer == "_123");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "_{}_", 123));
            SC_TEST_EXPECT(buffer == "_123_");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "_{}_TEXT_{}", 123, 12.4));
            SC_TEST_EXPECT(buffer == "_123_TEXT_12.400000");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{:.2}__", 12.4567f));
            SC_TEST_EXPECT(buffer == "__12.46__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", 12.4567f));
            SC_TEST_EXPECT(buffer == "__12.456700__");
        }
        if (test_section("append_formats"))
        {
            String buffer(StringEncoding::Ascii);
            auto   builder = StringBuilder::create(buffer);
            SC_TEST_EXPECT(builder.append("__{}__", static_cast<uint64_t>(MaxValue())));
            SC_TEST_EXPECT(builder.finalize() == "__18446744073709551615__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", static_cast<int64_t>(MaxValue())));
            SC_TEST_EXPECT(buffer == "__9223372036854775807__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", float(1.2)));
            SC_TEST_EXPECT(buffer == "__1.200000__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", double(1.2)));
            SC_TEST_EXPECT(buffer == "__1.200000__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", ssize_t(-4)));
            SC_TEST_EXPECT(buffer == "__-4__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", size_t(+4)));
            SC_TEST_EXPECT(buffer == "__4__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", int32_t(-4)));
            SC_TEST_EXPECT(buffer == "__-4__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", uint32_t(+4)));
            SC_TEST_EXPECT(buffer == "__4__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", int16_t(-4)));
            SC_TEST_EXPECT(buffer == "__-4__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", uint16_t(+4)));
            SC_TEST_EXPECT(buffer == "__4__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", char('c')));
            SC_TEST_EXPECT(buffer == "__c__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", "asd"));
            SC_TEST_EXPECT(buffer == "__asd__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", StringView("asd")));
            SC_TEST_EXPECT(buffer == "__asd__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", StringView("")));
            SC_TEST_EXPECT(buffer == "____");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", StringView()));
            SC_TEST_EXPECT(buffer == "____");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", String("asd")));
            SC_TEST_EXPECT(buffer == "__asd__");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", String("")));
            SC_TEST_EXPECT(buffer == "____");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "__{}__", String()));
            SC_TEST_EXPECT(buffer == "____");
        }
        if (test_section("format positional args"))
        {
            String buffer(StringEncoding::Ascii);
            SC_TEST_EXPECT(StringBuilder::format(buffer, "{1}_{0}_{1}", 1, 0));
            SC_TEST_EXPECT(buffer == "0_1_0");
            SC_TEST_EXPECT(StringBuilder::format(buffer, "{0:.2}_{1}_{0:.4}", 1.2222, "salve"));
            SC_TEST_EXPECT(buffer == "1.22_salve_1.2222");
        }
    }
};

namespace SC
{
void runStringFormatTest(SC::TestReport& report) { StringFormatTest test(report); }
} // namespace SC
