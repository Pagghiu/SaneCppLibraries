// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Foundation/Deferred.h"
#include "../../Foundation/Result.h"
#include "../../Strings/StringConverter.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif SC_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>
#endif

struct SC::StringConverter::Internal
{
    // Fallbacks for platforms without an API to do the conversion out of the box (Linux)
    [[nodiscard]] static bool convertUTF16LE_to_UTF8(const StringSpan sourceUtf16, IGrowableBuffer& destination,
                                                     int& writtenCodeUnits);
    [[nodiscard]] static bool convertUTF8_to_UTF16LE(const StringSpan sourceUtf8, IGrowableBuffer& destination,
                                                     int& writtenCodeUnits);
    [[nodiscard]] static bool convertSameEncoding(StringSpan text, IGrowableBuffer& buffer,
                                                  StringTermination terminate);
    [[nodiscard]] static bool convertEncodingToUTF8(StringSpan text, IGrowableBuffer& ibuffer,
                                                    StringTermination terminate);
    [[nodiscard]] static bool convertEncodingToUTF16(StringSpan text, IGrowableBuffer& ibuffer,
                                                     StringTermination terminate);

    static void eventuallyNullTerminate(IGrowableBuffer& buffer, StringEncoding destinationEncoding,
                                        StringTermination terminate);

    struct ConverterBuffer : public IGrowableBuffer
    {
        size_t size() const { return directAccess.sizeInBytes; }
        char*  data() const { return static_cast<char*>(directAccess.data); }

        Span<const char> toSpanConst() const { return {data(), size()}; }

        [[nodiscard]] bool append(Span<const char> span, size_t extraZeroes = 0)
        {
            const size_t oldSize = size();
            const size_t newSize = oldSize + span.sizeInBytes() + extraZeroes;
            SC_TRY(resizeWithoutInitializing(newSize));
            if (not span.empty())
            {
                ::memcpy(data() + oldSize, span.data(), span.sizeInBytes());
            }
            if (extraZeroes > 0)
            {
                ::memset(data() + oldSize + span.sizeInBytes(), 0, extraZeroes);
            }
            return true;
        }
    };
};

bool SC::StringConverter::Internal::convertSameEncoding(StringSpan text, IGrowableBuffer& ibuffer,
                                                        StringTermination terminate)
{
    Internal::ConverterBuffer& buffer = static_cast<Internal::ConverterBuffer&>(ibuffer);

    const size_t nullBytes = terminate == NullTerminate ? StringEncodingGetSize(text.getEncoding()) : 0;
    SC_TRY(buffer.append(text.toCharSpan(), nullBytes));
    return true;
}

void SC::StringConverter::Internal::eventuallyNullTerminate(IGrowableBuffer&                   ibuffer,
                                                            StringEncoding                     destinationEncoding,
                                                            StringConverter::StringTermination terminate)
{
    Internal::ConverterBuffer& buffer = static_cast<Internal::ConverterBuffer&>(ibuffer);

    const auto destinationCharSize = StringEncodingGetSize(destinationEncoding);
    if (terminate == StringConverter::NullTerminate)
    {
        char*  bufferData = buffer.data();
        size_t size       = buffer.size();
        for (uint32_t idx = 0; idx < destinationCharSize; ++idx)
        {
            bufferData[size - idx - 1] = 0; // null terminator
        }
    }
}

