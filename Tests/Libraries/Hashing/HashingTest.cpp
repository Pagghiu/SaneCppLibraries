// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Hashing/Hashing.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct HashingTest;
}
extern "C" const char* sc_hashing_test();
struct SC::HashingTest : public SC::TestCase
{
    HashingTest(SC::TestReport& report) : TestCase(report, "HashingTest")
    {
        using namespace SC;
        if (test_section("MD5"))
        {
            //! [HashingSnippet]
            Hashing hash;
            SC_TEST_EXPECT(hash.setType(Hashing::TypeMD5));

            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            Hashing::Result res;
            SC_TEST_EXPECT(hash.getHash(res));

            String test;
            SC_TEST_EXPECT(StringBuilder(test).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
            SC_TEST_EXPECT(test == "098F6BCD4621D373CADE4E832627B4F6"_a8);
            report.console.printLine(test.view());
            //! [HashingSnippet]
        }

        if (test_section("MD5 Update"))
        {
            //! [HashingUpdateSnippet]
            Hashing hash;
            SC_TEST_EXPECT(hash.setType(Hashing::TypeMD5));

            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            Hashing::Result res;
            SC_TEST_EXPECT(hash.getHash(res));

            String test;
            SC_TEST_EXPECT(StringBuilder(test).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
            SC_TEST_EXPECT(test == "05A671C66AEFEA124CC08B76EA6D30BB"_a8);
            //! [HashingUpdateSnippet]
        }

        if (test_section("SHA1"))
        {
            Hashing hash;
            SC_TEST_EXPECT(hash.setType(Hashing::TypeSHA1));

            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            Hashing::Result res;
            SC_TEST_EXPECT(hash.getHash(res));

            String test;
            SC_TEST_EXPECT(StringBuilder(test).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
            SC_TEST_EXPECT(test == "A94A8FE5CCB19BA61C4C0873D391E987982FBBD3"_a8);
            report.console.printLine(test.view());
        }

        if (test_section("SHA1 Update"))
        {
            Hashing hash;
            SC_TEST_EXPECT(hash.setType(Hashing::TypeSHA1));

            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            Hashing::Result res;
            SC_TEST_EXPECT(hash.getHash(res));

            String test;
            SC_TEST_EXPECT(StringBuilder(test).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
            SC_TEST_EXPECT(test == "51ABB9636078DEFBF888D8457A7C76F85C8F114C"_a8);
        }

        if (test_section("SHA256"))
        {
            Hashing hash;
            SC_TEST_EXPECT(hash.setType(Hashing::TypeSHA256));

            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            Hashing::Result res;
            SC_TEST_EXPECT(hash.getHash(res));

            String test;
            SC_TEST_EXPECT(StringBuilder(test).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
            SC_TEST_EXPECT(test == "9F86D081884C7D659A2FEAA0C55AD015A3BF4F1B2B0B822CD15D6C15B0F00A08"_a8);
            report.console.printLine(test.view());
        }

        if (test_section("SHA256 Update"))
        {
            Hashing hash;
            SC_TEST_EXPECT(hash.setType(Hashing::TypeSHA256));

            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            SC_TEST_EXPECT(hash.add("test"_a8.toBytesSpan()));
            Hashing::Result res;
            SC_TEST_EXPECT(hash.getHash(res));

            String test;
            SC_TEST_EXPECT(StringBuilder(test).appendHex(res.toBytesSpan(), StringBuilder::AppendHexCase::UpperCase));
            SC_TEST_EXPECT(test == "37268335DD6931045BDCDF92623FF819A64244B53D0E746D438797349D4DA578"_a8);
        }
        if (test_section("C Bindings"))
        {
            const char* res = sc_hashing_test();
            recordExpectation(res ? StringView::fromNullTerminated(res, StringEncoding::Utf8) : "Hashing",
                              res == nullptr);
        }
    }
};

namespace SC
{
void runHashingTest(SC::TestReport& report) { HashingTest test(report); }
} // namespace SC
