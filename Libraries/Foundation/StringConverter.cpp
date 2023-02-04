// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringConverter.h"
#include "Result.h"

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
#endif

bool SC::StringConverter::toNullTerminatedUTF8(StringView file, Vector<char>& buffer, StringView& encodedText,
                                               bool forceCopy)
{
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
                SC_TRY_IF(buffer.appendCopy(file.bytesIncludingTerminator(), file.sizeInBytesIncludingTerminator()));
                encodedText = StringView(buffer.data(), buffer.size() - 1, true, StringEncoding::Utf8);
            }
            else
            {
                encodedText = file;
            }
            return true;
        }
        else
        {
            SC_TRY_IF(buffer.reserve(buffer.size() + file.sizeInBytes() + 1));
            SC_TRY_IF(buffer.appendCopy(file.bytesWithoutTerminator(), file.sizeInBytes()));
            encodedText = StringView(buffer.data(), buffer.size(), true, StringEncoding::Utf8);
            SC_TRY_IF(buffer.push_back(0)); // null terminator
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
        SC_TRY_IF(buffer.resizeWithoutInitializing(oldSize + (static_cast<size_t>(numChars) + 1)));
        WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(wchar_t),
                            reinterpret_cast<char_t*>(buffer.data() + oldSize), numChars, nullptr, 0);
        buffer[buffer.size() - 1] = 0;
        encodedText               = StringView(buffer.data(), buffer.size() - 1, true, StringEncoding::Utf8);
        return true;
#endif
    }
    return false;
}

bool SC::StringConverter::toNullTerminatedUTF16(StringView file, Vector<char>& buffer, StringView& encodedText,
                                                bool forceCopy)
{
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
                SC_TRY_IF(buffer.appendCopy(file.bytesIncludingTerminator(), file.sizeInBytesIncludingTerminator()));
                encodedText = StringView(buffer.data(), buffer.size() - sizeof(wchar_t), true, StringEncoding::Utf16);
            }
            else
            {
                encodedText = file;
            }
            return true;
        }
        else
        {
            SC_TRY_IF(buffer.reserve(buffer.size() + file.sizeInBytes() + sizeof(wchar_t)));
            SC_TRY_IF(buffer.appendCopy(file.bytesWithoutTerminator(), file.sizeInBytes()));
            encodedText = StringView(buffer.data(), buffer.size(), true, StringEncoding::Utf16);
            SC_TRY_IF(buffer.resize(buffer.size() + sizeof(wchar_t), 0)); // null terminator
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
        SC_TRY_IF(buffer.resizeWithoutInitializing(oldSize + (static_cast<size_t>(numWChars) + 1) * sizeof(wchar_t)));
        MultiByteToWideChar(CP_UTF8, 0, file.bytesWithoutTerminator(), static_cast<int>(file.sizeInBytes()),
                            reinterpret_cast<wchar_t*>(buffer.data() + oldSize), numWChars);
        buffer[buffer.size() - 2] = 0; // null terminator
        buffer[buffer.size() - 1] = 0; // null terminator
        encodedText = StringView(buffer.data(), buffer.size() - sizeof(wchar_t), true, StringEncoding::Utf16);
        return true;
#endif
    }
    return false;
}

bool SC::StringConverter::toNullTerminated(StringEncoding encoding, StringView text, Vector<char>& buffer,
                                           StringView& encodedText, bool forceCopy)
{
    switch (encoding)
    {
    case StringEncoding::Ascii: return toNullTerminatedUTF8(text, buffer, encodedText, forceCopy);
    case StringEncoding::Utf8: return toNullTerminatedUTF8(text, buffer, encodedText, forceCopy);
    case StringEncoding::Utf16: return toNullTerminatedUTF16(text, buffer, encodedText, forceCopy);
    case StringEncoding::Utf32: break;
    }
    return false;
}