bool SC::StringConverter::Internal::convertEncodingToUTF8(StringSpan text, IGrowableBuffer& ibuffer,
                                                          StringTermination terminate)
{
    Internal::ConverterBuffer& buffer = static_cast<Internal::ConverterBuffer&>(ibuffer);
    if (text.getEncoding() == StringEncoding::Utf8 || text.getEncoding() == StringEncoding::Ascii)
    {
        return Internal::convertSameEncoding(text, buffer, terminate);
    }
    else if (text.getEncoding() == StringEncoding::Utf16)
    {
        const bool nullTerminate = terminate == NullTerminate;
        const auto oldSize       = buffer.size();
#if SC_PLATFORM_WINDOWS
        const int      sourceSizeInBytes = static_cast<int>(text.sizeInBytes());
        const wchar_t* source            = reinterpret_cast<const wchar_t*>(text.bytesWithoutTerminator());
        const int      numChars =
            WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(uint16_t), nullptr, 0, nullptr, 0);
#elif SC_PLATFORM_APPLE
        const int    sourceSizeInBytes = static_cast<int>(text.sizeInBytes());
        CFIndex      numChars          = 0;
        const UInt8* source            = reinterpret_cast<const UInt8*>(text.bytesWithoutTerminator());
        CFStringRef  tmpStr =
            CFStringCreateWithBytes(kCFAllocatorDefault, source, sourceSizeInBytes, kCFStringEncodingUTF16, false);
        auto    deferDeleteTempString = MakeDeferred([&] { CFRelease(tmpStr); });
        CFRange charRange             = {0, CFStringGetLength(tmpStr)};
        CFStringGetBytes(tmpStr, charRange, kCFStringEncodingUTF8, 0, false, NULL, 0, &numChars);
#else
        int numChars = -1;
        SC_TRY(Internal::convertUTF16LE_to_UTF8(text, buffer, numChars));
#endif

        if (numChars <= 0)
        {
            return false;
        }
        const size_t newSize = oldSize + (static_cast<size_t>(numChars) + (nullTerminate ? 1 : 0));
        SC_TRY(buffer.resizeWithoutInitializing(newSize));
#if SC_PLATFORM_WINDOWS
        WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(uint16_t),
                            reinterpret_cast<char*>(buffer.data() + oldSize), numChars, nullptr, 0);
#elif SC_PLATFORM_APPLE
        CFStringGetBytes(tmpStr, charRange, kCFStringEncodingUTF8, 0, false,
                         reinterpret_cast<UInt8*>(buffer.data() + oldSize), numChars, NULL);
#endif
        Internal::eventuallyNullTerminate(buffer, StringEncoding::Utf8, terminate);
        return true;
    }
    return false;
}

bool SC::StringConverter::Internal::convertEncodingToUTF16(StringSpan text, IGrowableBuffer& ibuffer,
                                                           StringTermination terminate)
{
    Internal::ConverterBuffer& buffer = static_cast<Internal::ConverterBuffer&>(ibuffer);
    if (text.getEncoding() == StringEncoding::Utf16)
    {
        return Internal::convertSameEncoding(text, buffer, terminate);
    }
    else if (text.getEncoding() == StringEncoding::Utf8 || text.getEncoding() == StringEncoding::Ascii)
    {
        const StringEncoding destinationEncoding = StringEncoding::Utf16;

        const bool nullTerminate       = terminate == NullTerminate;
        const auto destinationCharSize = StringEncodingGetSize(destinationEncoding);

        const auto oldSize = buffer.size();

#if SC_PLATFORM_WINDOWS
        const int sourceSizeInBytes = static_cast<int>(text.sizeInBytes());
        const int writtenCodeUnits =
            MultiByteToWideChar(CP_UTF8, 0, text.bytesWithoutTerminator(), sourceSizeInBytes, nullptr, 0);
#elif SC_PLATFORM_APPLE
        const int    sourceSizeInBytes = static_cast<int>(text.sizeInBytes());
        CFIndex      numChars          = 0;
        const UInt8* source            = reinterpret_cast<const UInt8*>(text.bytesWithoutTerminator());
        CFStringRef  tmpStr =
            CFStringCreateWithBytes(kCFAllocatorDefault, source, sourceSizeInBytes, kCFStringEncodingUTF8, false);
        auto    deferDeleteTempString = MakeDeferred([&] { CFRelease(tmpStr); });
        CFRange charRange             = {0, CFStringGetLength(tmpStr)};
        CFStringGetBytes(tmpStr, charRange, kCFStringEncodingUTF16, 0, false, NULL, 0, &numChars);
        CFIndex writtenCodeUnits = numChars / static_cast<CFIndex>(destinationCharSize);
#else
        int writtenCodeUnits = -1;
        SC_TRY(Internal::convertUTF8_to_UTF16LE(text, buffer, writtenCodeUnits));
#endif
        if (writtenCodeUnits <= 0)
        {
            return false;
        }
        const size_t newSize =
            oldSize + (static_cast<size_t>(writtenCodeUnits) + (nullTerminate ? 1 : 0)) * destinationCharSize;
        SC_TRY(buffer.resizeWithoutInitializing(newSize));
#if SC_PLATFORM_WINDOWS
        MultiByteToWideChar(CP_UTF8, 0, text.bytesWithoutTerminator(), static_cast<int>(text.sizeInBytes()),
                            reinterpret_cast<wchar_t*>(buffer.data() + oldSize), writtenCodeUnits);
#elif SC_PLATFORM_APPLE
        CFStringGetBytes(tmpStr, charRange, kCFStringEncodingUTF16, 0, false,
                         reinterpret_cast<UInt8*>(buffer.data() + oldSize), numChars, NULL);
#endif
        Internal::eventuallyNullTerminate(buffer, destinationEncoding, terminate);
        return true;
    }
    return false;
}

