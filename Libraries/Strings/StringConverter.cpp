// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../Strings/StringConverter.h"
#include "../Foundation/Deferred.h"
#include "../Foundation/Result.h"
#include "../Strings/String.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif SC_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>
#endif

SC::StringConverter::StringConverter(String& text, Flags flags) : encoding(text.getEncoding()), data(text.data)
{
    if (flags == Clear)
    {
        internalClear();
    }
}

SC::StringConverter::StringConverter(Vector<char>& data, StringEncoding encoding) : encoding(encoding), data(data) {}

bool SC::StringConverter::convertSameEncoding(StringView text, Vector<char>& buffer, StringView* encodedText,
                                              NullTermination terminate)
{
    const bool nullTerminate = terminate == AddZeroTerminator;
    if (text.isNullTerminated())
    {
        const bool forceCopy = encodedText == nullptr;
        if (forceCopy)
        {
            SC_TRY(buffer.append({text.bytesIncludingTerminator(),
                                  nullTerminate ? text.sizeInBytesIncludingTerminator() : text.sizeInBytes()}));
        }
        else
        {
            if (nullTerminate)
            {
                *encodedText = text;
            }
            else
            {
                *encodedText =
                    StringView({text.bytesWithoutTerminator(), text.sizeInBytes()}, false, text.getEncoding());
            }
        }
    }
    else
    {
        if (nullTerminate)
        {
            const auto numZeros = StringEncodingGetSize(text.getEncoding());
            SC_TRY(buffer.reserve(buffer.size() + text.sizeInBytes() + numZeros));
            SC_TRY(buffer.append(text.toCharSpan()));
            if (encodedText)
            {
                *encodedText = StringView(buffer.toSpanConst(), true, text.getEncoding());
            }
            SC_TRY(buffer.resize(buffer.size() + numZeros, 0)); // null terminator
        }
        else if (encodedText)
        {
            *encodedText = text;
        }
    }
    return true;
}

void SC::StringConverter::eventuallyNullTerminate(Vector<char>& buffer, StringEncoding destinationEncoding,
                                                  StringView* encodedText, StringConverter::NullTermination terminate)
{
    const auto destinationCharSize = StringEncodingGetSize(destinationEncoding);
    if (terminate == StringConverter::AddZeroTerminator)
    {
        for (uint32_t idx = 0; idx < destinationCharSize; ++idx)
        {
            buffer[buffer.size() - idx - 1] = 0; // null terminator
        }
        if (encodedText)
        {
            *encodedText = StringView({buffer.data(), buffer.size() - destinationCharSize}, true, destinationEncoding);
        }
    }
    else if (encodedText)
    {
        *encodedText = StringView(buffer.toSpanConst(), false, destinationEncoding);
    }
}

bool SC::StringConverter::convertEncodingToUTF8(StringView text, Vector<char>& buffer, StringView* encodedText,
                                                NullTermination terminate)
{
    if (text.isEmpty())
    {
        return false;
    }
    if (text.getEncoding() == StringEncoding::Utf8 || text.getEncoding() == StringEncoding::Ascii)
    {
        return convertSameEncoding(text, buffer, encodedText, terminate);
    }
    else if (text.getEncoding() == StringEncoding::Utf16)
    {
        const bool nullTerminate = terminate == AddZeroTerminator;
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
        SC_TRY(convertUTF16LE_to_UTF8(text, buffer, numChars));
#endif

        if (numChars <= 0)
        {
            return false;
        }
        SC_TRY(buffer.resizeWithoutInitializing(oldSize + (static_cast<size_t>(numChars) + (nullTerminate ? 1 : 0))));
#if SC_PLATFORM_WINDOWS
        WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(uint16_t),
                            reinterpret_cast<char*>(buffer.data() + oldSize), numChars, nullptr, 0);
#elif SC_PLATFORM_APPLE
        CFStringGetBytes(tmpStr, charRange, kCFStringEncodingUTF8, 0, false,
                         reinterpret_cast<UInt8*>(buffer.data() + oldSize), numChars, NULL);
#endif
        eventuallyNullTerminate(buffer, StringEncoding::Utf8, encodedText, terminate);
        return true;
    }
    return false;
}

