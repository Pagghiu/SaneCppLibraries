// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Foundation/StringSpan.h"
#include "Libraries/Memory/Memory.h"
#include "Libraries/Testing/Limits.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct StringSpanTest;
}

struct SC::StringSpanTest : public SC::TestCase
{
    StringSpanTest(SC::TestReport& report) : TestCase(report, "StringSpanTest")
    {
        using SS = StringSpan;
        using C  = SS::Comparison;

        // Test operator==
        {
            SS a("hello");
            SS b("hello");
            SC_TEST_EXPECT(a == b);
        }
        {
            SS a("hello");
            SS b("world");
            SC_TEST_EXPECT(!(a == b));
        }
        // UTF8 vs ASCII
        {
            SS a = SS::fromNullTerminated("hello", StringEncoding::Utf8);
            SS b("hello");
            SC_TEST_EXPECT(a == b);
        }
        // Different lengths
        {
            SS a("hello");
            SS b("helloworld");
            SC_TEST_EXPECT(!(a == b));
        }

        // Test compare
        {
            SS a("abc");
            SS b("abc");
            SC_TEST_EXPECT(a.compare(b) == C::Equals);
        }
        {
            SS a("abc");
            SS b("abd");
            SC_TEST_EXPECT(a.compare(b) == C::Smaller);
        }
        {
            SS a("abd");
            SS b("abc");
            SC_TEST_EXPECT(a.compare(b) == C::Bigger);
        }
        {
            SS a("abc");
            SS b("abcd");
            SC_TEST_EXPECT(a.compare(b) == C::Smaller);
        }
        {
            SS a("abcd");
            SS b("abc");
            SC_TEST_EXPECT(a.compare(b) == C::Bigger);
        }
        // UTF8 vs ASCII
        {
            SS a = SS::fromNullTerminated("hello", StringEncoding::Utf8);
            SS b("hello");
            SC_TEST_EXPECT(a.compare(b) == C::Equals);
        }
        // UTF16 vs ASCII (assuming little-endian)
        {
            const char utf16[] = {'h', 0, 'e', 0, 'l', 0, 'l', 0, 'o', 0};

            SS a(utf16, false, StringEncoding::Utf16);
            SS b("hello");
            SC_TEST_EXPECT(a.compare(b) == C::Equals);
        }

        // Test 2-byte UTF8 sequences (U+0080 to U+07FF)
        {
            // Latin-1 Supplement: ñ (U+00F1) - 2 bytes: 0xC3 0xB1
            const char utf8_n[] = {static_cast<char>(0xC3), static_cast<char>(0xB1)};

            SS a(utf8_n, false, StringEncoding::Utf8);
            SS b("ñ");
            SC_TEST_EXPECT(a == b);
        }
        {
            // Latin Extended-A: ā (U+0101) - 2 bytes: 0xC4 0x81
            const char utf8_a[]  = {static_cast<char>(0xC4), static_cast<char>(0x81)};
            const char utf8_a2[] = {static_cast<char>(0xC4), static_cast<char>(0x81)};

            SS a(utf8_a, false, StringEncoding::Utf8);
            SS b(utf8_a2, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(a == b);
        }

        // Test 3-byte UTF8 sequences (U+0800 to U+FFFF)
        {
            // Greek: α (U+03B1) - 3 bytes: 0xCE 0xB1
            const char utf8_alpha[]  = {static_cast<char>(0xCE), static_cast<char>(0xB1)};
            const char utf8_alpha2[] = {static_cast<char>(0xCE), static_cast<char>(0xB1)};

            SS b(utf8_alpha2, false, StringEncoding::Utf8);
            SS a(utf8_alpha, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(a == b);
        }
        {
            // Cyrillic: я (U+044F) - 3 bytes: 0xD1 0x8F
            const char utf8_ya[]  = {static_cast<char>(0xD1), static_cast<char>(0x8F)};
            const char utf8_ya2[] = {static_cast<char>(0xD1), static_cast<char>(0x8F)};

            SS a(utf8_ya, false, StringEncoding::Utf8);
            SS b(utf8_ya2, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(a == b);
        }
        {
            // CJK: 你 (U+4F60) - 3 bytes: 0xE4 0xBD 0xA0
            const char utf8_ni[]  = {static_cast<char>(0xE4), static_cast<char>(0xBD), static_cast<char>(0xA0)};
            const char utf8_ni2[] = {static_cast<char>(0xE4), static_cast<char>(0xBD), static_cast<char>(0xA0)};

            SS a(utf8_ni, false, StringEncoding::Utf8);
            SS b(utf8_ni2, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(a == b);
        }

        // Test 4-byte UTF8 sequences (U+10000 to U+10FFFF)
        {
            // Emoji: 😀 (U+1F600) - 4 bytes: 0xF0 0x9F 0x98 0x80
            const char utf8_grinning[]  = {static_cast<char>(0xF0), static_cast<char>(0x9F), static_cast<char>(0x98),
                                           static_cast<char>(0x80)};
            const char utf8_grinning2[] = {static_cast<char>(0xF0), static_cast<char>(0x9F), static_cast<char>(0x98),
                                           static_cast<char>(0x80)};

            SS a(utf8_grinning, false, StringEncoding::Utf8);
            SS b(utf8_grinning2, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(a == b);
        }
        {
            // Musical symbol: 𝄞 (U+1D11E) - 4 bytes: 0xF0 0x9D 0x84 0x9E
            const char utf8_gclef[]  = {static_cast<char>(0xF0), static_cast<char>(0x9D), static_cast<char>(0x84),
                                        static_cast<char>(0x9E)};
            const char utf8_gclef2[] = {static_cast<char>(0xF0), static_cast<char>(0x9D), static_cast<char>(0x84),
                                        static_cast<char>(0x9E)};

            SS a(utf8_gclef, false, StringEncoding::Utf8);
            SS b(utf8_gclef2, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(a == b);
        }

        // Test UTF16 surrogate pairs
        {
            // Emoji: 😀 (U+1F600) - UTF16 surrogate pair: 0xD83D 0xDE00
            const char utf16_grinning[]  = {static_cast<char>(0x3D), static_cast<char>(0xD8), static_cast<char>(0x00),
                                            static_cast<char>(0xDE)};
            const char utf16_grinning2[] = {static_cast<char>(0x3D), static_cast<char>(0xD8), static_cast<char>(0x00),
                                            static_cast<char>(0xDE)};

            SS a(utf16_grinning, false, StringEncoding::Utf16);
            SS b(utf16_grinning2, false, StringEncoding::Utf16);
            SC_TEST_EXPECT(a == b);
        }
        {
            // Gothic letter: 𐌰 (U+10330) - UTF16 surrogate pair: 0xD800 0xDF30
            const char utf16_gothic[]  = {static_cast<char>(0x00), static_cast<char>(0xD8), static_cast<char>(0x30),
                                          static_cast<char>(0xDF)};
            const char utf16_gothic2[] = {static_cast<char>(0x00), static_cast<char>(0xD8), static_cast<char>(0x30),
                                          static_cast<char>(0xDF)};

            SS a(utf16_gothic, false, StringEncoding::Utf16);
            SS b(utf16_gothic2, false, StringEncoding::Utf16);
            SC_TEST_EXPECT(a == b);
        }

        // Test mixed encoding comparisons
        {
            // UTF8 2-byte vs UTF16 BMP
            const char utf8_n[]  = {static_cast<char>(0xC3), static_cast<char>(0xB1)}; // ñ
            const char utf16_n[] = {static_cast<char>(0xF1), static_cast<char>(0x00)}; // ñ in UTF16

            SS utf8_span(utf8_n, false, StringEncoding::Utf8);
            SS utf16_span(utf16_n, false, StringEncoding::Utf16);
            SC_TEST_EXPECT(utf8_span == utf16_span);
        }
        {
            // UTF8 4-byte vs UTF16 surrogate pair
            const char utf8_emoji[]  = {static_cast<char>(0xF0), static_cast<char>(0x9F), static_cast<char>(0x98),
                                        static_cast<char>(0x80)}; // 😀
            const char utf16_emoji[] = {static_cast<char>(0x3D), static_cast<char>(0xD8), static_cast<char>(0x00),
                                        static_cast<char>(0xDE)}; // 😀 surrogate pair

            SS utf8_span(utf8_emoji, false, StringEncoding::Utf8);
            SS utf16_span(utf16_emoji, false, StringEncoding::Utf16);
            SC_TEST_EXPECT(utf8_span == utf16_span);
        }

        // Test comparison ordering with different byte sequences
        {
            // 2-byte UTF8 comparison
            const char utf8_a[] = {static_cast<char>(0xC3), static_cast<char>(0xA1)}; // á (U+00E1)
            const char utf8_b[] = {static_cast<char>(0xC3), static_cast<char>(0xA9)}; // é (U+00E9)

            SS a_span(utf8_a, false, StringEncoding::Utf8);
            SS b_span(utf8_b, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(a_span.compare(b_span) == C::Smaller);
        }
        {
            // 3-byte UTF8 comparison
            const char utf8_alpha[] = {static_cast<char>(0xCE), static_cast<char>(0xB1)}; // α (U+03B1)
            const char utf8_beta[]  = {static_cast<char>(0xCE), static_cast<char>(0xB2)}; // β (U+03B2)

            SS alpha_span(utf8_alpha, false, StringEncoding::Utf8);
            SS beta_span(utf8_beta, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(alpha_span.compare(beta_span) == C::Smaller);
        }
        {
            // 4-byte UTF8 comparison
            const char utf8_emoji1[] = {static_cast<char>(0xF0), static_cast<char>(0x9F), static_cast<char>(0x98),
                                        static_cast<char>(0x80)}; // 😀
            const char utf8_emoji2[] = {static_cast<char>(0xF0), static_cast<char>(0x9F), static_cast<char>(0x98),
                                        static_cast<char>(0x81)}; // 😁

            SS emoji1_span(utf8_emoji1, false, StringEncoding::Utf8);
            SS emoji2_span(utf8_emoji2, false, StringEncoding::Utf8);
            SC_TEST_EXPECT(emoji1_span.compare(emoji2_span) == C::Smaller);
        }
    }
};

namespace SC
{
void runStringSpanTest(SC::TestReport& report) { StringSpanTest test(report); }
} // namespace SC