bool SC::StringConverter::appendEncodingTo(StringEncoding encoding, StringSpan text, IGrowableBuffer& buffer,
                                           StringTermination terminate)
{
    if (text.isEmpty())
    {
        if (terminate == NullTerminate)
        {
            return static_cast<Internal::ConverterBuffer&>(buffer).append({}, StringEncodingGetSize(encoding));
        }
        return true;
    }

    switch (encoding)
    {
    case StringEncoding::Ascii: return Internal::convertEncodingToUTF8(text, buffer, terminate);
    case StringEncoding::Utf8: return Internal::convertEncodingToUTF8(text, buffer, terminate);
    case StringEncoding::Utf16: return Internal::convertEncodingToUTF16(text, buffer, terminate);
    }
    return false;
}

#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_APPLE

bool SC::StringConverter::Internal::convertUTF8_to_UTF16LE(const StringSpan sourceUtf8, IGrowableBuffer& ibuffer,
                                                           int& writtenCodeUnits)
{
    Internal::ConverterBuffer& buffer = static_cast<Internal::ConverterBuffer&>(ibuffer);

    const char*  utf8    = sourceUtf8.bytesWithoutTerminator();
    const size_t utf8Len = sourceUtf8.sizeInBytes();

    // Calculate the maximum possible size for the UTF-16 string
    // Each UTF-8 character can be represented by at most one UTF-16 code unit
    SC_TRY(buffer.resizeWithoutInitializing(utf8Len + 1));

    uint16_t* utf16 = reinterpret_cast<uint16_t*>(buffer.data());

    size_t srcIndex = 0;
    size_t dstIndex = 0;

    // Assuming little-endian byte order for UTF-16
    while (srcIndex < utf8Len)
    {
        uint32_t codePoint;

        // Decode the next UTF-8 character
        if ((utf8[srcIndex] & 0x80) == 0x00)
        {
            // Single-byte character
            codePoint = static_cast<uint32_t>(utf8[srcIndex++]);
        }
        else if ((utf8[srcIndex] & 0xE0) == 0xC0)
        {
            // Two-byte character
            codePoint = static_cast<uint32_t>(((utf8[srcIndex] & 0x1F) << 6) | (utf8[srcIndex + 1] & 0x3F));
            srcIndex += 2;
        }
        else if ((utf8[srcIndex] & 0xF0) == 0xE0)
        {
            // Three-byte character
            codePoint = static_cast<uint32_t>(((utf8[srcIndex] & 0x0F) << 12) | ((utf8[srcIndex + 1] & 0x3F) << 6) |
                                              (utf8[srcIndex + 2] & 0x3F));
            srcIndex += 3;
        }
        else if ((utf8[srcIndex] & 0xF8) == 0xF0)
        {
            // Four-byte character
            codePoint = static_cast<uint32_t>(((utf8[srcIndex] & 0x07) << 18) | ((utf8[srcIndex + 1] & 0x3F) << 12) |
                                              ((utf8[srcIndex + 2] & 0x3F) << 6) | (utf8[srcIndex + 3] & 0x3F));
            srcIndex += 4;
        }
        else
        {
            return false;
        }

        // Encode the code point in UTF-16
        if (codePoint <= 0xFFFF)
        {
            // Single 16-bit code unit
            utf16[dstIndex++] = (uint16_t)codePoint;
        }
        else if (codePoint <= 0x10FFFF)
        {
            // Surrogate pair
            utf16[dstIndex++] = (uint16_t)(((codePoint - 0x10000) >> 10) | 0xD800);
            utf16[dstIndex++] = (uint16_t)(((codePoint - 0x10000) & 0x3FF) | 0xDC00);
        }
        else
        {
            return false;
        }
    }
    writtenCodeUnits = static_cast<int>(dstIndex);
    return true;
}

