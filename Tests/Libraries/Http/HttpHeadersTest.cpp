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
        if (test_section("authorization helpers"))
        {
            authorizationHelpers();
        }
    }

    void cookieIterator();
    void authorizationHelpers();
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

void HttpHeadersTest::authorizationHelpers()
{
    HttpAuthorizationView authorization;
    SC_TEST_EXPECT(authorization.parse("  bEaReR token-123  "));
    SC_TEST_EXPECT(authorization.isBearer());
    SC_TEST_EXPECT(not authorization.isBasic());
    SC_TEST_EXPECT(authorization.scheme == "bEaReR");
    SC_TEST_EXPECT(authorization.credentials == "token-123");

    StringSpan token;
    SC_TEST_EXPECT(HttpParseBearerToken("Bearer abc.def", token));
    SC_TEST_EXPECT(token == "abc.def");

    char       storage[64];
    StringSpan username;
    StringSpan password;
    SC_TEST_EXPECT(HttpParseBasicCredentials("Basic dXNlcjpwYXNz", {storage, sizeof(storage)}, username, password));
    SC_TEST_EXPECT(username == "user");
    SC_TEST_EXPECT(password == "pass");

    SC_TEST_EXPECT(HttpParseBasicCredentials("Basic dXNlcjo=", {storage, sizeof(storage)}, username, password));
    SC_TEST_EXPECT(username == "user");
    SC_TEST_EXPECT(password.isEmpty());

    SC_TEST_EXPECT(not HttpParseBearerToken("Basic dXNlcjpwYXNz", token));
    SC_TEST_EXPECT(not HttpParseBasicCredentials("Basic !!!=", {storage, sizeof(storage)}, username, password));
    SC_TEST_EXPECT(not HttpParseBasicCredentials("Basic bm9jb2xvbg==", {storage, sizeof(storage)}, username, password));
    SC_TEST_EXPECT(not HttpParseBasicCredentials("Basic dXNlcjpwYXNz", {storage, 3}, username, password));
}

void runHttpHeadersTest(SC::TestReport& report) { HttpHeadersTest test(report); }
} // namespace SC
