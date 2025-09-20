// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/StringSpan.h"

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // MultiByteToWideChar
#endif

SC::size_t SC::StringSpan::sizeInBytesIncludingTerminator() const
{
    SC_ASSERT_RELEASE(hasNullTerm);
    return textSizeInBytes > 0 ? textSizeInBytes + StringEncodingGetSize(getEncoding()) : 0;
}

const char* SC::StringSpan::bytesIncludingTerminator() const
{
    SC_ASSERT_RELEASE(hasNullTerm);
    return text;
}

SC::Result SC::StringSpan::writeNullTerminatedTo(NativeWritable& string) const
{
    string.length = 0;
    return appendNullTerminatedTo(string, false);
}

SC::Result SC::StringSpan::appendNullTerminatedTo(NativeWritable& string, bool removePreviousNullTerminator) const
{
    const size_t toSlice = removePreviousNullTerminator ? string.length : (string.length > 0 ? string.length + 1 : 0);
    Span<native_char_t> remaining;
    SC_TRY_MSG(string.writableSpan.sliceStart(toSlice, remaining), "StringSpan::append - sliceStart failed");
    size_t         numWritten = 0;
    native_char_t* buffer     = remaining.data();
#if SC_PLATFORM_WINDOWS
    if (getEncoding() == StringEncoding::Utf16)
    {
        SC_TRY_MSG(sizeInBytes() < remaining.sizeInBytes(), "StringSpan::append - exceeded buffer size");
        ::memcpy(buffer, bytesWithoutTerminator(), sizeInBytes());
        buffer[sizeInBytes() / sizeof(wchar_t)] = 0;

        numWritten = sizeInBytes() / sizeof(wchar_t);
    }
    else
    {
        const int stringLen =
            ::MultiByteToWideChar(CP_UTF8, 0, bytesWithoutTerminator(), static_cast<int>(sizeInBytes()), buffer,
                                  static_cast<int>(remaining.sizeInElements()));
        SC_TRY_MSG(stringLen > 0, "StringSpan::append - MultiByteToWideChar failed");
        buffer[stringLen] = L'\0'; // Ensure null termination

        numWritten = static_cast<size_t>(stringLen);
    }
#else
    SC_TRY_MSG(getEncoding() != StringEncoding::Utf16, "StringSpan::append - UTF16 not supported");
    SC_TRY_MSG(sizeInBytes() < remaining.sizeInBytes(), "StringSpan::append - exceeded buffer size");
    ::memcpy(buffer, bytesWithoutTerminator(), sizeInBytes());
    buffer[sizeInBytes()] = 0; // Ensure null termination

    numWritten = sizeInBytes();
#endif
    string.length = toSlice + numWritten;
    return Result(true);
}

SC::uint32_t SC::StringSpan::advanceUTF8(const char*& it, const char* end)
{
    const uint8_t lead = static_cast<uint8_t>(*(it++));
    if (lead < 0x80)
    {
        return lead;
    }
    else if ((lead >> 5) == 0x06 and it < end) // 2-byte sequence
    {
        const uint8_t trail = static_cast<uint8_t>(*(it++));
        if ((trail >> 6) == 0x02)
            return ((lead & 0x1Fu) << 6) | (trail & 0x3Fu);
    }
    else if ((lead >> 4) == 0x0E and it + 1 < end) // 3-byte sequence
    {
        const uint8_t trail1 = static_cast<uint8_t>(*(it++));
        const uint8_t trail2 = static_cast<uint8_t>(*(it++));
        if ((trail1 >> 6) == 0x02 and (trail2 >> 6) == 0x02)
            return ((lead & 0x0Fu) << 12) | ((trail1 & 0x3Fu) << 6) | (trail2 & 0x3Fu);
    }
    else if ((lead >> 3) == 0x1E and it + 2 < end) // 4-byte sequence
    {
        const uint8_t trail1 = static_cast<uint8_t>(*(it++));
        const uint8_t trail2 = static_cast<uint8_t>(*(it++));
        const uint8_t trail3 = static_cast<uint8_t>(*(it++));
        if ((trail1 >> 6) == 0x02 and (trail2 >> 6) == 0x02 and (trail3 >> 6) == 0x02)
            return ((lead & 0x07u) << 18) | ((trail1 & 0x3Fu) << 12) | ((trail2 & 0x3Fu) << 6) | (trail3 & 0x3F);
    }
    return 0; // Invalid sequence
}

SC::uint32_t SC::StringSpan::advanceUTF16(const char*& it, const char* end)
{
    uint16_t lead, trail;
    ::memcpy(&lead, it, sizeof(uint16_t)); // Avoid potential unaligned read
    it += sizeof(uint16_t);
    if (lead < 0xD800 or lead > 0xDFFF)
        return lead;
    ::memcpy(&trail, it, sizeof(uint16_t)); // Avoid potential unaligned read
    if ((lead >= 0xDC00) or (it >= end) or (trail < 0xDC00) or (trail > 0xDFFF))
        return 0; // trail surrogate without lead / incomplete surrogate pair / invalid trail surrogate
    it += sizeof(uint16_t);
    return 0x10000u + ((lead - 0xD800u) << 10) + (trail - 0xDC00u);
}

SC::StringSpan::Comparison SC::StringSpan::compare(StringSpan other) const
{
    if (getEncoding() == other.getEncoding())
    {
        const size_t minSize = sizeInBytes() < other.sizeInBytes() ? sizeInBytes() : other.sizeInBytes();
        if (text == nullptr)
            return other.textSizeInBytes == 0 ? Comparison::Equals : Comparison::Smaller;
        if (other.text == nullptr)
            return textSizeInBytes == 0 ? Comparison::Equals : Comparison::Bigger;
        const int cmp = ::memcmp(text, other.text, minSize);
        if (cmp != 0)
            return cmp < 0 ? Comparison::Smaller : Comparison::Bigger;
        return textSizeInBytes < other.textSizeInBytes
                   ? Comparison::Smaller
                   : (textSizeInBytes > other.textSizeInBytes ? Comparison::Bigger : Comparison::Equals);
    }

    const char *p1 = text, *end1 = p1 + textSizeInBytes;
    const char *p2 = other.text, *end2 = p2 + other.textSizeInBytes;
    while (p1 < end1 and p2 < end2)
    {
        uint32_t cp1 = getEncoding() == StringEncoding::Utf16 ? advanceUTF16(p1, end1) : advanceUTF8(p1, end1);
        uint32_t cp2 = other.getEncoding() == StringEncoding::Utf16 ? advanceUTF16(p2, end2) : advanceUTF8(p2, end2);
        if (cp1 < cp2)
            return Comparison::Smaller;
        if (cp1 > cp2)
            return Comparison::Bigger;
    }
    return p1 < end1 ? Comparison::Bigger : (p2 < end2 ? Comparison::Smaller : Comparison::Equals);
}