bool SC::StringConverter::Internal::convertUTF16LE_to_UTF8(const StringSpan sourceUtf16, IGrowableBuffer& ibuffer,
                                                           int& writtenCodeUnits)
{
    Internal::ConverterBuffer& buffer = static_cast<Internal::ConverterBuffer&>(ibuffer);

    const uint16_t* utf16    = reinterpret_cast<const uint16_t*>(sourceUtf16.bytesWithoutTerminator());
    const size_t    utf16Len = sourceUtf16.sizeInBytes() / sizeof(uint16_t);

    // Calculate the maximum possible size for the UTF-8 string
    // UTF-8 uses at most 4 bytes per character
    SC_TRY(buffer.resizeWithoutInitializing(4 * utf16Len + 1));

    char*  utf8     = buffer.data();
    size_t srcIndex = 0;
    size_t dstIndex = 0;

    // Assuming little-endian byte order for UTF-16
    while (srcIndex < utf16Len)
    {
        uint32_t codePoint;

        if ((utf16[srcIndex] & 0xFC00) == 0xD800 && (srcIndex + 1) < utf16Len &&
            (utf16[srcIndex + 1] & 0xFC00) == 0xDC00)
        {
            // Surrogate pair
            codePoint = 0x10000U + ((utf16[srcIndex] & 0x3FFU) << 10U) + (utf16[srcIndex + 1] & 0x3FF);
            srcIndex += 2;
        }
        else
        {
            // Single 16-bit code unit
            codePoint = utf16[srcIndex];
            srcIndex += 1;
        }

        // Encode the code point in UTF-8
        if (codePoint <= 0x7F)
        {
            // Single-byte character
            utf8[dstIndex++] = static_cast<char>(codePoint);
        }
        else if (codePoint <= 0x7FF)
        {
            // Two-byte character
            utf8[dstIndex++] = static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F));
            utf8[dstIndex++] = static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else if (codePoint <= 0xFFFF)
        {
            // Three-byte character
            utf8[dstIndex++] = static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F));
            utf8[dstIndex++] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            utf8[dstIndex++] = static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else if (codePoint <= 0x10FFFF)
        {
            // Four-byte character
            utf8[dstIndex++] = static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07));
            utf8[dstIndex++] = static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
            utf8[dstIndex++] = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
            utf8[dstIndex++] = static_cast<char>(0x80 | (codePoint & 0x3F));
        }
        else
        {
            return false; // Invalid sequence
        }
    }
    writtenCodeUnits = static_cast<int>(dstIndex);
    return true;
}
#endif
