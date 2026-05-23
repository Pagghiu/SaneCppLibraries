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
        if (test_section("header builders"))
        {
            headerBuilders();
        }
        if (test_section("output reset"))
        {
            outputReset();
        }
    }

    void cookieIterator();
    void authorizationHelpers();
    void setCookieHelpers();
    void headerBuilders();
    void outputReset();
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

void HttpHeadersTest::headerBuilders()
{
    SC_TEST_EXPECT(HttpContentTypeTextPlainUtf8() == "text/plain; charset=utf-8");
    SC_TEST_EXPECT(HttpContentTypeTextHtmlUtf8() == "text/html; charset=utf-8");
    SC_TEST_EXPECT(HttpContentTypeApplicationJson() == "application/json");
    SC_TEST_EXPECT(HttpContentTypeApplicationOctetStream() == "application/octet-stream");

    char       storage[128];
    StringSpan output;

    HttpCacheControlBuilder cache;
    cache.publicCache    = true;
    cache.hasMaxAge      = true;
    cache.maxAgeSeconds  = 3600;
    cache.mustRevalidate = true;
    cache.immutable      = true;
    SC_TEST_EXPECT(cache.writeTo({storage, sizeof(storage)}, output));
    SC_TEST_EXPECT(output == "public, max-age=3600, must-revalidate, immutable");

    cache         = {};
    cache.noStore = true;
    SC_TEST_EXPECT(cache.writeTo({storage, sizeof(storage)}, output));
    SC_TEST_EXPECT(output == "no-store");

    cache              = {};
    cache.publicCache  = true;
    cache.privateCache = true;
    SC_TEST_EXPECT(not cache.writeTo({storage, sizeof(storage)}, output));
    SC_TEST_EXPECT(output.isEmpty());

    cache               = {};
    cache.hasMaxAge     = true;
    cache.maxAgeSeconds = 42;
    SC_TEST_EXPECT(not cache.writeTo({storage, 8}, output));
    SC_TEST_EXPECT(output.isEmpty());

    SC_TEST_EXPECT(HttpWriteBearerAuthorization("token", {storage, sizeof(storage)}, output));
    SC_TEST_EXPECT(output == "Bearer token");
    SC_TEST_EXPECT(HttpWriteBasicAuthorization("dXNlcjpwYXNz", {storage, sizeof(storage)}, output));
    SC_TEST_EXPECT(output == "Basic dXNlcjpwYXNz");

    SC_TEST_EXPECT(not HttpWriteBearerAuthorization("", {storage, sizeof(storage)}, output));
    SC_TEST_EXPECT(output.isEmpty());
    SC_TEST_EXPECT(not HttpWriteBasicAuthorization("dXNlcjpwYXNz", {storage, 8}, output));
    SC_TEST_EXPECT(output.isEmpty());
}

void HttpHeadersTest::outputReset()
{
    HttpHeaderKeyValue pair;
    pair.name     = "stale-name";
    pair.value    = "stale-value";
    pair.hasValue = true;

    HttpCookieIterator cookieIterator("");
    SC_TEST_EXPECT(not cookieIterator.next(pair));
    SC_TEST_EXPECT(pair.name.isEmpty());
    SC_TEST_EXPECT(pair.value.isEmpty());
    SC_TEST_EXPECT(not pair.hasValue);

    pair.name     = "stale-name";
    pair.value    = "stale-value";
    pair.hasValue = true;
    HttpSetCookieAttributeIterator attributeIterator("");
    SC_TEST_EXPECT(not attributeIterator.next(pair));
    SC_TEST_EXPECT(pair.name.isEmpty());
    SC_TEST_EXPECT(pair.value.isEmpty());
    SC_TEST_EXPECT(not pair.hasValue);

    HttpAuthorizationView authorization;
    SC_TEST_EXPECT(authorization.parse("Bearer token"));
    SC_TEST_EXPECT(not authorization.parse("missing-credentials"));
    SC_TEST_EXPECT(authorization.scheme.isEmpty());
    SC_TEST_EXPECT(authorization.credentials.isEmpty());

    StringSpan token = "stale";
    SC_TEST_EXPECT(not HttpParseBearerToken("Basic dXNlcjpwYXNz", token));
    SC_TEST_EXPECT(token.isEmpty());

    char       storage[64];
    StringSpan username = "stale-user";
    StringSpan password = "stale-pass";
    SC_TEST_EXPECT(not HttpParseBasicCredentials("Bearer token", {storage, sizeof(storage)}, username, password));
    SC_TEST_EXPECT(username.isEmpty());
    SC_TEST_EXPECT(password.isEmpty());
}

void runHttpHeadersTest(SC::TestReport& report) { HttpHeadersTest test(report); }
} // namespace SC
