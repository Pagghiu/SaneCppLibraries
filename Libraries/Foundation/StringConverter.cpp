// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "StringConverter.h"
#include "Result.h"

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
#endif

bool SC::StringConverter::toNullTerminatedUTF8(StringView file, Vector<char_t>& buffer,
                                               const char_t** nullTerminatedUTF8)
{
    if (file.isEmpty())
    {
        return false;
    }
    if (file.getEncoding() == StringEncoding::Utf8 || file.getEncoding() == StringEncoding::Ascii)
    {
        if (file.isNullTerminated())
        {
            *nullTerminatedUTF8 = file.getIterator<StringIteratorASCII>().getIt();
            return true;
        }
        else
        {
            SC_TRY_IF(buffer.reserve(file.sizeInBytes() + 1));
            SC_TRY_IF(buffer.appendCopy(file.getIterator<StringIteratorASCII>().getIt(), file.sizeInBytes() + 1));
            *nullTerminatedUTF8 = buffer.data();
            return true;
        }
    }
    else if (file.getEncoding() == StringEncoding::Utf16)
    {
#if SC_PLATFORM_WINDOWS
        const wchar_t* source = reinterpret_cast<const wchar_t*>(file.getIterator<StringIteratorASCII>().getIt());
        const int      sourceSizeInBytes = static_cast<int>(file.sizeInBytes());
        const int      numChars =
            WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(wchar_t), nullptr, 0, nullptr, 0);
        if (numChars <= 0)
        {
            return false;
        }
        SC_TRY_IF(buffer.resizeWithoutInitializing((static_cast<size_t>(numChars) + 1)));
        WideCharToMultiByte(CP_UTF8, 0, source, sourceSizeInBytes / sizeof(wchar_t),
                            reinterpret_cast<char_t*>(buffer.data()), numChars, nullptr, 0);
        buffer[buffer.size() - 1] = 0;
        *nullTerminatedUTF8       = reinterpret_cast<const char_t*>(buffer.data());
        return true;
#else
#endif
    }
    return false;
}

bool SC::StringConverter::toNullTerminatedUTF16(StringView file, Vector<char_t>& buffer,
                                                const wchar_t** nullTerminatedUTF16)
{
    if (file.isEmpty())
    {
        return false;
    }
    if (file.getEncoding() == StringEncoding::Utf16)
    {
        if (file.isNullTerminated())
        {
            *nullTerminatedUTF16 = reinterpret_cast<const wchar_t*>(file.getIterator<StringIteratorASCII>().getIt());
            return true;
        }
        else
        {
            SC_TRY_IF(buffer.reserve(file.sizeInBytes() + 1));
            SC_TRY_IF(
                buffer.appendCopy(reinterpret_cast<const char_t*>(file.getIterator<StringIteratorASCII>().getIt()),
                                  file.sizeInBytes() + 1));
            *nullTerminatedUTF16 = reinterpret_cast<const wchar_t*>(buffer.data());
            return true;
        }
    }
    else if (file.getEncoding() == StringEncoding::Utf8 || file.getEncoding() == StringEncoding::Ascii)
    {
#if SC_PLATFORM_WINDOWS
        const int numWChars = MultiByteToWideChar(CP_UTF8, 0, file.getIterator<StringIteratorASCII>().getIt(),
                                                  static_cast<int>(file.sizeInBytes()), nullptr, 0);
        if (numWChars <= 0)
        {
            return false;
        }
        SC_TRY_IF(buffer.resizeWithoutInitializing((static_cast<size_t>(numWChars) + 1) * sizeof(wchar_t)));
        MultiByteToWideChar(CP_UTF8, 0, file.getIterator<StringIteratorASCII>().getIt(),
                            static_cast<int>(file.sizeInBytes()), reinterpret_cast<wchar_t*>(buffer.data()), numWChars);
        buffer[buffer.size() - 1] = 0;
        *nullTerminatedUTF16      = reinterpret_cast<const wchar_t*>(buffer.data());
        return true;
#else
#endif
    }
    return false;
}
