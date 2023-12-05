// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../JsonTokenizer.h"
#include "../../Containers/SmallVector.h"
#include "../../Strings/String.h"
#include "../../Testing/Testing.h"

namespace SC
{
struct JsonTokenizerTest;
}

struct SC::JsonTokenizerTest : public SC::TestCase
{
    using It = StringIteratorASCII;

    static constexpr bool testTokenizeObject(StringView text)
    {
        auto                   it = text.getIterator<It>();
        Json::Tokenizer::Token token;
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::ObjectStart);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::ObjectEnd);
        return true;
    }

    static constexpr bool testTokenizeObjectWithField(StringView text)
    {
        auto                   it = text.getIterator<It>();
        Json::Tokenizer::Token token;
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::ObjectStart);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::String);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::Colon);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::Number);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::ObjectEnd);
        return true;
    }
    static constexpr bool testTokenizeObjectWithTwoFields(StringView text)
    {
        auto                   it = text.getIterator<It>();
        Json::Tokenizer::Token token;
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::ObjectStart);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::String);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::Colon);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::Number);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::Comma);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::String);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::Colon);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));
        SC_TRY(token.getType() == Json::Tokenizer::Token::Number);
        SC_TRY(Json::Tokenizer::tokenizeNext(it, token));

        SC_TRY(token.getType() == Json::Tokenizer::Token::ObjectEnd);
        return true;
    }

    [[nodiscard]] static constexpr Json::Tokenizer::Token scanToken(StringView text)
    {
        auto                   it = text.getIterator<It>();
        Json::Tokenizer::Token token;
        (void)Json::Tokenizer::scanToken(it, token);
        return token;
    }

    JsonTokenizerTest(SC::TestReport& report) : TestCase(report, "JsonTokenizerTest")
    {
        if (test_section("scanToken"))
        {
            constexpr StringView asdString("\"ASD\"");

            static_assert(scanToken("").getType() == Json::Tokenizer::Token::Invalid, "Error");
            static_assert(scanToken(" ").getType() == Json::Tokenizer::Token::Invalid, "Error");
            static_assert(scanToken("true").getType() == Json::Tokenizer::Token::True, "Error");
            static_assert(scanToken("false").getType() == Json::Tokenizer::Token::False, "Error");
            static_assert(scanToken("null").getType() == Json::Tokenizer::Token::Null, "Error");
            static_assert(scanToken("{").getType() == Json::Tokenizer::Token::ObjectStart, "Error");
            static_assert(scanToken("}").getType() == Json::Tokenizer::Token::ObjectEnd, "Error");
            static_assert(scanToken("[").getType() == Json::Tokenizer::Token::ArrayStart, "Error");
            static_assert(scanToken("]").getType() == Json::Tokenizer::Token::ArrayEnd, "Error");
            static_assert(scanToken(":").getType() == Json::Tokenizer::Token::Colon, "Error");
            static_assert(scanToken(",").getType() == Json::Tokenizer::Token::Comma, "Error");
            static_assert(scanToken("\"").getType() == Json::Tokenizer::Token::Invalid, "Error");
            static_assert(scanToken("\"\"").getType() == Json::Tokenizer::Token::String, "Error");
            static_assert(scanToken("\"String\"").getType() == Json::Tokenizer::Token::String, "Error");
            static_assert(scanToken(asdString).getToken(asdString) == "ASD", "Error");
            static_assert(scanToken("\"ASD").getType() == Json::Tokenizer::Token::Invalid, "Error");
            static_assert(scanToken("\"ASD\"\"").getType() == Json::Tokenizer::Token::String, "Error");
            static_assert(scanToken("123").getType() == Json::Tokenizer::Token::Number, "Error");
            static_assert(scanToken("adsf").getType() == Json::Tokenizer::Token::Invalid,
                          "Error"); // numbers a not validated

            SC_TEST_EXPECT(scanToken("").getType() == Json::Tokenizer::Token::Invalid);
            SC_TEST_EXPECT(scanToken(" ").getType() == Json::Tokenizer::Token::Invalid);
            SC_TEST_EXPECT(scanToken("true").getType() == Json::Tokenizer::Token::True);
            SC_TEST_EXPECT(scanToken("false").getType() == Json::Tokenizer::Token::False);
            SC_TEST_EXPECT(scanToken("null").getType() == Json::Tokenizer::Token::Null);
            SC_TEST_EXPECT(scanToken("{").getType() == Json::Tokenizer::Token::ObjectStart);
            SC_TEST_EXPECT(scanToken("}").getType() == Json::Tokenizer::Token::ObjectEnd);
            SC_TEST_EXPECT(scanToken("[").getType() == Json::Tokenizer::Token::ArrayStart);
            SC_TEST_EXPECT(scanToken("]").getType() == Json::Tokenizer::Token::ArrayEnd);
            SC_TEST_EXPECT(scanToken(":").getType() == Json::Tokenizer::Token::Colon);
            SC_TEST_EXPECT(scanToken(",").getType() == Json::Tokenizer::Token::Comma);
            SC_TEST_EXPECT(scanToken("\"").getType() == Json::Tokenizer::Token::Invalid);
            SC_TEST_EXPECT(scanToken("\"\"").getType() == Json::Tokenizer::Token::String);
            SC_TEST_EXPECT(scanToken("\"String\"").getType() == Json::Tokenizer::Token::String);
            SC_TEST_EXPECT(scanToken(asdString).getToken(asdString) == "ASD");
            SC_TEST_EXPECT(scanToken("\"ASD").getType() == Json::Tokenizer::Token::Invalid);
            SC_TEST_EXPECT(scanToken("\"ASD\"\"").getType() == Json::Tokenizer::Token::String);
            SC_TEST_EXPECT(scanToken("123").getType() == Json::Tokenizer::Token::Number);
            SC_TEST_EXPECT(scanToken("adsf").getType() == Json::Tokenizer::Token::Invalid); // numbers a not validated
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
void runJsonTokenizerTest(SC::TestReport& report) { JsonTokenizerTest test(report); }
} // namespace SC
