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
        if (test_section("utf8"))
        {
            testUTF8();
        }
    }

    void testFull();
    void testMini();
    void testInvalid();
    void testCaseInsensitive();
    void testIPv6();
    void testInvalidPort();
    void testUTF8();
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

void SC::HttpURLParserTest::testUTF8()
{
    HttpURLParser urlParser;

    // Test UTF8 in hostname
    SC_TEST_EXPECT(urlParser.parse("http://tëst.com/path"_u8));
    SC_TEST_EXPECT(urlParser.protocol == "http");
    SC_TEST_EXPECT(urlParser.hostname == "tëst.com"_u8);
    SC_TEST_EXPECT(urlParser.pathname == "/path");

    // Test UTF8 in path
    SC_TEST_EXPECT(urlParser.parse("http://example.com/pâth/tëst"_u8));
    SC_TEST_EXPECT(urlParser.protocol == "http");
    SC_TEST_EXPECT(urlParser.hostname == "example.com");
    SC_TEST_EXPECT(urlParser.pathname == "/pâth/tëst"_u8);

    // Test UTF8 in query parameters
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?q=tëst&vâlue"_u8));
    SC_TEST_EXPECT(urlParser.protocol == "http");
    SC_TEST_EXPECT(urlParser.hostname == "example.com");
    SC_TEST_EXPECT(urlParser.pathname == "/path");
    SC_TEST_EXPECT(urlParser.search == "?q=tëst&vâlue"_u8);

    // Test UTF8 in fragment
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path#frâgment"_u8));
    SC_TEST_EXPECT(urlParser.protocol == "http");
    SC_TEST_EXPECT(urlParser.hostname == "example.com");
    SC_TEST_EXPECT(urlParser.pathname == "/path");
    SC_TEST_EXPECT(urlParser.hash == "#frâgment"_u8);

    // Test UTF8 in username/password
    SC_TEST_EXPECT(urlParser.parse("http://ûser:pâss@example.com/path"_u8));
    SC_TEST_EXPECT(urlParser.protocol == "http");
    SC_TEST_EXPECT(urlParser.username == "ûser"_u8);
    SC_TEST_EXPECT(urlParser.password == "pâss"_u8);
    SC_TEST_EXPECT(urlParser.hostname == "example.com");
    SC_TEST_EXPECT(urlParser.pathname == "/path");

    // Test mixed UTF8 and ASCII
    SC_TEST_EXPECT(urlParser.parse("http://tëst.com:8080/pâth?q=âsk#frâg"_u8));
    SC_TEST_EXPECT(urlParser.protocol == "http");
    SC_TEST_EXPECT(urlParser.hostname == "tëst.com"_u8);
    SC_TEST_EXPECT(urlParser.port == 8080);
    SC_TEST_EXPECT(urlParser.pathname == "/pâth"_u8);
    SC_TEST_EXPECT(urlParser.search == "?q=âsk"_u8);
    SC_TEST_EXPECT(urlParser.hash == "#frâg"_u8);
}

namespace SC
{
void runHttpURLParserTest(SC::TestReport& report) { HttpURLParserTest test(report); }
} // namespace SC
