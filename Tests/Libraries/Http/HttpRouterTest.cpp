// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpRouter.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
namespace
{
static bool resultMessageEquals(Result result, StringSpan expected)
{
    return not result and StringSpan::fromNullTerminated(result.message, StringEncoding::Ascii) == expected;
}
} // namespace

struct HttpRouterTest : public TestCase
{
    HttpRouterTest(SC::TestReport& report) : TestCase(report, "HttpRouterTest")
    {
        if (test_section("route matching"))
        {
            routeMatching();
        }
        if (test_section("diagnostic messages"))
        {
            diagnosticMessages();
        }
    }

    void routeMatching();
    void diagnosticMessages();
};

void HttpRouterTest::routeMatching()
{
    const HttpRoute routes[] = {
        {HttpParser::Method::HttpGET, "/health"},
        {HttpParser::Method::HttpGET, "/users/:id"},
        {HttpParser::Method::HttpPUT, "/users/:id"},
        {HttpParser::Method::HttpDELETE, "/users/:id/books/:book"},
    };

    HttpRouter router;
    SC_TEST_EXPECT(router.init(routes));

    HttpRouteParam params[2];
    HttpRouteMatch match;
    SC_TEST_EXPECT(router.match(HttpParser::Method::HttpGET, "/users/42?verbose=1", params, match));
    SC_TEST_EXPECT(match.status == HttpRouteMatchStatus::Matched);
    SC_TEST_EXPECT(match.route == &routes[1]);
    SC_TEST_EXPECT(match.numParams == 1);
    SC_TEST_EXPECT(params[0].name == "id");
    SC_TEST_EXPECT(params[0].value == "42");

    SC_TEST_EXPECT(router.match(HttpParser::Method::HttpPOST, "/users/42", params, match));
    SC_TEST_EXPECT(match.status == HttpRouteMatchStatus::MethodNotAllowed);
    SC_TEST_EXPECT(match.route == nullptr);

    char       allowStorage[64];
    StringSpan allow;
    SC_TEST_EXPECT(router.formatAllowHeader("/users/42?verbose=1", {allowStorage, sizeof(allowStorage)}, allow));
    SC_TEST_EXPECT(allow == "GET, PUT");
    SC_TEST_EXPECT(router.formatAllowHeader("/users/42#fragment", {allowStorage, sizeof(allowStorage)}, allow));
    SC_TEST_EXPECT(allow == "GET, PUT");

    SC_TEST_EXPECT(router.match(HttpParser::Method::HttpDELETE, "/users/42/books/abc", params, match));
    SC_TEST_EXPECT(match.status == HttpRouteMatchStatus::Matched);
    SC_TEST_EXPECT(match.route == &routes[3]);
    SC_TEST_EXPECT(match.numParams == 2);
    SC_TEST_EXPECT(params[0].name == "id");
    SC_TEST_EXPECT(params[0].value == "42");
    SC_TEST_EXPECT(params[1].name == "book");
    SC_TEST_EXPECT(params[1].value == "abc");

    SC_TEST_EXPECT(router.match(HttpParser::Method::HttpDELETE, "/users/42/books/abc", {params, 1}, match));
    SC_TEST_EXPECT(match.status == HttpRouteMatchStatus::TooManyParams);

    SC_TEST_EXPECT(router.match(HttpParser::Method::HttpGET, "/missing", params, match));
    SC_TEST_EXPECT(match.status == HttpRouteMatchStatus::NotFound);

    SC_TEST_EXPECT(not router.formatAllowHeader("/users/42", {allowStorage, 3}, allow));
    SC_TEST_EXPECT(not router.match(HttpParser::Method::HttpGET, "http://example.com/users/42", params, match));
}

void HttpRouterTest::diagnosticMessages()
{
    const HttpRoute routes[] = {
        {HttpParser::Method::HttpGET, "/users/:id"},
        {HttpParser::Method::HttpPUT, "/users/:id"},
    };

    HttpRouter router;
    SC_TEST_EXPECT(router.init(routes));

    char       allowStorage[3];
    StringSpan allow;
    SC_TEST_EXPECT(resultMessageEquals(router.formatAllowHeader("/users/42", allowStorage, allow),
                                       "HttpRouter Allow output buffer is too small"));

    HttpRouteParam params[1];
    HttpRouteMatch match;
    SC_TEST_EXPECT(
        resultMessageEquals(router.match(HttpParser::Method::HttpGET, "http://example.com/users/42", params, match),
                            "HttpRequestTargetView only supports origin-form request targets"));
}

void runHttpRouterTest(SC::TestReport& report) { HttpRouterTest test(report); }
} // namespace SC
