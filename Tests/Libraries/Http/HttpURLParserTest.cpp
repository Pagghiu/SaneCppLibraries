// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpURLParser.h"
#include "Libraries/Strings/StringView.h"
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
        if (test_section("edgeCases"))
        {
            testEdgeCases();
        }
        if (test_section("protocols"))
        {
            testProtocols();
        }
        if (test_section("queryParams"))
        {
            testQueryParams();
        }
        if (test_section("specialChars"))
        {
            testSpecialChars();
        }
        if (test_section("ipAddresses"))
        {
            testIPAddresses();
        }
    }

    void testFull();
    void testMini();
    void testInvalid();
    void testCaseInsensitive();
    void testIPv6();
    void testInvalidPort();
    void testUTF8();
    void testEdgeCases();
    void testProtocols();
    void testQueryParams();
    void testSpecialChars();
    void testIPAddresses();
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

void SC::HttpURLParserTest::testEdgeCases()
{
    HttpURLParser urlParser;

    // Username only (no password)
    SC_TEST_EXPECT(urlParser.parse("http://user@example.com/path"));
    SC_TEST_EXPECT(urlParser.username == "user");
    SC_TEST_EXPECT(urlParser.password.isEmpty());
    SC_TEST_EXPECT(urlParser.hostname == "example.com");

    // Root path only
    SC_TEST_EXPECT(urlParser.parse("http://example.com/"));
    SC_TEST_EXPECT(urlParser.pathname == "/");
    SC_TEST_EXPECT(urlParser.path == "/");

    // No path at all
    SC_TEST_EXPECT(urlParser.parse("http://example.com"));
    SC_TEST_EXPECT(urlParser.pathname == "/");
    SC_TEST_EXPECT(urlParser.path == "/");

    // Port 0
    SC_TEST_EXPECT(urlParser.parse("http://example.com:0/path"));
    SC_TEST_EXPECT(urlParser.port == 0);

    // Very long path
    SC_TEST_EXPECT(urlParser.parse("http://example.com/very/long/path/with/many/segments"));
    SC_TEST_EXPECT(urlParser.pathname == "/very/long/path/with/many/segments");

    // Path with dots
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path/./subpath/../other"));
    SC_TEST_EXPECT(urlParser.pathname == "/path/./subpath/../other");
}

void SC::HttpURLParserTest::testProtocols()
{
    HttpURLParser urlParser;

    // HTTPS protocol
    SC_TEST_EXPECT(urlParser.parse("https://example.com"));
    SC_TEST_EXPECT(urlParser.protocol == "https");
    SC_TEST_EXPECT(urlParser.port == 443);

    // Mixed case protocol
    SC_TEST_EXPECT(urlParser.parse("Https://example.com"));
    SC_TEST_EXPECT(urlParser.protocol == "Https");

    // Invalid protocol
    SC_TEST_EXPECT(not urlParser.parse("ftp://example.com"));
    SC_TEST_EXPECT(not urlParser.parse("custom://example.com"));
}

void SC::HttpURLParserTest::testQueryParams()
{
    HttpURLParser urlParser;

    // Multiple query parameters
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?key1=value1&key2=value2&key3=value3"));
    SC_TEST_EXPECT(urlParser.search == "?key1=value1&key2=value2&key3=value3");

    // Query parameter with empty value
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?empty=&key=value"));
    SC_TEST_EXPECT(urlParser.search == "?empty=&key=value");

    // Query parameter with no value
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?flag&key=value"));
    SC_TEST_EXPECT(urlParser.search == "?flag&key=value");

    // Only query parameters, no path
    SC_TEST_EXPECT(urlParser.parse("http://example.com?query=value"));
    SC_TEST_EXPECT(urlParser.pathname == "/");
    SC_TEST_EXPECT(urlParser.search == "?query=value");

    // Query parameters with special characters
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?q=hello%20world&special=%2B%2D"));
    SC_TEST_EXPECT(urlParser.search == "?q=hello%20world&special=%2B%2D");
}

void SC::HttpURLParserTest::testSpecialChars()
{
    HttpURLParser urlParser;

    // Path with allowed special characters
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path_with_underscores-and-dashes"));
    SC_TEST_EXPECT(urlParser.pathname == "/path_with_underscores-and-dashes");

    // Path with numbers
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path123/456"));
    SC_TEST_EXPECT(urlParser.pathname == "/path123/456");

    // Hostname with numbers
    SC_TEST_EXPECT(urlParser.parse("http://site123.com/path"));
    SC_TEST_EXPECT(urlParser.hostname == "site123.com");

    // Username with special characters
    SC_TEST_EXPECT(urlParser.parse("http://user_name@example.com/path"));
    SC_TEST_EXPECT(urlParser.username == "user_name");

    // Invalid: space in path (should fail)
    SC_TEST_EXPECT(not urlParser.parse("http://example.com/path with space"));
}

void SC::HttpURLParserTest::testIPAddresses()
{
    HttpURLParser urlParser;

    // IPv4 address
    SC_TEST_EXPECT(urlParser.parse("http://192.168.1.1/path"));
    SC_TEST_EXPECT(urlParser.hostname == "192.168.1.1");
    SC_TEST_EXPECT(urlParser.port == 80);

    // IPv4 with port
    SC_TEST_EXPECT(urlParser.parse("http://192.168.1.1:8080/path"));
    SC_TEST_EXPECT(urlParser.hostname == "192.168.1.1");
    SC_TEST_EXPECT(urlParser.port == 8080);

    // IPv6 localhost
    SC_TEST_EXPECT(urlParser.parse("http://[::1]/path"));
    SC_TEST_EXPECT(urlParser.hostname == "[::1]");

    // IPv6 with port
    SC_TEST_EXPECT(urlParser.parse("http://[::1]:8080/path"));
    SC_TEST_EXPECT(urlParser.hostname == "[::1]");
    SC_TEST_EXPECT(urlParser.port == 8080);

    // IPv6 full address
    SC_TEST_EXPECT(urlParser.parse("http://[2001:db8::1]/path"));
    SC_TEST_EXPECT(urlParser.hostname == "[2001:db8::1]");

    // IPv6 compressed
    SC_TEST_EXPECT(urlParser.parse("http://[2001:db8::]/path"));
    SC_TEST_EXPECT(urlParser.hostname == "[2001:db8::]");
}

namespace SC
{
void runHttpURLParserTest(SC::TestReport& report) { HttpURLParserTest test(report); }
} // namespace SC
