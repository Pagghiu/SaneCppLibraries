// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Http/HttpURLParser.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"
namespace SC
{
struct HttpURLParserTest;
}

namespace
{
static bool resultMessageEquals(SC::Result result, SC::StringSpan expected)
{
    if (result or result.message == nullptr)
    {
        return false;
    }
    return SC::StringSpan::fromNullTerminated(result.message, SC::StringEncoding::Ascii) == expected;
}
} // namespace

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
        if (test_section("formUrlEncoded"))
        {
            testFormUrlEncoded();
        }
        if (test_section("requestTargetView"))
        {
            testRequestTargetView();
        }
        if (test_section("diagnostic messages"))
        {
            testDiagnosticMessages();
        }
        if (test_section("specialChars"))
        {
            testSpecialChars();
        }
        if (test_section("ipAddresses"))
        {
            testIPAddresses();
        }
        if (test_section("reuseClearsPreviousResult"))
        {
            testReuseClearsPreviousResult();
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
    void testFormUrlEncoded();
    void testRequestTargetView();
    void testDiagnosticMessages();
    void testSpecialChars();
    void testIPAddresses();
    void testReuseClearsPreviousResult();
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
    SC_TEST_EXPECT(not urlParser.parse("http://exa mple.com/path"));
    SC_TEST_EXPECT(not urlParser.parse("http://example.com/path\twith-tab"));
    SC_TEST_EXPECT(not urlParser.parse("http://example.com/path?bad query"));
    SC_TEST_EXPECT(not urlParser.parse("http://example.com/path#bad fragment"));
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
    SC_TEST_EXPECT(not urlParser.parse("http://site.com:/path"));
    SC_TEST_EXPECT(not urlParser.parse("http://[::1]:/path"));
    SC_TEST_EXPECT(not urlParser.parse("http://[::1]extra/path"));
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
    StringSpan    value;

    // Multiple query parameters
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?key1=value1&key2=value2&key3=value3"));
    SC_TEST_EXPECT(urlParser.search == "?key1=value1&key2=value2&key3=value3");
    SC_TEST_EXPECT(urlParser.getQueryValue("key2", value));
    SC_TEST_EXPECT(value == "value2");
    SC_TEST_EXPECT(HttpURLParser::getQueryValue("key1=value1&key2=value2", "key1", value));
    SC_TEST_EXPECT(value == "value1");

    // Query parameter with empty value
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?empty=&key=value"));
    SC_TEST_EXPECT(urlParser.search == "?empty=&key=value");
    SC_TEST_EXPECT(urlParser.getQueryValue("empty", value));
    SC_TEST_EXPECT(value.isEmpty());

    // Query parameter with no value
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?flag&key=value"));
    SC_TEST_EXPECT(urlParser.search == "?flag&key=value");
    SC_TEST_EXPECT(urlParser.getQueryValue("flag", value));
    SC_TEST_EXPECT(value.isEmpty());

    // Only query parameters, no path
    SC_TEST_EXPECT(urlParser.parse("http://example.com?query=value"));
    SC_TEST_EXPECT(urlParser.pathname == "/");
    SC_TEST_EXPECT(urlParser.search == "?query=value");

    // Query parameters with special characters
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?q=hello%20world&special=%2B%2D"));
    SC_TEST_EXPECT(urlParser.search == "?q=hello%20world&special=%2B%2D");
    SC_TEST_EXPECT(urlParser.getQueryValue("q", value));
    SC_TEST_EXPECT(value == "hello%20world");
    SC_TEST_EXPECT(urlParser.getQueryValue("special", value));
    SC_TEST_EXPECT(value == "%2B%2D");

    // First repeated key wins
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?name=first&name=second"));
    SC_TEST_EXPECT(urlParser.getQueryValue("name", value));
    SC_TEST_EXPECT(value == "first");

    // Missing and empty search
    SC_TEST_EXPECT(not urlParser.getQueryValue("missing", value));
    SC_TEST_EXPECT(value.isEmpty());
    SC_TEST_EXPECT(not HttpURLParser::getQueryValue("", "missing", value));
    SC_TEST_EXPECT(not HttpURLParser::getQueryValue("?", "missing", value));

    // UTF8 values stay raw and zero-copy.
    SC_TEST_EXPECT(urlParser.parse("http://example.com/path?q=tëst&vâlue=ok"_u8));
    SC_TEST_EXPECT(urlParser.getQueryValue("q", value));
    SC_TEST_EXPECT(value == "tëst"_u8);
    SC_TEST_EXPECT(urlParser.getQueryValue("vâlue"_u8, value));
    SC_TEST_EXPECT(value == "ok");

    HttpURLQueryIterator iterator("?flag&empty=&key=value");
    HttpURLQueryItem     item;
    SC_TEST_EXPECT(iterator.next(item));
    SC_TEST_EXPECT(item.name == "flag");
    SC_TEST_EXPECT(item.value.isEmpty());
    SC_TEST_EXPECT(not item.hasValue);
    SC_TEST_EXPECT(iterator.next(item));
    SC_TEST_EXPECT(item.name == "empty");
    SC_TEST_EXPECT(item.value.isEmpty());
    SC_TEST_EXPECT(item.hasValue);
    SC_TEST_EXPECT(iterator.next(item));
    SC_TEST_EXPECT(item.name == "key");
    SC_TEST_EXPECT(item.value == "value");
    SC_TEST_EXPECT(item.hasValue);
    SC_TEST_EXPECT(not iterator.next(item));
}

