// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/SmallVector.h"
#include "../Foundation/String.h"
#include "../Testing/Test.h"
#include "JsonTokenizer.h"

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
        SC_TRY_IF(JsonTokenizer::tokenizeNext(it, token, 0));
        SC_TRY_IF(token.type == JsonTokenizer::Token::ObjectStart);
        SC_TRY_IF(JsonTokenizer::tokenizeNext(it, token, 0));
        SC_TRY_IF(token.type == JsonTokenizer::Token::ObjectEnd);
        return true;
    }

    [[nodiscard]] static constexpr JsonTokenizer::Token scanToken(StringView text)
    {
        auto                 it = text.getIterator<It>();
        JsonTokenizer::Token token;
        SC_TRUST_RESULT(JsonTokenizer::scanToken(it, token));
        return token;
    }

    JsonTokenizerTest(SC::TestReport& report) : TestCase(report, "JsonTokenizerTest")
    {
        if (test_section("scanToken"))
        {
            static_assert(scanToken("").type == JsonTokenizer::Token::Invalid, "Error");
            static_assert(scanToken(" ").type == JsonTokenizer::Token::Invalid, "Error");
            static_assert(scanToken("true").type == JsonTokenizer::Token::True, "Error");
            static_assert(scanToken("false").type == JsonTokenizer::Token::False, "Error");
            static_assert(scanToken("null").type == JsonTokenizer::Token::Null, "Error");
            static_assert(scanToken("{").type == JsonTokenizer::Token::ObjectStart, "Error");
            static_assert(scanToken("}").type == JsonTokenizer::Token::ObjectEnd, "Error");
            static_assert(scanToken("[").type == JsonTokenizer::Token::ArrayStart, "Error");
            static_assert(scanToken("]").type == JsonTokenizer::Token::ArrayEnd, "Error");
            static_assert(scanToken(":").type == JsonTokenizer::Token::Colon, "Error");
            static_assert(scanToken(",").type == JsonTokenizer::Token::Comma, "Error");
            static_assert(scanToken("\"").type == JsonTokenizer::Token::Invalid, "Error");
            static_assert(scanToken("\"\"").type == JsonTokenizer::Token::String, "Error");
            static_assert(scanToken("\"String\"").type == JsonTokenizer::Token::String, "Error");
            constexpr StringView asdString("\"ASD\"");
            static_assert(scanToken(asdString).getToken(asdString) == "ASD"_a8, "Error");
            static_assert(scanToken("\"ASD").type == JsonTokenizer::Token::Invalid, "Error");
            static_assert(scanToken("\"ASD\"\"").type == JsonTokenizer::Token::String, "Error");
            static_assert(scanToken("123").type == JsonTokenizer::Token::Number, "Error");
            static_assert(scanToken("adsf").type == JsonTokenizer::Token::Number, "Error"); // numbers a not validated
        }
        if (test_section("tokenizeObject"))
        {
            static_assert(testTokenizeObject("{}"_a8), "Invalid");
            static_assert(testTokenizeObject(" { \n\t} "_a8), "Invalid");
            static_assert(not testTokenizeObject(" {_} "_a8), "Invalid");
        }
    }
};
