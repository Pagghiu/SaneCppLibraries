// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpHeaders.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct HttpHeadersTest : public TestCase
{
    HttpHeadersTest(SC::TestReport& report) : TestCase(report, "HttpHeadersTest")
    {
        if (test_section("cookie iterator"))
        {
            cookieIterator();
        }
    }

    void cookieIterator();
};

void HttpHeadersTest::cookieIterator()
{
    HttpCookieIterator it("  session = abc ; theme=light; empty= ; repeated=one; repeated=two ; flag ; ; raw=%20+  ");
    HttpHeaderKeyValue pair;

    SC_TEST_EXPECT(it.next(pair));
    SC_TEST_EXPECT(pair.name == "session");
    SC_TEST_EXPECT(pair.value == "abc");
    SC_TEST_EXPECT(pair.hasValue);

    SC_TEST_EXPECT(it.next(pair));
    SC_TEST_EXPECT(pair.name == "theme");
    SC_TEST_EXPECT(pair.value == "light");
    SC_TEST_EXPECT(pair.hasValue);

    SC_TEST_EXPECT(it.next(pair));
    SC_TEST_EXPECT(pair.name == "empty");
    SC_TEST_EXPECT(pair.value.isEmpty());
    SC_TEST_EXPECT(pair.hasValue);

    SC_TEST_EXPECT(it.next(pair));
    SC_TEST_EXPECT(pair.name == "repeated");
    SC_TEST_EXPECT(pair.value == "one");
    SC_TEST_EXPECT(pair.hasValue);

    SC_TEST_EXPECT(it.next(pair));
    SC_TEST_EXPECT(pair.name == "repeated");
    SC_TEST_EXPECT(pair.value == "two");
    SC_TEST_EXPECT(pair.hasValue);

    SC_TEST_EXPECT(it.next(pair));
    SC_TEST_EXPECT(pair.name == "flag");
    SC_TEST_EXPECT(pair.value.isEmpty());
    SC_TEST_EXPECT(not pair.hasValue);

    SC_TEST_EXPECT(it.next(pair));
    SC_TEST_EXPECT(pair.name == "raw");
    SC_TEST_EXPECT(pair.value == "%20+");
    SC_TEST_EXPECT(pair.hasValue);

    SC_TEST_EXPECT(not it.next(pair));
}

void runHttpHeadersTest(SC::TestReport& report) { HttpHeadersTest test(report); }
} // namespace SC
