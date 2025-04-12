// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/SerializationText/Internal/JsonTokenizer.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct JsonTokenizerTest;
}

struct SC::JsonTokenizerTest : public SC::TestCase
{
    using It = StringIteratorASCII;

    static constexpr bool testTokenizeObject(StringView text)
    {
        auto                 it = text.getIterator<It>();
        JsonTokenizer::Token token;
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::ObjectStart);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::ObjectEnd);
        return true;
    }

    static constexpr bool testTokenizeObjectWithField(StringView text)
    {
        auto                 it = text.getIterator<It>();
        JsonTokenizer::Token token;
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::ObjectStart);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::String);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Colon);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Number);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::ObjectEnd);
        return true;
    }
    static constexpr bool testTokenizeObjectWithTwoFields(StringView text)
    {
        auto                 it = text.getIterator<It>();
        JsonTokenizer::Token token;
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::ObjectStart);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::String);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Colon);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Number);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Comma);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::String);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Colon);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == JsonTokenizer::Token::Number);
        SC_TRY(JsonTokenizer::tokenizeNext(it, token));

        SC_TRY(token.getType() == JsonTokenizer::Token::ObjectEnd);
        return true;
    }

    [[nodiscard]] static constexpr JsonTokenizer::Token scanToken(StringView text)
    {
        auto                 it = text.getIterator<It>();
        JsonTokenizer::Token token;
        (void)JsonTokenizer::scanToken(it, token);
        return token;
    }

    JsonTokenizerTest(SC::TestReport& report) : TestCase(report, "JsonTokenizerTest")
    {
        if (test_section("scanToken"))
        {
            constexpr StringView asdString("\"ASD\"");

            static_assert(scanToken("").getType() == JsonTokenizer::Token::Invalid, "Error");
            static_assert(scanToken(" ").getType() == JsonTokenizer::Token::Invalid, "Error");
            static_assert(scanToken("true").getType() == JsonTokenizer::Token::True, "Error");
            static_assert(scanToken("false").getType() == JsonTokenizer::Token::False, "Error");
            static_assert(scanToken("null").getType() == JsonTokenizer::Token::Null, "Error");
            static_assert(scanToken("{").getType() == JsonTokenizer::Token::ObjectStart, "Error");
            static_assert(scanToken("}").getType() == JsonTokenizer::Token::ObjectEnd, "Error");
            static_assert(scanToken("[").getType() == JsonTokenizer::Token::ArrayStart, "Error");
            static_assert(scanToken("]").getType() == JsonTokenizer::Token::ArrayEnd, "Error");
            static_assert(scanToken(":").getType() == JsonTokenizer::Token::Colon, "Error");
            static_assert(scanToken(",").getType() == JsonTokenizer::Token::Comma, "Error");
            static_assert(scanToken("\"").getType() == JsonTokenizer::Token::Invalid, "Error");
            static_assert(scanToken("\"\"").getType() == JsonTokenizer::Token::String, "Error");
            static_assert(scanToken("\"String\"").getType() == JsonTokenizer::Token::String, "Error");
            static_assert(scanToken(asdString).getToken(asdString) == "ASD", "Error");
            static_assert(scanToken("\"ASD").getType() == JsonTokenizer::Token::Invalid, "Error");
            static_assert(scanToken("\"ASD\"\"").getType() == JsonTokenizer::Token::String, "Error");
            static_assert(scanToken("123").getType() == JsonTokenizer::Token::Number, "Error");
            static_assert(scanToken("adsf").getType() == JsonTokenizer::Token::Invalid,
                          "Error"); // numbers a not validated

            SC_TEST_EXPECT(scanToken("").getType() == JsonTokenizer::Token::Invalid);
            SC_TEST_EXPECT(scanToken(" ").getType() == JsonTokenizer::Token::Invalid);
            SC_TEST_EXPECT(scanToken("true").getType() == JsonTokenizer::Token::True);
            SC_TEST_EXPECT(scanToken("false").getType() == JsonTokenizer::Token::False);
            SC_TEST_EXPECT(scanToken("null").getType() == JsonTokenizer::Token::Null);
            SC_TEST_EXPECT(scanToken("{").getType() == JsonTokenizer::Token::ObjectStart);
            SC_TEST_EXPECT(scanToken("}").getType() == JsonTokenizer::Token::ObjectEnd);
            SC_TEST_EXPECT(scanToken("[").getType() == JsonTokenizer::Token::ArrayStart);
            SC_TEST_EXPECT(scanToken("]").getType() == JsonTokenizer::Token::ArrayEnd);
            SC_TEST_EXPECT(scanToken(":").getType() == JsonTokenizer::Token::Colon);
            SC_TEST_EXPECT(scanToken(",").getType() == JsonTokenizer::Token::Comma);
            SC_TEST_EXPECT(scanToken("\"").getType() == JsonTokenizer::Token::Invalid);
            SC_TEST_EXPECT(scanToken("\"\"").getType() == JsonTokenizer::Token::String);
            SC_TEST_EXPECT(scanToken("\"String\"").getType() == JsonTokenizer::Token::String);
            SC_TEST_EXPECT(scanToken(asdString).getToken(asdString) == "ASD");
            SC_TEST_EXPECT(scanToken("\"ASD").getType() == JsonTokenizer::Token::Invalid);
            SC_TEST_EXPECT(scanToken("\"ASD\"\"").getType() == JsonTokenizer::Token::String);
            SC_TEST_EXPECT(scanToken("123").getType() == JsonTokenizer::Token::Number);
            SC_TEST_EXPECT(scanToken("adsf").getType() == JsonTokenizer::Token::Invalid); // numbers a not validated
        }
        if (test_section("tokenizeObject"))
        {
            static_assert(testTokenizeObject("{}"), "Invalid");
            static_assert(testTokenizeObject(" { \n\t} "), "Invalid");
            static_assert(not testTokenizeObject(" {_} "), "Invalid");
            static_assert(testTokenizeObjectWithField("{  \"x\"\t   :   \t1.2\t  }"), "Invalid");
            static_assert(testTokenizeObjectWithTwoFields("{\"x\":1,\"y\":2}"), "Invalid");
            SC_TEST_EXPECT(testTokenizeObject("{}"));
            SC_TEST_EXPECT(testTokenizeObject(" { \n\t} "));
            SC_TEST_EXPECT(not testTokenizeObject(" {_} "));
            SC_TEST_EXPECT(testTokenizeObjectWithField("{  \"x\"\t   :   \t1.2\t  }"));
            SC_TEST_EXPECT(testTokenizeObjectWithTwoFields("{\"x\":1,\"y\":2}"));
        }
    }
};

namespace SC
{
void runSerializationJsonTokenizerTest(SC::TestReport& report) { JsonTokenizerTest test(report); }
} // namespace SC