bool SC::StringConverter::convertEncodingToUTF16(StringView text, Vector<char>& buffer, StringView* encodedText,
                                                 NullTermination terminate)
{
    if (text.isEmpty())
    {
        return false;
    }
    if (text.getEncoding() == StringEncoding::Utf16)
    {
        return convertSameEncoding(text, buffer, encodedText, terminate);
    }
    else if (text.getEncoding() == StringEncoding::Utf8 || text.getEncoding() == StringEncoding::Ascii)
    {
        const StringEncoding destinationEncoding = StringEncoding::Utf16;

        const bool nullTerminate       = terminate == AddZeroTerminator;
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
        SC_TRY(convertUTF8_to_UTF16LE(text, buffer, writtenCodeUnits));
#endif
        if (writtenCodeUnits <= 0)
        {
            return false;
        }
        SC_TRY(buffer.resizeWithoutInitializing(
            oldSize + (static_cast<size_t>(writtenCodeUnits) + (nullTerminate ? 1 : 0)) * destinationCharSize));
#if SC_PLATFORM_WINDOWS
        MultiByteToWideChar(CP_UTF8, 0, text.bytesWithoutTerminator(), static_cast<int>(text.sizeInBytes()),
                            reinterpret_cast<wchar_t*>(buffer.data() + oldSize), writtenCodeUnits);
#elif SC_PLATFORM_APPLE
        CFStringGetBytes(tmpStr, charRange, kCFStringEncodingUTF16, 0, false,
                         reinterpret_cast<UInt8*>(buffer.data() + oldSize), numChars, NULL);
#endif
        eventuallyNullTerminate(buffer, destinationEncoding, encodedText, terminate);
        return true;
    }
    return false;
}

bool SC::StringConverter::convertEncodingTo(StringEncoding encoding, StringView text, Vector<char>& buffer,
                                            StringView* encodedText, NullTermination terminate)
{
    switch (encoding)
    {
    case StringEncoding::Ascii: return convertEncodingToUTF8(text, buffer, encodedText, terminate);
    case StringEncoding::Utf8: return convertEncodingToUTF8(text, buffer, encodedText, terminate);
    case StringEncoding::Utf16: return convertEncodingToUTF16(text, buffer, encodedText, terminate);
    }
    return false;
}

void SC::StringConverter::internalClear() { data.clearWithoutInitializing(); }

bool SC::StringConverter::convertNullTerminateFastPath(StringView input, StringView& encodedText)
{
    data.clearWithoutInitializing();
    SC_TRY(internalAppend(input, &encodedText));
    return true;
}

bool SC::StringConverter::appendNullTerminated(StringView input, bool popExistingNullTerminator)
{
    if (popExistingNullTerminator)
    {
        (void)StringConverter::popNullTermIfNotEmpty(data, encoding);
    }
    return internalAppend(input, nullptr);
}

bool SC::StringConverter::setTextLengthInBytesIncludingTerminator(size_t newDataSize)
{
    const auto zeroSize = StringEncodingGetSize(encoding);
    if (newDataSize >= zeroSize)
    {
        const bool res = data.resizeWithoutInitializing(newDataSize - zeroSize);
        return res && data.resize(newDataSize, 0); // Adds the null terminator
    }
    return true;
}

bool SC::StringConverter::internalAppend(StringView input, StringView* encodedText)
{
    return StringConverter::convertEncodingTo(encoding, input, data, encodedText);
}

bool SC::StringConverter::ensureZeroTermination(Vector<char>& data, StringEncoding encoding)
{
    const size_t numZeros = StringEncodingGetSize(encoding);
    if (data.size() >= numZeros)
    {
        for (size_t idx = 0; idx < numZeros; ++idx)
        {
            (&data.back())[-static_cast<int>(idx)] = 0;
        }
    }
    return true;
}

bool SC::StringConverter::popNullTermIfNotEmpty(Vector<char>& stringData, StringEncoding encoding)
{
    const auto sizeOfZero = StringEncodingGetSize(encoding);
    const auto dataSize   = stringData.size();
    if (dataSize >= sizeOfZero)
    {
        (void)stringData.resizeWithoutInitializing(dataSize - sizeOfZero);
        return true;
    }
    else
    {
        return false;
    }
}

bool SC::StringConverter::pushNullTerm(Vector<char>& stringData, StringEncoding encoding)
{
    return stringData.resize(stringData.size() + StringEncodingGetSize(encoding), 0);
}

#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_APPLE

// Fallbacks for platforms without a supported fast conversion function (Linux for now)

bool SC::StringConverter::convertUTF8_to_UTF16LE(const SC::StringView sourceUtf8, SC::Vector<char>& destination,
                                                 int& writtenCodeUnits)
{
    const char*  utf8    = sourceUtf8.bytesWithoutTerminator();
    const size_t utf8Len = sourceUtf8.sizeInBytes();

    // Calculate the maximum possible size for the UTF-16 string
    // Each UTF-8 character can be represented by at most one UTF-16 code unit
    SC_TRY(destination.resizeWithoutInitializing(utf8Len + 1));

    uint16_t* utf16 = reinterpret_cast<uint16_t*>(destination.data());

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

bool SC::StringConverter::convertUTF16LE_to_UTF8(const SC::StringView sourceUtf16, SC::Vector<char>& destination,
                                                 int& writtenCodeUnits)
{
    const uint16_t* utf16    = reinterpret_cast<const uint16_t*>(sourceUtf16.bytesWithoutTerminator());
    const size_t    utf16Len = sourceUtf16.sizeInBytes() / sizeof(uint16_t);

    // Calculate the maximum possible size for the UTF-8 string
    // UTF-8 uses at most 4 bytes per character
    SC_TRY(destination.resizeWithoutInitializing(4 * utf16Len + 1));

    char*  utf8     = destination.data();
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
