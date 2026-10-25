// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Strings/StringConverter.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct StringConverterTest;
}

struct SC::StringConverterTest : public SC::TestCase
{
    inline void convertUtf8Utf16();
#if SC_PLATFORM_LINUX
    void exactCapacity();
    void strictInput();
    void prefixAlias();
#endif
    StringConverterTest(SC::TestReport& report) : TestCase(report, "StringConverterTest")
    {
        using namespace SC;
        if (test_section("UTF8<->UTF16"))
        {
            convertUtf8Utf16();
        }
#if SC_PLATFORM_LINUX
        if (test_section("Exact UTF conversion capacity"))
            exactCapacity();
        if (test_section("Strict UTF conversion rejection"))
            strictInput();
        if (test_section("UTF conversion prefix relocation"))
            prefixAlias();
#endif
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
#if SC_PLATFORM_LINUX
namespace
{
using SC::size_t;

bool converterBytesEqual(const char* first, const char* second, size_t size)
{
    for (size_t idx = 0; idx < size; ++idx)
        if (first[idx] != second[idx])
            return false;
    return true;
}

// Physical backing stays large enough to observe the former logical-capacity overrun safely.
struct ConverterTestBuffer : SC::IGrowableBuffer
{
    alignas(8) char first[160];
    alignas(8) char second[160];
    bool   relocate    = false;
    bool   didRelocate = false;
    size_t deniedSize  = 0;

    ConverterTestBuffer(size_t capacity, SC::Span<const char> prefix = {}) : IGrowableBuffer(&grow)
    {
        for (size_t idx = 0; idx < sizeof(first); ++idx)
            first[idx] = second[idx] = '\x5A';
        for (size_t idx = 0; idx < prefix.sizeInBytes(); ++idx)
            first[idx + 9] = prefix[idx];
        directAccess = {prefix.sizeInBytes(), capacity, first + 9}; // Deliberately unaligned UTF16 storage.
    }

    static bool grow(IGrowableBuffer& base, size_t size)
    {
        auto& self = static_cast<ConverterTestBuffer&>(base);
        if (not self.relocate or size > sizeof(self.second) - 9)
        {
            self.deniedSize = size;
            return false;
        }
        for (size_t idx = 0; idx < self.size(); ++idx)
            self.second[idx + 9] = self.data()[idx];
        for (size_t idx = 0; idx < sizeof(self.first); ++idx)
            self.first[idx] = '\x33'; // Stale source pointers cannot accidentally pass the output oracle.
        self.directAccess = {size, sizeof(self.second) - 9, self.second + 9};
        self.didRelocate  = true;
        return true;
    }

    bool guardsIntact(size_t written) const
    {
        const char* storage = didRelocate ? second : first;
        for (size_t idx = 0; idx < sizeof(first); ++idx)
            if ((idx < 9 or idx >= 9 + written) and storage[idx] != '\x5A')
                return false;
        return true;
    }
};
} // namespace

void SC::StringConverterTest::exactCapacity()
{
    const char          ascii[] = "01234567890123456789012345678901";
    ConverterTestBuffer tooSmall(33);
    SC_TEST_EXPECT(not StringConverter::appendEncodingTo(
        StringEncoding::Utf16, {{ascii, sizeof(ascii) - 1}, false, StringEncoding::Utf8},
        static_cast<IGrowableBuffer&>(tooSmall), StringConverter::DoNotTerminate));
    SC_TEST_EXPECT(tooSmall.size() == 0 and tooSmall.deniedSize == 64 and tooSmall.guardsIntact(0));

    for (int terminationIndex = 0; terminationIndex < 2; ++terminationIndex)
    {
        const size_t        finalSize = terminationIndex == 0 ? 64 : 66;
        ConverterTestBuffer exact(finalSize);
        SC_TEST_EXPECT(StringConverter::appendEncodingTo(
            StringEncoding::Utf16, {{ascii, sizeof(ascii) - 1}, false, StringEncoding::Utf8},
            static_cast<IGrowableBuffer&>(exact),
            terminationIndex == 0 ? StringConverter::DoNotTerminate : StringConverter::NullTerminate));
        SC_TEST_EXPECT(exact.size() == finalSize and exact.guardsIntact(finalSize));
        for (size_t idx = 0; idx < sizeof(ascii) - 1; ++idx)
            SC_TEST_EXPECT(exact.data()[2 * idx] == ascii[idx] and exact.data()[2 * idx + 1] == 0);
        if (terminationIndex != 0)
            SC_TEST_EXPECT(exact.data()[64] == 0 and exact.data()[65] == 0);
    }

    const char utf8[]  = {'A', 0, '\xC2', '\xA2', '\xE6', '\x97', '\xA5', '\xF0', '\x9F', '\x98', '\x80'};
    const char utf16[] = {'A', 0, 0, 0, '\xA2', 0, '\xE5', '\x65', '\x3D', '\xD8', 0, '\xDE'};

    alignas(8) char unalignedUtf16[sizeof(utf16) + 1];
    for (size_t idx = 0; idx < sizeof(utf16); ++idx)
        unalignedUtf16[idx + 1] = utf16[idx];
    const Span<const char> sources[] = {{utf8, sizeof(utf8)}, {unalignedUtf16 + 1, sizeof(utf16)}};
    const Span<const char> outputs[] = {{utf16, sizeof(utf16)}, {utf8, sizeof(utf8)}};
    for (size_t direction = 0; direction < 2; ++direction)
    {
        const auto   sourceEncoding = direction == 0 ? StringEncoding::Utf8 : StringEncoding::Utf16;
        const auto   targetEncoding = direction == 0 ? StringEncoding::Utf16 : StringEncoding::Utf8;
        const size_t terminatorSize = StringEncodingGetSize(targetEncoding);
        for (int terminationIndex = 0; terminationIndex < 2; ++terminationIndex)
        {
            const bool   terminate   = terminationIndex != 0;
            const size_t finalSize   = 2 + outputs[direction].sizeInBytes() + (terminate ? terminatorSize : 0);
            const auto   termination = terminate ? StringConverter::NullTerminate : StringConverter::DoNotTerminate;
            ConverterTestBuffer exact(finalSize, {"PQ", 2});
            SC_TEST_EXPECT(StringConverter::appendEncodingTo(targetEncoding,
                                                             {sources[direction], false, sourceEncoding},
                                                             static_cast<IGrowableBuffer&>(exact), termination));
            SC_TEST_EXPECT(exact.size() == finalSize and converterBytesEqual(exact.data(), "PQ", 2));
            SC_TEST_EXPECT(
                converterBytesEqual(exact.data() + 2, outputs[direction].data(), outputs[direction].sizeInBytes()));
            SC_TEST_EXPECT(exact.guardsIntact(finalSize));
            if (terminate)
                SC_TEST_EXPECT(converterBytesEqual(exact.data() + finalSize - terminatorSize, "\0\0", terminatorSize));

            ConverterTestBuffer shortBuffer(finalSize - 1, {"PQ", 2});
            SC_TEST_EXPECT(
                not StringConverter::appendEncodingTo(targetEncoding, {sources[direction], false, sourceEncoding},
                                                      static_cast<IGrowableBuffer&>(shortBuffer), termination));
            SC_TEST_EXPECT(shortBuffer.size() == 2 and converterBytesEqual(shortBuffer.data(), "PQ", 2) and
                           shortBuffer.guardsIntact(2));

            Buffer owning;
            SC_TEST_EXPECT(StringConverter::appendEncodingTo(
                targetEncoding, {sources[direction], false, sourceEncoding}, owning, termination));
            SC_TEST_EXPECT(
                owning.size() == finalSize - 2 and
                converterBytesEqual(owning.data(), outputs[direction].data(), outputs[direction].sizeInBytes()));
        }
    }
}

void SC::StringConverterTest::strictInput()
{
    // Each malformed suffix follows valid ASCII, so validation must finish before any output is committed.
    const char             truncated2[]   = {'A', '\xC2'};
    const char             truncated3[]   = {'A', '\xE2', '\x82'};
    const char             truncated4[]   = {'A', '\xF0', '\x9F', '\x98'};
    const char             continuation[] = {'A', '\xC2', 'B'};
    const char             overlong[]     = {'A', '\xC0', '\x80'};
    const char             surrogate[]    = {'A', '\xED', '\xA0', '\x80'};
    const char             aboveMaximum[] = {'A', '\xF4', '\x90', '\x80', '\x80'};
    const char             high[]         = {'A', 0, 0, '\xD8'};
    const char             low[]          = {'A', 0, 0, '\xDC'};
    const char             badPair[]      = {'A', 0, 0, '\xD8', 'B', 0};
    const char             odd[]          = {'A', 0, 'B'};
    const Span<const char> malformed[]    = {{truncated2, sizeof(truncated2)},
                                             {truncated3, sizeof(truncated3)},
                                             {truncated4, sizeof(truncated4)},
                                             {continuation, sizeof(continuation)},
                                             {overlong, sizeof(overlong)},
                                             {surrogate, sizeof(surrogate)},
                                             {aboveMaximum, sizeof(aboveMaximum)},
                                             {high, sizeof(high)},
                                             {low, sizeof(low)},
                                             {badPair, sizeof(badPair)},
                                             {odd, sizeof(odd)}};
    for (size_t idx = 0; idx < sizeof(malformed) / sizeof(malformed[0]); ++idx)
    {
        const bool fromUtf8 = idx < 7;
        // Offset the source by one too: no aligned uint16_t access is permitted.
        alignas(8) char unaligned[16];
        for (size_t byte = 0; byte < malformed[idx].sizeInBytes(); ++byte)
            unaligned[byte + 1] = malformed[idx][byte];
        ConverterTestBuffer destination(100, {"PQ", 2});
        SC_TEST_EXPECT(not StringConverter::appendEncodingTo(fromUtf8 ? StringEncoding::Utf16 : StringEncoding::Utf8,
                                                             {{unaligned + 1, malformed[idx].sizeInBytes()},
                                                              false,
                                                              fromUtf8 ? StringEncoding::Utf8 : StringEncoding::Utf16},
                                                             static_cast<IGrowableBuffer&>(destination),
                                                             StringConverter::NullTerminate));
        SC_TEST_EXPECT(destination.size() == 2 and destination.deniedSize == 0 and
                       converterBytesEqual(destination.data(), "PQ", 2) and destination.guardsIntact(2));
    }
}

void SC::StringConverterTest::prefixAlias()
{
    const char utf8[]  = {'A', '\xF0', '\x9F', '\x98', '\x80'};
    const char utf16[] = {'A', 0, '\x3D', '\xD8', 0, '\xDE'};

    const Span<const char> sources[] = {{utf8, sizeof(utf8)}, {utf16, sizeof(utf16)}};
    const Span<const char> outputs[] = {{utf16, sizeof(utf16)}, {utf8, sizeof(utf8)}};
    for (size_t direction = 0; direction < 2; ++direction)
    {
        ConverterTestBuffer destination(sources[direction].sizeInBytes() + 2, {"PQ", 2});
        for (size_t idx = 0; idx < sources[direction].sizeInBytes(); ++idx)
            destination.data()[idx + 2] = sources[direction][idx];
        SC_TEST_EXPECT(destination.resizeWithoutInitializing(2 + sources[direction].sizeInBytes()));
        destination.relocate        = true;
        const auto   targetEncoding = direction == 0 ? StringEncoding::Utf16 : StringEncoding::Utf8;
        const size_t oldSize        = destination.size();
        SC_TEST_EXPECT(StringConverter::appendEncodingTo(
            targetEncoding,
            {{destination.data() + 2, sources[direction].sizeInBytes()},
             false,
             direction == 0 ? StringEncoding::Utf8 : StringEncoding::Utf16},
            static_cast<IGrowableBuffer&>(destination), StringConverter::NullTerminate));
        const size_t terminatorSize = StringEncodingGetSize(targetEncoding);
        SC_TEST_EXPECT(destination.didRelocate and
                       destination.size() == oldSize + outputs[direction].sizeInBytes() + terminatorSize);
        SC_TEST_EXPECT(
            converterBytesEqual(destination.data(), "PQ", 2) and
            converterBytesEqual(destination.data() + 2, sources[direction].data(), sources[direction].sizeInBytes()));
        SC_TEST_EXPECT(converterBytesEqual(destination.data() + oldSize, outputs[direction].data(),
                                           outputs[direction].sizeInBytes()));
        SC_TEST_EXPECT(
            converterBytesEqual(destination.data() + destination.size() - terminatorSize, "\0\0", terminatorSize) and
            destination.guardsIntact(destination.size()));
    }
}
#endif
namespace SC
{
void runStringConverterTest(SC::TestReport& report) { StringConverterTest test(report); }
} // namespace SC
