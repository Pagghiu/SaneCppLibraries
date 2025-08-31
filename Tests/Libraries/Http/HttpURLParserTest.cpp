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
            testFull();
        }
        if (test_section("mini"))
        {
            testMini();
        }
        if (test_section("invalid"))
        {
            testInvalid();
        }
        if (test_section("caseInsensitive"))
        {
            testCaseInsensitive();
        }
        if (test_section("ipv6"))
        {
            testIPv6();
        }
        if (test_section("invalidPort"))
        {
            testInvalidPort();
        }
    }

    void testFull();
    void testMini();
    void testInvalid();
    void testCaseInsensitive();
    void testIPv6();
    void testInvalidPort();
};

void SC::HttpURLParserTest::testFull()
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

void SC::HttpURLParserTest::testMini()
{
    HttpURLParser urlParser;
    SC_TEST_EXPECT(urlParser.parse("http://site.com"));

    SC_TEST_EXPECT(urlParser.protocol == "http");
    SC_TEST_EXPECT(urlParser.username.isEmpty());
    SC_TEST_EXPECT(urlParser.password.isEmpty());
    SC_TEST_EXPECT(urlParser.hostname == "site.com");
    SC_TEST_EXPECT(urlParser.port == 80);
    SC_TEST_EXPECT(urlParser.host == "site.com");
    SC_TEST_EXPECT(urlParser.pathname == "/");
    SC_TEST_EXPECT(urlParser.path == "/");
    SC_TEST_EXPECT(urlParser.search.isEmpty());
    SC_TEST_EXPECT(urlParser.hash.isEmpty());
}

void SC::HttpURLParserTest::testInvalid()
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

void SC::HttpURLParserTest::testCaseInsensitive()
{
    HttpURLParser urlParser;
    SC_TEST_EXPECT(urlParser.parse("HTTP://site.com"));
    SC_TEST_EXPECT(urlParser.protocol == "HTTP");
    SC_TEST_EXPECT(urlParser.hostname == "site.com");
    SC_TEST_EXPECT(urlParser.port == 80);
    SC_TEST_EXPECT(urlParser.pathname == "/");
}

void SC::HttpURLParserTest::testIPv6()
{
    HttpURLParser urlParser;
    SC_TEST_EXPECT(urlParser.parse("http://[::1]/"));
    SC_TEST_EXPECT(urlParser.protocol == "http");
    SC_TEST_EXPECT(urlParser.hostname == "[::1]");
    SC_TEST_EXPECT(urlParser.port == 80);
    SC_TEST_EXPECT(urlParser.host == "[::1]");
    SC_TEST_EXPECT(urlParser.pathname == "/");
}

void SC::HttpURLParserTest::testInvalidPort()
{
    HttpURLParser urlParser;
    SC_TEST_EXPECT(not urlParser.parse("http://site.com:99999"));
    SC_TEST_EXPECT(not urlParser.parse("http://site.com:-1"));
}

namespace SC
{
void runHttpURLParserTest(SC::TestReport& report) { HttpURLParserTest test(report); }
} // namespace SC
