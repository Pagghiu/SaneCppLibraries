// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Strings/StringConverter.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct StringConverterTest;
}

struct SC::StringConverterTest : public SC::TestCase
{
    inline void convertUtf8Utf16();
    StringConverterTest(SC::TestReport& report) : TestCase(report, "StringConverterTest")
    {
        using namespace SC;
        if (test_section("UTF8<->UTF16"))
        {
            convertUtf8Utf16();
        }
    }
};

void SC::StringConverterTest::convertUtf8Utf16()
{
    //! [stringConverterTestSnippet]
    // Setup: Construct some simple test StringViews
    const char utf8String1[]  = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"; // "日本語" in UTF-8
    const char utf16String1[] = "\xE5\x65\x2C\x67\x9E\x8a";             // "日本語" in UTF-16LE

    StringView u8view  = StringView({utf8String1, sizeof(utf8String1) - 1}, false, StringEncoding::Utf8);
    StringView u16view = StringView({utf16String1, sizeof(utf16String1) - 1}, false, StringEncoding::Utf16);

    StringEncoding encoding = StringEncoding::Utf16;

    // Example 1: Use a String without needing to add a zero terminator (it's handled automatically)
    String string = encoding; // Important: set the encoding, as StringConverter will not change it
    SC_TEST_EXPECT(StringConverter::appendEncodingTo(encoding, u8view, string, StringConverter::DoNotTerminate));
    SC_TEST_EXPECT(string.view() == u16view);

    // Example 2: Manually append to a plain byte buffer works as well, in this case we also null terminate it
    SmallBuffer<255> buffer;
    SC_TEST_EXPECT(StringConverter::appendEncodingTo(encoding, u8view, buffer, StringConverter::NullTerminate));

    // Manually build a StringView from the buffer, taking care of slicing the null bytes at the end
    Span<const char> span;
    SC_TEST_EXPECT(buffer.toSpanConst().sliceStartLength(0, buffer.size() - StringEncodingGetSize(encoding), span));
    StringView output = {span, true, encoding}; // true == StringView is null terminated (after span)
    SC_TEST_EXPECT(output == u16view);
    //! [stringConverterTestSnippet]
}
namespace SC
{
void runStringConverterTest(SC::TestReport& report) { StringConverterTest test(report); }
} // namespace SC
