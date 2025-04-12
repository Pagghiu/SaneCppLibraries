// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpURLParser.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct HttpURLParserTest;
}

struct SC::HttpURLParserTest : public SC::TestCase
{
    HttpURLParserTest(SC::TestReport& report) : TestCase(report, "HttpURLParserTest")
    {
        if (test_section("full"))
        {
            HttpURLParser urlParser;
            SC_TEST_EXPECT(urlParser.parse("http://user:pass@site.com:80/pa/th?q=val#hash"));

            SC_TEST_EXPECT(urlParser.protocol == "http");
            SC_TEST_EXPECT(urlParser.username == "user");
            SC_TEST_EXPECT(urlParser.password == "pass");
            SC_TEST_EXPECT(urlParser.hostname == "site.com");
            SC_TEST_EXPECT(urlParser.port == 80);
            SC_TEST_EXPECT(urlParser.host == "site.com:80");
            SC_TEST_EXPECT(urlParser.pathname == "/pa/th");
            SC_TEST_EXPECT(urlParser.path == "/pa/th?q=val");
            SC_TEST_EXPECT(urlParser.search == "?q=val");
            SC_TEST_EXPECT(urlParser.hash == "#hash");
        }
        if (test_section("mini"))
        {
            HttpURLParser urlParser;
            SC_TEST_EXPECT(urlParser.parse("http://site.com"));

            SC_TEST_EXPECT(urlParser.protocol == "http");
            SC_TEST_EXPECT(urlParser.username.isEmpty());
            SC_TEST_EXPECT(urlParser.password.isEmpty());
            SC_TEST_EXPECT(urlParser.hostname == "site.com");
            SC_TEST_EXPECT(urlParser.port == 80);
            SC_TEST_EXPECT(urlParser.host == "site.com");
            SC_TEST_EXPECT(urlParser.pathname.isEmpty());
            SC_TEST_EXPECT(urlParser.path.isEmpty());
            SC_TEST_EXPECT(urlParser.search.isEmpty());
            SC_TEST_EXPECT(urlParser.hash.isEmpty());
        }
        if (test_section("invalid"))
        {
            HttpURLParser urlParser;
            SC_TEST_EXPECT(not urlParser.parse("http:/site.com"));           // missing double //
            SC_TEST_EXPECT(not urlParser.parse("http.//site.com"));          // wrong . instead of :
            SC_TEST_EXPECT(not urlParser.parse("http//site.com"));           // missing :
            SC_TEST_EXPECT(not urlParser.parse("http://"));                  // missing host
            SC_TEST_EXPECT(not urlParser.parse("http://a"));                 // no dot
            SC_TEST_EXPECT(not urlParser.parse("http://site.com/asd dsa/")); // no space in paths
            SC_TEST_EXPECT(not urlParser.parse("hppt://site.com"));          // unknown protocol
        }
    }
};

namespace SC
{
void runHttpURLParserTest(SC::TestReport& report) { HttpURLParserTest test(report); }
} // namespace SC
