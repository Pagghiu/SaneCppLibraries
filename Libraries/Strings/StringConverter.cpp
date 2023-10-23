// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include "../Strings/StringConverter.h"
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
                *encodedText = StringView(text.bytesWithoutTerminator(), text.sizeInBytes(), false, text.getEncoding());
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
                *encodedText = StringView(buffer.data(), buffer.size(), true, text.getEncoding());
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
            *encodedText = StringView(buffer.data(), buffer.size() - destinationCharSize, true, destinationEncoding);
        }
    }
    else if (encodedText)
    {
        *encodedText = StringView(buffer.data(), buffer.size(), false, destinationEncoding);
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
        const bool nullTerminate     = terminate == AddZeroTerminator;
        const int  sourceSizeInBytes = static_cast<int>(text.sizeInBytes());
#if SC_PLATFORM_WINDOWS
        const wchar_t* source = reinterpret_cast<const wchar_t*>(text.bytesWithoutTerminator());
        const int      numChars =
            WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(uint16_t), nullptr, 0, nullptr, 0);
#elif SC_PLATFORM_APPLE
        CFIndex      numChars = 0;
        const UInt8* source   = reinterpret_cast<const UInt8*>(text.bytesWithoutTerminator());
        CFStringRef  tmpStr =
            CFStringCreateWithBytes(kCFAllocatorDefault, source, sourceSizeInBytes, kCFStringEncodingUTF16, false);
        auto    deferDeleteTempString = MakeDeferred([&] { CFRelease(tmpStr); });
        CFRange charRange             = {0, CFStringGetLength(tmpStr)};
        CFStringGetBytes(tmpStr, charRange, kCFStringEncodingUTF8, 0, false, NULL, 0, &numChars);
#else
        // Implement convertEncodingToUTF8 for this platform
        int numChars = -1;
#endif
        if (numChars <= 0)
        {
            return false;
        }
        const auto oldSize = buffer.size();
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
        const bool           nullTerminate       = terminate == AddZeroTerminator;
        const auto           destinationCharSize = StringEncodingGetSize(destinationEncoding);
        const int            sourceSizeInBytes   = static_cast<int>(text.sizeInBytes());
#if SC_PLATFORM_WINDOWS
        const int numWChars =
            MultiByteToWideChar(CP_UTF8, 0, text.bytesWithoutTerminator(), sourceSizeInBytes, nullptr, 0);
#elif SC_PLATFORM_APPLE
        CFIndex      numChars = 0;
        const UInt8* source   = reinterpret_cast<const UInt8*>(text.bytesWithoutTerminator());
        CFStringRef  tmpStr =
            CFStringCreateWithBytes(kCFAllocatorDefault, source, sourceSizeInBytes, kCFStringEncodingUTF8, false);
        auto    deferDeleteTempString = MakeDeferred([&] { CFRelease(tmpStr); });
        CFRange charRange             = {0, CFStringGetLength(tmpStr)};
        CFStringGetBytes(tmpStr, charRange, kCFStringEncodingUTF16, 0, false, NULL, 0, &numChars);
        CFIndex numWChars = numChars / static_cast<CFIndex>(destinationCharSize);
#else
        // Implement convertEncodingToUTF16 for this platform
        int numWChars = -1;
#endif
        if (numWChars <= 0)
        {
            return false;
        }
        const auto oldSize = buffer.size();
        SC_TRY(buffer.resizeWithoutInitializing(oldSize + (static_cast<size_t>(numWChars) + (nullTerminate ? 1 : 0)) *
                                                              destinationCharSize));
#if SC_PLATFORM_WINDOWS
        MultiByteToWideChar(CP_UTF8, 0, text.bytesWithoutTerminator(), static_cast<int>(text.sizeInBytes()),
                            reinterpret_cast<wchar_t*>(buffer.data() + oldSize), numWChars);
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

bool SC::StringConverter::appendNullTerminated(StringView input)
{
    SC_TRY(StringConverter::popNulltermIfExists(data, encoding));
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

bool SC::StringConverter::popNulltermIfExists(Vector<char>& stringData, StringEncoding encoding)
{
    const auto sizeOfZero = StringEncodingGetSize(encoding);
    const auto dataSize   = stringData.size();
    return dataSize >= sizeOfZero ? stringData.resizeWithoutInitializing(dataSize - sizeOfZero) : true;
}

bool SC::StringConverter::pushNullTerm(Vector<char>& stringData, StringEncoding encoding)
{
    return stringData.resize(stringData.size() + StringEncodingGetSize(encoding), 0);
}