void SC::HttpURLParserTest::testFormUrlEncoded()
{
    HttpFormUrlEncodedIterator iterator("name=Sane+Cpp&empty=&flag&encoded=%7Bok%7D&bad=%ZZ");
    HttpURLQueryItem           item;
    char                       storage[64];
    StringSpan                 decoded;

    SC_TEST_EXPECT(iterator.next(item));
    SC_TEST_EXPECT(item.name == "name");
    SC_TEST_EXPECT(item.value == "Sane+Cpp");
    SC_TEST_EXPECT(item.hasValue);
    SC_TEST_EXPECT(HttpFormUrlDecode(item.value, storage, decoded));
    SC_TEST_EXPECT(decoded == "Sane Cpp");

    SC_TEST_EXPECT(iterator.next(item));
    SC_TEST_EXPECT(item.name == "empty");
    SC_TEST_EXPECT(item.value.isEmpty());
    SC_TEST_EXPECT(item.hasValue);

    SC_TEST_EXPECT(iterator.next(item));
    SC_TEST_EXPECT(item.name == "flag");
    SC_TEST_EXPECT(item.value.isEmpty());
    SC_TEST_EXPECT(not item.hasValue);

    SC_TEST_EXPECT(iterator.next(item));
    SC_TEST_EXPECT(item.name == "encoded");
    SC_TEST_EXPECT(HttpPercentDecode(item.value, storage, decoded));
    SC_TEST_EXPECT(decoded == "{ok}");

    SC_TEST_EXPECT(iterator.next(item));
    SC_TEST_EXPECT(item.name == "bad");
    SC_TEST_EXPECT(not HttpPercentDecode(item.value, storage, decoded));
    SC_TEST_EXPECT(not HttpFormUrlDecode("%2", storage, decoded));
    SC_TEST_EXPECT(not HttpFormUrlDecode("abc", {storage, 2}, decoded));

    SC_TEST_EXPECT(not iterator.next(item));

    StringSpan value;
    SC_TEST_EXPECT(HttpFormUrlEncodedIterator::getValue("name=first&name=second", "name", value));
    SC_TEST_EXPECT(value == "first");
    SC_TEST_EXPECT(HttpFormUrlEncodedIterator::getValue("empty=&flag&encoded=%7Bok%7D", "empty", value));
    SC_TEST_EXPECT(value.isEmpty());
    SC_TEST_EXPECT(HttpFormUrlEncodedIterator::getValue("empty=&flag&encoded=%7Bok%7D", "flag", value));
    SC_TEST_EXPECT(value.isEmpty());
    SC_TEST_EXPECT(HttpFormUrlEncodedIterator::getValue("empty=&flag&encoded=%7Bok%7D", "encoded", value));
    SC_TEST_EXPECT(value == "%7Bok%7D");
    SC_TEST_EXPECT(HttpFormUrlEncodedIterator::getValue("vâlue=tëst"_u8, "vâlue"_u8, value));
    SC_TEST_EXPECT(value == "tëst"_u8);
    SC_TEST_EXPECT(not HttpFormUrlEncodedIterator::getValue("", "missing", value));
    SC_TEST_EXPECT(value.isEmpty());
}

