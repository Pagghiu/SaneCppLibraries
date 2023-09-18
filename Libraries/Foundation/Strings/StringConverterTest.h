// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Testing/Test.h"
#include "../Containers/SmallVector.h"
#include "StringConverter.h"

namespace SC
{
struct StringConverterTest;
}

struct SC::StringConverterTest : public SC::TestCase
{
    StringConverterTest(SC::TestReport& report) : TestCase(report, "StringConverterTest")
    {
        using namespace SC;
        if (test_section("UTF8<->UTF16"))
        {
            const char utf8String1[]  = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"; // "日本語" int UTF-8
            const char utf16String1[] = "\xE5\x65\x2C\x67\x9E\x8a";             // "日本語" in UTF-16LE

            SmallVector<char, 255> buffer;

            StringView input, output, expected;

            input    = StringView(utf8String1, sizeof(utf8String1) - 1, false, StringEncoding::Utf8);
            expected = StringView(utf16String1, sizeof(utf16String1) - 1, false, StringEncoding::Utf16);
            buffer.clear();
            SC_TEST_EXPECT(
                StringConverter::convertEncodingToUTF16(input, buffer, &output, StringConverter::AddZeroTerminator));
            SC_TEST_EXPECT(output == expected);

            input    = StringView(utf16String1, sizeof(utf16String1) - 1, false, StringEncoding::Utf16);
            expected = StringView(utf8String1, sizeof(utf8String1) - 1, false, StringEncoding::Utf8);
            buffer.clear();
            SC_TEST_EXPECT(StringConverter::convertEncodingToUTF8(input, buffer, &output,
                                                                  StringConverter::DoNotAddZeroTerminator));
            SC_TEST_EXPECT(output == expected);
        }
    }
};
