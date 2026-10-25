// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "../../Common/Deferred.h"
#include "../../Common/Result.h"
#include "../../Strings/StringConverter.h"
#include <memory.h> // memcpy
#include <stdint.h> // uintptr_t

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif SC_PLATFORM_APPLE
#include <CoreFoundation/CoreFoundation.h>
#endif

struct SC::StringConverter::Internal
{
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_APPLE
    [[nodiscard]] static bool readCodePoint(const char*& source, const char* end, bool utf16, uint32_t& codePoint);
    [[nodiscard]] static bool convertFallback(StringSpan text, IGrowableBuffer& buffer, StringTermination terminate,
                                              StringEncoding destinationEncoding);
#endif
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
    if (text.getEncoding() == StringEncoding::Utf8 || text.getEncoding() == StringEncoding::Ascii)
    {
        return Internal::convertSameEncoding(text, ibuffer, terminate);
    }
    else if (text.getEncoding() == StringEncoding::Utf16)
    {
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_APPLE
        return Internal::convertFallback(text, ibuffer, terminate, StringEncoding::Utf8);
#else
        Internal::ConverterBuffer& buffer        = static_cast<Internal::ConverterBuffer&>(ibuffer);
        const bool                 nullTerminate = terminate == NullTerminate;
        const auto                 oldSize       = buffer.size();
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
#endif
    }
    return false;
}

bool SC::StringConverter::Internal::convertEncodingToUTF16(StringSpan text, IGrowableBuffer& ibuffer,
                                                           StringTermination terminate)
{
    if (text.getEncoding() == StringEncoding::Utf16)
    {
        return Internal::convertSameEncoding(text, ibuffer, terminate);
    }
    else if (text.getEncoding() == StringEncoding::Utf8 || text.getEncoding() == StringEncoding::Ascii)
    {
#if !SC_PLATFORM_WINDOWS && !SC_PLATFORM_APPLE
        return Internal::convertFallback(text, ibuffer, terminate, StringEncoding::Utf16);
#else
        Internal::ConverterBuffer& buffer              = static_cast<Internal::ConverterBuffer&>(ibuffer);
        const StringEncoding       destinationEncoding = StringEncoding::Utf16;

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
#endif
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

bool SC::StringConverter::Internal::readCodePoint(const char*& source, const char* end, bool utf16, uint32_t& codePoint)
{
    const char* start = source;
    if (utf16)
    {
        const size_t remaining = static_cast<size_t>(end - source);
        SC_TRY(remaining >= 2);
        const uint16_t lead = static_cast<uint16_t>(static_cast<uint8_t>(source[0]) |
                                                    (static_cast<uint16_t>(static_cast<uint8_t>(source[1])) << 8));
        // Common's decoder forms source + 2 before checking the end, including after a surrogate lead.
        SC_TRY(lead < 0xD800 or lead > 0xDFFF or remaining >= 4);
        codePoint = StringSpan::advanceUTF16(source, end);
        return codePoint != 0 or lead == 0;
    }
    codePoint = StringSpan::advanceUTF8(source, end);
    return codePoint != 0 or *start == 0;
}

bool SC::StringConverter::Internal::convertFallback(StringSpan text, IGrowableBuffer& buffer,
                                                    StringTermination terminate, StringEncoding destinationEncoding)
{
    const bool   sourceUtf16  = text.getEncoding() == StringEncoding::Utf16;
    const bool   outputUtf16  = destinationEncoding == StringEncoding::Utf16;
    const char*  source       = text.bytesWithoutTerminator();
    const char*  end          = source + text.sizeInBytes();
    const size_t oldSize      = buffer.size();
    const size_t maxSize      = ~static_cast<size_t>(0);
    size_t       encodedBytes = 0;
    const char*  cursor       = source;
    while (cursor != end)
    {
        uint32_t codePoint = 0;
        SC_TRY(readCodePoint(cursor, end, sourceUtf16, codePoint));
        const size_t width = outputUtf16 ? (codePoint <= 0xFFFF ? 2 : 4)
                                         : (codePoint <= 0x7F     ? 1
                                            : codePoint <= 0x7FF  ? 2
                                            : codePoint <= 0xFFFF ? 3
                                                                  : 4);
        SC_TRY(width <= maxSize - encodedBytes);
        encodedBytes += width;
    }
    const size_t nullBytes = terminate == NullTerminate ? (outputUtf16 ? 2 : 1) : 0;
    SC_TRY(encodedBytes <= maxSize - oldSize);
    const size_t payloadSize = oldSize + encodedBytes;
    SC_TRY(nullBytes <= maxSize - payloadSize);

    // A source inside the logical prefix is preserved by growth, but its address may change.
    // Compare integer addresses so unrelated source/destination allocations need no pointer ordering/subtraction.
    const uintptr_t sourceAddress  = reinterpret_cast<uintptr_t>(source);
    const uintptr_t bufferAddress  = reinterpret_cast<uintptr_t>(buffer.data());
    const bool      sourceInPrefix = sourceAddress >= bufferAddress and sourceAddress - bufferAddress <= oldSize and
                                text.sizeInBytes() <= oldSize - static_cast<size_t>(sourceAddress - bufferAddress);
    const size_t sourceOffset = sourceInPrefix ? static_cast<size_t>(sourceAddress - bufferAddress) : 0;
    SC_TRY(buffer.resizeWithoutInitializing(payloadSize + nullBytes));
    if (sourceInPrefix)
    {
        source = buffer.data() + sourceOffset;
        end    = source + text.sizeInBytes();
    }
    cursor       = source;
    char* output = buffer.data() + oldSize;
    while (cursor != end)
    {
        uint32_t codePoint = 0;
        SC_TRY(readCodePoint(cursor, end, sourceUtf16, codePoint)); // Already validated before resizing.
        if (outputUtf16)
        {
            const uint32_t units[] = {codePoint <= 0xFFFF ? codePoint : 0xD800u + ((codePoint - 0x10000u) >> 10),
                                      0xDC00u + ((codePoint - 0x10000u) & 0x3FFu)};
            const size_t count = codePoint <= 0xFFFF ? 1 : 2;
            for (size_t idx = 0; idx < count; ++idx)
            {
                *output++ = static_cast<char>(units[idx] & 0xFFu);
                *output++ = static_cast<char>(units[idx] >> 8);
            }
        }
        else if (codePoint <= 0x7F)
        {
            *output++ = static_cast<char>(codePoint);
        }
        else
        {
            const size_t width = codePoint <= 0x7FF ? 2 : codePoint <= 0xFFFF ? 3 : 4;
            for (size_t idx = width - 1; idx > 0; --idx)
            {
                output[idx] = static_cast<char>(0x80u | (codePoint & 0x3Fu));
                codePoint >>= 6;
            }
            output[0] = static_cast<char>((width == 2 ? 0xC0u : width == 3 ? 0xE0u : 0xF0u) | codePoint);
            output += width;
        }
    }
    if (nullBytes > 0)
        ::memset(output, 0, nullBytes);
    return true;
}
#endif
