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
