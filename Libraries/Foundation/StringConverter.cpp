// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.

#include "StringConverter.h"
#include "Result.h"
#include "String.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

bool SC::StringConverter::convertEncodingToUTF8(StringView file, Vector<char>& buffer, StringView* encodedText,
                                                NullTermination terminate)
{
    const bool nullTerminate = terminate == AddZeroTerminator;
    const bool forceCopy     = encodedText == nullptr;
    if (file.isEmpty())
    {
        return false;
    }
    if (file.getEncoding() == StringEncoding::Utf8 || file.getEncoding() == StringEncoding::Ascii)
    {
        if (file.isNullTerminated())
        {
            if (forceCopy)
            {
                SC_TRY_IF(buffer.appendCopy(file.bytesIncludingTerminator(), nullTerminate
                                                                                 ? file.sizeInBytesIncludingTerminator()
                                                                                 : file.sizeInBytes()));
            }
            else
            {
                if (nullTerminate)
                {
                    *encodedText = file;
                }
                else
                {
                    *encodedText =
                        StringView(file.bytesWithoutTerminator(), file.sizeInBytes(), false, file.getEncoding());
                }
            }
            return true;
        }
        else
        {
            if (nullTerminate)
            {
                SC_TRY_IF(buffer.reserve(buffer.size() + file.sizeInBytes() + 1));
                SC_TRY_IF(buffer.appendCopy(file.bytesWithoutTerminator(), file.sizeInBytes()));
                if (encodedText)
                {
                    *encodedText = StringView(buffer.data(), buffer.size(), true, StringEncoding::Utf8);
                }
                SC_TRY_IF(buffer.push_back(0)); // null terminator
            }
            else if (encodedText)
            {
                *encodedText = file;
            }
            return true;
        }
    }
    else if (file.getEncoding() == StringEncoding::Utf16)
    {
#if SC_PLATFORM_WINDOWS
        const wchar_t* source            = reinterpret_cast<const wchar_t*>(file.bytesWithoutTerminator());
        const int      sourceSizeInBytes = static_cast<int>(file.sizeInBytes());
        const int      numChars =
            WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(wchar_t), nullptr, 0, nullptr, 0);
        if (numChars <= 0)
        {
            return false;
        }
        const auto oldSize = buffer.size();
        SC_TRY_IF(
            buffer.resizeWithoutInitializing(oldSize + (static_cast<size_t>(numChars) + (nullTerminate ? 1 : 0))));
        WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(wchar_t),
                            reinterpret_cast<char_t*>(buffer.data() + oldSize), numChars, nullptr, 0);
        if (nullTerminate)
        {
            buffer[buffer.size() - 1] = 0;
        }
        if (encodedText)
        {
            if (nullTerminate)
            {
                *encodedText = StringView(buffer.data(), buffer.size() - 1, true, StringEncoding::Utf8);
            }
            else
            {
                *encodedText = StringView(buffer.data(), buffer.size(), false, StringEncoding::Utf8);
            }
        }
        return true;
#endif
    }
    return false;
}

bool SC::StringConverter::convertEncodingToUTF16(StringView file, Vector<char>& buffer, StringView* encodedText,
                                                 NullTermination terminate)
{
    const bool nullTerminate = terminate == AddZeroTerminator;
    const bool forceCopy     = encodedText == nullptr;
    if (file.isEmpty())
    {
        return false;
    }
    if (file.getEncoding() == StringEncoding::Utf16)
    {
        if (file.isNullTerminated())
        {
            if (forceCopy)
            {
                SC_TRY_IF(buffer.appendCopy(file.bytesIncludingTerminator(), nullTerminate
                                                                                 ? file.sizeInBytesIncludingTerminator()
                                                                                 : file.sizeInBytes()));
            }
            else
            {
                if (nullTerminate)
                {
                    *encodedText = file;
                }
                else
                {
                    *encodedText =
                        StringView(file.bytesWithoutTerminator(), file.sizeInBytes(), false, file.getEncoding());
                }
            }
            return true;
        }
        else
        {
            if (nullTerminate)
            {
                SC_TRY_IF(buffer.reserve(buffer.size() + file.sizeInBytes() + sizeof(wchar_t)));
                SC_TRY_IF(buffer.appendCopy(file.bytesWithoutTerminator(), file.sizeInBytes()));
                if (encodedText)
                {
                    *encodedText = StringView(buffer.data(), buffer.size(), true, StringEncoding::Utf16);
                }
                SC_TRY_IF(buffer.resize(buffer.size() + sizeof(wchar_t), 0)); // null terminator
            }
            else if (encodedText)
            {
                *encodedText = file;
            }
            return true;
        }
    }
    else if (file.getEncoding() == StringEncoding::Utf8 || file.getEncoding() == StringEncoding::Ascii)
    {
#if SC_PLATFORM_WINDOWS
        const int numWChars = MultiByteToWideChar(CP_UTF8, 0, file.bytesWithoutTerminator(),
                                                  static_cast<int>(file.sizeInBytes()), nullptr, 0);
        if (numWChars <= 0)
        {
            return false;
        }
        const auto oldSize = buffer.size();
        SC_TRY_IF(buffer.resizeWithoutInitializing(
            oldSize + (static_cast<size_t>(numWChars) + (nullTerminate ? 1 : 0)) * sizeof(wchar_t)));
        MultiByteToWideChar(CP_UTF8, 0, file.bytesWithoutTerminator(), static_cast<int>(file.sizeInBytes()),
                            reinterpret_cast<wchar_t*>(buffer.data() + oldSize), numWChars);
        if (nullTerminate)
        {
            buffer[buffer.size() - 2] = 0; // null terminator
            buffer[buffer.size() - 1] = 0; // null terminator
            if (encodedText)
            {
                *encodedText = StringView(buffer.data(), buffer.size() - sizeof(wchar_t), true, StringEncoding::Utf16);
            }
        }
        else if (encodedText)
        {
            *encodedText = StringView(buffer.data(), buffer.size(), false, StringEncoding::Utf16);
        }
        return true;
#endif
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
    case StringEncoding::Utf32: break;
    }
    return false;
}

void SC::StringConverter::clear() { text.data.clearWithoutInitializing(); }

bool SC::StringConverter::convertNullTerminateFastPath(StringView input, StringView& encodedText)
{
    text.data.clearWithoutInitializing();
    SC_TRY_IF(internalAppend(input, &encodedText));
    return true;
}

bool SC::StringConverter::appendNullTerminated(StringView input)
{
    SC_TRY_IF(text.popNulltermIfExists());
    return internalAppend(input, nullptr);
}

bool SC::StringConverter::setTextLengthInBytesIncludingTerminator(size_t newDataSize)
{
    const auto zeroSize = StringEncodingGetSize(text.getEncoding());
    if (newDataSize >= zeroSize)
    {
        const bool res = text.data.resizeWithoutInitializing(newDataSize - zeroSize);
        return res && text.data.resize(newDataSize, 0); // Adds the null terminator
    }
    return true;
}

/// Appends the input string null terminated
bool SC::StringConverter::internalAppend(StringView input, StringView* encodedText)
{
    return StringConverter::convertEncodingTo(text.getEncoding(), input, text.data, encodedText);
}
