// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../../Foundation/StringPath.h"
#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <limits.h> // PATH_MAX
#endif

bool SC::StringPath::assign(StringViewData filePath)
{
#if SC_PLATFORM_WINDOWS
    static_assert(MaxPath >= MAX_PATH, "MAX_PATH");
    if (filePath.getEncoding() == StringEncoding::Utf16)
    {
        if (filePath.sizeInBytes() >= sizeof(path))
        {
            return false;
        }
        ::memcpy(path, filePath.bytesWithoutTerminator(), filePath.sizeInBytes());
        path[filePath.sizeInBytes() / sizeof(wchar_t)] = 0;

        length = filePath.sizeInBytes() / sizeof(wchar_t);
    }
    else
    {
        const int stringLen =
            ::MultiByteToWideChar(CP_UTF8, 0, filePath.bytesWithoutTerminator(),
                                  static_cast<int>(filePath.sizeInBytes()), path, sizeof(path) / sizeof(wchar_t));
        if (stringLen <= 0)
        {
            return false; // Error while converting the string
        }
        path[stringLen] = L'\0'; // Ensure null termination

        length = static_cast<size_t>(stringLen);
    }
    return true;
#else
    static_assert(MaxPath >= PATH_MAX, "PATH_MAX");
    if (filePath.getEncoding() == StringEncoding::Utf16)
    {
        return false;
    }
    if (filePath.sizeInBytes() >= sizeof(path))
    {
        return false;
    }
    ::memcpy(path, filePath.bytesWithoutTerminator(), filePath.sizeInBytes());
    path[filePath.sizeInBytes()] = 0; // Ensure null termination

    length = filePath.sizeInBytes();
    return true;
#endif
}
