// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_COMPILER_BUILTINS_DEFINITION_H
#if SC_FOUNDATION_COMPILER_BUILTINS_DEFINITION_H != 2
#error "CompilerBuiltins.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_BUILTINS_DEFINITION_H 2 // Increment to indicate a new version of the file

namespace SC
{
namespace CompilerBuiltins
{
using size_t = decltype(sizeof(0));
[[nodiscard]] constexpr size_t length(const char* text)
{
#if defined(_MSC_VER) && !defined(__clang__)
    const char* it = text;
    while (*it != 0)
        ++it;
    return static_cast<size_t>(it - text);
#else
    return __builtin_strlen(text);
#endif
}

#if defined(_WIN32) || defined(_WIN64)
[[nodiscard]] constexpr size_t length(const wchar_t* text)
{
#if defined(_MSC_VER) && !defined(__clang__)
    const wchar_t* it = text;
    while (*it != 0)
        ++it;
    return static_cast<size_t>(it - text);
#else
    return __builtin_wcslen(text);
#endif
}
#endif
constexpr void copy(char* destination, const char* source, size_t sizeInBytes)
{
#if defined(_MSC_VER) && !defined(__clang__)
    for (size_t idx = 0; idx < sizeInBytes; ++idx)
        destination[idx] = source[idx];
#else
    __builtin_memcpy(destination, source, sizeInBytes);
#endif
}

[[nodiscard]] constexpr int compare(const char* first, const char* second, size_t sizeInBytes)
{
#if defined(_MSC_VER) && !defined(__clang__)
    for (size_t idx = 0; idx < sizeInBytes; ++idx)
    {
        if (first[idx] != second[idx])
            return first[idx] < second[idx] ? -1 : 1;
    }
    return 0;
#else
    return __builtin_memcmp(first, second, sizeInBytes);
#endif
}
} // namespace CompilerBuiltins
} // namespace SC

#endif // SC_FOUNDATION_COMPILER_BUILTINS_DEFINITION_H
