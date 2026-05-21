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
        if (test_section("set-cookie helpers"))
        {
            setCookieHelpers();
        }
    }

    void cookieIterator();
    void authorizationHelpers();
    void setCookieHelpers();
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

void HttpHeadersTest::setCookieHelpers()
{
    HttpSetCookieView cookie;
    SC_TEST_EXPECT(cookie.parse(" id = 42 ; Path=/app; Domain=example.com; Expires=Wed, 21 Oct 2015 07:28:00 GMT; "
                                "Max-Age=3600; Secure; HttpOnly; SameSite=Lax; Priority=High "));
    SC_TEST_EXPECT(cookie.name == "id");
    SC_TEST_EXPECT(cookie.value == "42");
    SC_TEST_EXPECT(cookie.path == "/app");
    SC_TEST_EXPECT(cookie.domain == "example.com");
    SC_TEST_EXPECT(cookie.expires == "Wed, 21 Oct 2015 07:28:00 GMT");
    SC_TEST_EXPECT(cookie.maxAge == "3600");
    SC_TEST_EXPECT(cookie.hasMaxAge);
    SC_TEST_EXPECT(cookie.secure);
    SC_TEST_EXPECT(cookie.httpOnly);
    SC_TEST_EXPECT(cookie.sameSite == "Lax");

    HttpSetCookieAttributeIterator it(cookie.attributes);
    HttpHeaderKeyValue             attribute;
    bool                           foundPriority = false;
    while (it.next(attribute))
    {
        if (attribute.name == "Priority")
        {
            foundPriority = true;
            SC_TEST_EXPECT(attribute.value == "High");
            SC_TEST_EXPECT(attribute.hasValue);
        }
    }
    SC_TEST_EXPECT(foundPriority);

    char                 storage[256];
    StringSpan           output;
    HttpSetCookieBuilder builder;
    builder.name     = "id";
    builder.value    = "42";
    builder.path     = "/app";
    builder.domain   = "example.com";
    builder.maxAge   = "3600";
    builder.secure   = true;
    builder.httpOnly = true;
    builder.sameSite = "Strict";
    SC_TEST_EXPECT(builder.writeTo({storage, sizeof(storage)}, output));
    SC_TEST_EXPECT(output == "id=42; Path=/app; Domain=example.com; Max-Age=3600; Secure; HttpOnly; SameSite=Strict");

    HttpSetCookieView roundTrip;
    SC_TEST_EXPECT(roundTrip.parse(output));
    SC_TEST_EXPECT(roundTrip.name == "id");
    SC_TEST_EXPECT(roundTrip.value == "42");
    SC_TEST_EXPECT(roundTrip.path == "/app");
    SC_TEST_EXPECT(roundTrip.domain == "example.com");
    SC_TEST_EXPECT(roundTrip.maxAge == "3600");
    SC_TEST_EXPECT(roundTrip.secure);
    SC_TEST_EXPECT(roundTrip.httpOnly);
    SC_TEST_EXPECT(roundTrip.sameSite == "Strict");

    SC_TEST_EXPECT(not builder.writeTo({storage, 4}, output));
    SC_TEST_EXPECT(not cookie.parse("=missing-name"));
    SC_TEST_EXPECT(not cookie.parse("missing-value"));
}

void runHttpHeadersTest(SC::TestReport& report) { HttpHeadersTest test(report); }
} // namespace SC