void SC::HttpURLParserTest::testRequestTargetView()
{
    HttpRequestTargetView target;
    StringSpan            value;

    SC_TEST_EXPECT(target.parse("/api/users?name=first&empty=&flag&name=second#section"));
    SC_TEST_EXPECT(target.raw == "/api/users?name=first&empty=&flag&name=second#section");
    SC_TEST_EXPECT(target.path == "/api/users");
    SC_TEST_EXPECT(target.search == "?name=first&empty=&flag&name=second");
    SC_TEST_EXPECT(target.hash == "#section");
    SC_TEST_EXPECT(target.getQueryValue("name", value));
    SC_TEST_EXPECT(value == "first");
    SC_TEST_EXPECT(target.getQueryValue("empty", value));
    SC_TEST_EXPECT(value.isEmpty());
    SC_TEST_EXPECT(target.getQueryValue("flag", value));
    SC_TEST_EXPECT(value.isEmpty());

    SC_TEST_EXPECT(target.parse("/only-path"));
    SC_TEST_EXPECT(target.path == "/only-path");
    SC_TEST_EXPECT(target.search.isEmpty());
    SC_TEST_EXPECT(target.hash.isEmpty());

    SC_TEST_EXPECT(target.parse("/empty-query?"));
    SC_TEST_EXPECT(target.path == "/empty-query");
    SC_TEST_EXPECT(target.search == "?");
    SC_TEST_EXPECT(not target.getQueryValue("missing", value));

    SC_TEST_EXPECT(target.parse("/fragment#only"));
    SC_TEST_EXPECT(target.path == "/fragment");
    SC_TEST_EXPECT(target.search.isEmpty());
    SC_TEST_EXPECT(target.hash == "#only");

    SC_TEST_EXPECT(target.parse("*"));
    SC_TEST_EXPECT(target.path == "*");
    SC_TEST_EXPECT(target.search.isEmpty());

    SC_TEST_EXPECT(not target.parse(""));
    SC_TEST_EXPECT(not target.parse("http://example.com/path"));
    SC_TEST_EXPECT(not target.parse("relative/path"));
    SC_TEST_EXPECT(not target.parse("/bad path"));
    SC_TEST_EXPECT(not target.parse("/bad\tpath"));
}

void SC::HttpURLParserTest::testDiagnosticMessages()
{
    HttpRequestTargetView target;

    SC_TEST_EXPECT(resultMessageEquals(target.parse(""), "HttpRequestTargetView empty request target"));
    SC_TEST_EXPECT(resultMessageEquals(target.parse("/bad path"),
                                       "HttpRequestTargetView request target contains invalid whitespace"));
    SC_TEST_EXPECT(resultMessageEquals(target.parse("relative/path"),
                                       "HttpRequestTargetView only supports origin-form request targets"));

    char       storage[4];
    StringSpan decoded;
    SC_TEST_EXPECT(resultMessageEquals(HttpPercentDecode("%2", storage, decoded), "Malformed percent escape"));
    SC_TEST_EXPECT(
        resultMessageEquals(HttpPercentDecode("abcdef", {storage, 2}, decoded), "Decoded output buffer is too small"));
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

void SC::HttpURLParserTest::testReuseClearsPreviousResult()
{
    HttpURLParser urlParser;
    SC_TEST_EXPECT(urlParser.parse("https://user:pass@example.com:8443/path?q=1#frag"));
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

    SC_TEST_EXPECT(urlParser.parse("http://site.com?fresh=1"));
    SC_TEST_EXPECT(urlParser.path == "/");
    SC_TEST_EXPECT(urlParser.pathname == "/");
    SC_TEST_EXPECT(urlParser.search == "?fresh=1");
    SC_TEST_EXPECT(urlParser.hash.isEmpty());
}

namespace SC
{
void runHttpURLParserTest(SC::TestReport& report) { HttpURLParserTest test(report); }
} // namespace SC
