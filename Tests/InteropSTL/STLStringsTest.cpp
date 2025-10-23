// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Remember to compile this with SC_COMPILER_ENABLE_STD_CPP=1, and possibly exceptions and RTTI enabled

#include "SaneCppSTLAdapters.h"

#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct CppSTLStringsTest;

} // namespace SC

struct SC::CppSTLStringsTest : public SC::TestCase
{
    CppSTLStringsTest(SC::TestReport& report) : TestCase(report, "CppSTLStringsTest")
    {
        if (test_section("std::string format"))
        {
            stringFormat();
        }
        if (test_section("std::string conversions"))
        {
            stringConversions();
        }
        if (test_section("std::vector<char> format"))
        {
            vectorFormat();
        }
    }

    void stringFormat();
    void stringConversions();
    void vectorFormat();
};

void SC::CppSTLStringsTest::stringFormat()
{
    // This will probably be handled inside std::string SSO
    std::string buffer;
    SC_TEST_EXPECT(StringBuilder::format(buffer, "_{}", 123));
    SC_TEST_EXPECT(buffer == "_123");

    // Let's make something that doesn't fit in the SSO and it will be heap allocated
    std::string buffer2;
    SC_TEST_EXPECT(StringBuilder::format(buffer2, "_{0}_{0}_{0}_{0}_{0}_{0}_{0}_{0}", 123456));
    SC_TEST_EXPECT(buffer2 == "_123456_123456_123456_123456_123456_123456_123456_123456");

#if SC_PLATFORM_WINDOWS
    std::wstring wbuffer;
    SC_TEST_EXPECT(StringBuilder::format(wbuffer, "_{0}_{0}_{0}_{0}_{0}_{0}_{0}_{0}", 123456));
    SC_TEST_EXPECT(wbuffer == L"_123456_123456_123456_123456_123456_123456_123456_123456");
#endif
}

void SC::CppSTLStringsTest::stringConversions()
{
    StringView saneStringView = "Sane C++ Libraries";
    String     saneString     = saneStringView;

    SmallString<32> saneSmallString = saneStringView;

    std::string_view stdString      = asStd(saneString);
    std::string_view stdStringView  = asStd(saneStringView);
    std::string_view stdSmallString = asStd(saneSmallString);

    SC_TEST_EXPECT(stdString == stdStringView);
    SC_TEST_EXPECT(stdString == stdSmallString);

    std::string stdSaneString     = asStdString(saneString);      // or asStd with explicit std::string_view constructor
    std::string stdSaneStringView = asStdString(saneStringView);  // or asStd with explicit std::string_view constructor
    std::string stdSaneSmallStr   = asStdString(saneSmallString); // or asStd with explicit std::string_view constructor

    SC_TEST_EXPECT(stdSaneString == stdSaneStringView);
    SC_TEST_EXPECT(stdSaneStringView == stdSaneSmallStr);
    SC_TEST_EXPECT(stdSaneString == asStd(saneString));
    SC_TEST_EXPECT(stdSaneString == asStd(saneStringView));
    SC_TEST_EXPECT(saneString == asSane(stdSaneString));
    SC_TEST_EXPECT(saneStringView == asSane(stdSaneString));
}

void SC::CppSTLStringsTest::vectorFormat()
{
    std::vector<char> buffer;
    SC_TEST_EXPECT(StringBuilder::format(buffer, "_{1}_{0}_{1}_{0}_{1}_{0}_{1}_{0}", "YEAH", "OH"));
    std::string_view bufferView(buffer.data(), buffer.size()); // not null-terminated
    SC_TEST_EXPECT(bufferView == "_OH_YEAH_OH_YEAH_OH_YEAH_OH_YEAH");

#if SC_PLATFORM_WINDOWS
    std::vector<wchar_t> wbuffer;
    SC_TEST_EXPECT(StringBuilder::format(wbuffer, "_{1}_{0}_{1}_{0}_{1}_{0}_{1}_{0}", "YEAH", "OH"));
    std::wstring_view wbufferView(wbuffer.data(), wbuffer.size()); // not null-terminated
    SC_TEST_EXPECT(wbufferView == L"_OH_YEAH_OH_YEAH_OH_YEAH_OH_YEAH");
#endif
}

namespace SC
{
void runCppSTLStringsTest(SC::TestReport& report) { CppSTLStringsTest test(report); }
} // namespace SC
