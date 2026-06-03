// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_COMPILER_BUILTINS_DEFINITION_H
#if SC_FOUNDATION_COMPILER_BUILTINS_DEFINITION_H != 1
#error "CompilerBuiltins.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_COMPILER_BUILTINS_DEFINITION_H 1 // Increment to indicate a new version of the file

namespace SC
{
namespace CompilerBuiltins
{
using size_t = decltype(sizeof(0));
[[nodiscard]] inline size_t length(const char* text)
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
[[nodiscard]] inline size_t length(const wchar_t* text)
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

inline void copy(void* destination, const void* source, size_t sizeInBytes)
{
    if (sizeInBytes == 0)
        return;
#if defined(_MSC_VER) && !defined(__clang__)
    char*       dst = static_cast<char*>(destination);
    const char* src = static_cast<const char*>(source);
    for (size_t idx = 0; idx < sizeInBytes; ++idx)
        dst[idx] = src[idx];
#else
    __builtin_memcpy(destination, source, sizeInBytes);
#endif
}

[[nodiscard]] inline int compare(const void* first, const void* second, size_t sizeInBytes)
{
    if (sizeInBytes == 0)
        return 0;
#if defined(_MSC_VER) && !defined(__clang__)
    const unsigned char* lhs = static_cast<const unsigned char*>(first);
    const unsigned char* rhs = static_cast<const unsigned char*>(second);
    for (size_t idx = 0; idx < sizeInBytes; ++idx)
    {
        if (lhs[idx] != rhs[idx])
            return lhs[idx] < rhs[idx] ? -1 : 1;
    }
    return 0;
#else
    return __builtin_memcmp(first, second, sizeInBytes);
#endif
}
} // namespace CompilerBuiltins
} // namespace SC

#endif // SC_FOUNDATION_COMPILER_BUILTINS_DEFINITION_H
